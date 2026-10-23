/*
 * Copyright (c) 2026 Văn Tiến <tien11102004@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Modified 2026-07-29: switched the master's wait-for-response handling from
 * a polling loop to a proper wait-queue style block/wake, and regularized the
 * underlying wait/signal primitives into portable macros so the porting
 * layer can be swapped independently later.
 */

#define LIGHTMODBUS_MASTER_FULL
#define LIGHTMODBUS_IMPL
#define LIGHTMODBUS_DEBUG

#include "lightmodbus/lightmodbus.h"
#include "port.h"
#include "mb.h"
#include "mbport.h"
#include "mbrtu.h"
#include "mbinst.h"
#include "modbus.h"

/* -------------------------------------------------------------------------- */
/* Definitions									*/
/* -------------------------------------------------------------------------- */

#define MB_ADDRESS_BROADCAST 0

/* All state (master, locks, buffers, ...) lives in the ModbusRS485 instance
 * passed in by the caller, see mbinst.h. */

/* -------------------------------------------------------------------------- */
/* LightModbus Callbacks                          */
/* -------------------------------------------------------------------------- */

/**
 * @brief Handles successfully parsed data from a Slave response.
 * If request read more than 1 register, this callback is called each time.
 */
static ModbusError dataCallback(const ModbusMaster *master, const ModbusDataCallbackArgs *args)
{
    ModbusRS485 *mb = (ModbusRS485 *)modbusMasterGetUserPointer(master);
    CHAR typechar = '?';
    switch (args->type)
    {
        case MODBUS_HOLDING_REGISTER: typechar = 'R'; break;
        case MODBUS_INPUT_REGISTER:   typechar = 'I'; break;
        case MODBUS_COIL:             typechar = 'C'; break;
        case MODBUS_DISCRETE_INPUT:   typechar = 'D'; break;
    }
    PR_INFO(
        "F: %03d, T: %c, ID: %03d, VAL: 0x%04x (%d)\n",
        args->function,
        typechar,
        args->index,
        args->value,
        args->value
        );
	/* ESP32 is little-endian but Modbus register values are big-endian on
	 * the wire; ModbusReceive() memcpy's usRspone out as raw bytes, so swap
	 * here to keep that raw output in genuine Modbus (big-endian) byte order
	 * instead of leaking the host's native layout to the caller. */
	mb->usRspone[mb->ubRspLength++] = (USHORT)((args->value << 8) | (args->value >> 8));
    return MODBUS_OK;
}

/**
 * @brief Handles exception codes returned by the Slave.
 */
static ModbusError exceptionCallback(const ModbusMaster *master, uint8_t address, uint8_t function, ModbusExceptionCode code)
{
    PR_INFO(
        "EXCEPTION SLAVE: %03d, F: %03d, CODE: %03d\n",
        address,
        function,
        (int) code
        );
    return MODBUS_OK;
}

/* -------------------------------------------------------------------------- */
/* Internal Helper Functions                         */
/* -------------------------------------------------------------------------- */

/*
 * send_and_wait_logic - Sends a request and waits for completion or timeout.
 * @timeout: Maximum wait time in milliseconds.
 *
 * This function initiates a request and puts the calling thread to sleep until
 * either the requested event completes or the specified timeout expires.
 * It uses a condition variable and mutex to prevent race conditions (lost wakeups).
 *
 * Return: 0 on success, or -1 if the operation timed out.
 */
static eMBErrorCode send_and_wait_logic(ModbusRS485 *mb, INT timeout_ms)
{
    BOOL timed_out = FALSE;

    /* Lock the mutex to protect state evaluation and condition wait */
    LOCK_MUTEX(mb->send_mutex);

    /* Trigger the hardware/event operation */
    xMBPortEventPost(mb, EV_MASTER_SEND_REQUEST);

    /*
     * Check condition before sleeping to prevent race conditions.
     * Loop handles potential spurious wakeups.
     */
    while (mb->master_state != EM_PR && mb->master_state != EM_PER) {
        COND_WAIT_TIMEOUT(mb->send_cond, mb->send_mutex, timeout_ms, timed_out);
        if (timed_out) {
            break; /* Timeout reached */
        }
    }

    /* Release lock after state check/wait completes */
    UNLOCK_MUTEX(mb->send_mutex);

    return timed_out ? MB_ETIMEDOUT : MB_ENOERR;
}

/**
 * @brief Formats the Modbus PDU using LightModbus builder functions.
 */
static INT buildreq(ModbusMaster *master, UCHAR function, USHORT startAddress, USHORT quantity)
{
    ModbusErrorInfo err = MODBUS_NO_ERROR();

    switch (function)
    {
        case 1:
        case 2:
        case 3:
        case 4:
            err = modbusBuildRequest01020304(master, function, startAddress, quantity);
            break;

        case 5:
        case 6:
            err = modbusBuildRequest0506(master, function, startAddress, quantity);
            break;

        default:
            /* Unsupported function codes: don't send a stale request */
            PR_ERR("Unsupported function code %d\n", function);
            return 1;
    }

    if (!modbusIsOk(err))
    {
        PR_ERR("Error building request: %s(%s)\n",
            modbusErrorSourceStr(modbusGetErrorSource(err)),
            modbusErrorStr(modbusGetErrorCode(err)));
		return 1;
    }
	return 0;
}

/**
 * @brief Main State Machine for Modbus Master.
 * Handles Event dispatching and State transitions.
 */
static eMBErrorCode eMBMasterPoll( ModbusRS485 *mb )
{
    CHAR            Poll_log[17] = "MBMasterPoll";
    eMBErrorCode    eStatus = MB_ENOERR;
    eMBEventType    eEvent;

    PR_INFO("%s: Event trigger\n", Poll_log);

    /* Check for events from the porting layer (Timer/Serial) */
    if( xMBPortEventGet( mb, &eEvent ) == TRUE )
    {
        switch ( eEvent )
        {
            case EV_READY:
                break;

            case EV_MASTER_SEND_REQUEST:
                if(mb->master_state != EM_IDLE)
                {
                    PR_ERR("%s: Something go wrong, previous request haven't completely handled\n", Poll_log);
                    eStatus = MB_EINVAL;
                }
                else
                {
                    PR_INFO("%s: Modbus master request send\n", Poll_log);
                    /* Dispatch via RTU Link Layer */
                    eMBRTUSend(
                        mb,
                        mb->ucMBAddress,
                        modbusMasterGetRequest(&mb->master),
                        modbusMasterGetRequestLength(&mb->master)
                    );
                    mb->master_state = EM_WFR;
                }
                break;

            case EV_FRAME_RECEIVED:
                /* Accquire lock to modify shread state safely,
                 * After send_wait_logic puted into wait queue safely*/
                LOCK_MUTEX(mb->send_mutex);
                /* Validation: Only accept frames when waiting for a reply */
				UCHAR ucRcvAddress;
                if(mb->master_state != EM_WFR)
                {
                    PR_ERR("%s: EV_FRAME_RECEIVED: Unexpected frame, master not in WFR state\n", Poll_log);
                    eStatus = MB_EINVAL;
                }
                else
                {
                    PR_INFO("%s: EV_FRAME_RECEIVED: Received frame\n", Poll_log);
                    eStatus = eMBRTUReceive( mb, &ucRcvAddress, mb->pucMBFrame, &mb->usLength );
					if (eStatus != MB_ENOERR)
					{
						/* Garbled/short frame (e.g. bus noise or the master's own
						 * echoed request): ucRcvAddress/usLength were never written
						 * by eMBRTUReceive, so they must not be touched here. Drop
						 * the frame and stay in EM_WFR for the real reply/timeout. */
						PR_ERR("%s: EV_FRAME_RECEIVED: CRC/length error, discarding frame\n", Poll_log);
					}
					else if (ucRcvAddress != mb->ucMBAddress)
					{
						PR_INFO("%s: EV_FRAME_RECEIVED: Invalid address\n", Poll_log);
						mb->master_state = EM_PER; /* Move to Processing Error Reply */
					}
					else
					{
						for(INT i = 0; i < mb->usLength; i++)
						{
							PR_INFO("pucMBFrame[%d]=0x%x\n",i,mb->pucMBFrame[i]);
						}
						mb->master_state = EM_PR; /* Move to Processing Reply */
					}
                }
                COND_SIGNAL(mb->send_cond);
                UNLOCK_MUTEX(mb->send_mutex);
                break;
            default:
                break;
        }
    }
    return eStatus;
}

/* -------------------------------------------------------------------------- */
/* Public API Functions                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Initializes the LightModbus Master stack of the given RS485 instance and
 * stores its configuration, including the UART port, TX/RX pins and the
 * optional RS485 driver-enable pin used later by ModbusStart().
 */
BOOL ModbusInit(ModbusRS485 &inst, HardwareSerial &serial, ULONG baud, ModbusSerialConfig_t rtu_mode, INT tx_pin, INT rx_pin, INT en_pin)
{
    ModbusRS485 *mb = &inst;

    ModbusErrorInfo err = modbusMasterInit(
        &mb->master,
        dataCallback,
        exceptionCallback,
        modbusDefaultAllocator,
        modbusMasterDefaultFunctions,
        modbusMasterDefaultFunctionCount);

    if (!modbusIsOk(err)) return FALSE;

    /* Let the LightModbus callbacks find their way back to this instance */
    modbusMasterSetUserPointer(&mb->master, mb);

    PR_INFO("ModBus: Init Master successfully\n");
    mb->serial = &serial;
    mb->baudrate = baud;
    mb->serial_config = rtu_mode;
    mb->txPin = tx_pin;
    mb->rxPin = rx_pin;
    mb->enPin = en_pin;
    mb->master_state = EM_IDLE;
    return TRUE;
}

/**
 * @brief Initializes the Porting Layer (RTU/Serial/Timer) with the configuration
 * stored in the instance, and starts the stack.
 */
BOOL ModbusStart(ModbusRS485 &inst)
{
    ModbusRS485 *mb = &inst;

    if (mb->serial == NULL) return FALSE;
    eMBErrorCode eStatus = eMBRTUInit(mb);
    if (eStatus != MB_ENOERR) return FALSE;
    xMBPortEventInit(mb);
    eMBRTUStart(mb);
    return TRUE;
}

/**
 * @brief Frees resources and stops the Modbus stack.
 */
void ModbusDestroy(ModbusRS485 &inst)
{
    ModbusRS485 *mb = &inst;

    modbusMasterDestroy(&mb->master);
    eMBRTUStop(mb);
    vMBPortEventDeinit(mb);
    PR_INFO("ModBus: Destroy successfully\n");
}

/**
 * @brief Will be call from tasklet handler.
 */
void ModbusRun(ModbusRS485 &inst)
{
    eMBMasterPoll(&inst);
}

/**
 * @brief Shared implementation of ModbusSend() and ModbusSendReceive().
 * @param out    Optional caller buffer; when not NULL the response bytes are
 *               copied into it *before* the master lock is released.
 * @param outLen In: capacity of out in bytes. Out: bytes copied (0 on failure).
 */
static SendRetType modbus_transfer(ModbusRS485 *mb, UCHAR Address, UCHAR function, USHORT startAddress, USHORT quantity, INT timeout, UCHAR *out, USHORT *outLen)
{
	SendRetType ret_val = ESEND_NOERR;
    eMBErrorCode send_err;
	USHORT out_cap = (out != NULL && outLen != NULL) ? *outLen : 0;
	if (outLen != NULL) *outLen = 0;
	/* 0. Accquire the master lock*/
	LOCK_MUTEX(mb->master_lock);
	mb->ubRspLength = 0; /* Reset response length before start read */
    /* 1. Build the PDU (Application Layer) */
    if(buildreq(&mb->master, function, startAddress, quantity))
	{
		ret_val = ESEND_RQINVAL;
		goto out;
	}

    /* 2. Cache metadata for the upcoming response validation */
    mb->ucMBAddress = Address;

    /* 3. Prepare and wating to recive or timeout*/
	PR_INFO("ModbusSend: waiting\n");
	send_err = send_and_wait_logic(mb, timeout);
    /* 4.  Process continues here after wake-up
	 * Parsing to read input
	 * Application Layer: Parse the received PDU
	 * */
	if (send_err == MB_ETIMEDOUT)
	{
		/* Internal timer woke you up */
		PR_INFO("Modbus Send: Request timeout\n");
		ret_val = ESEND_TIMEOUT;
	}
	else
	{
		/* Wake up when receiving messes from slave */
		PR_INFO("usLength:%d\n",mb->usLength);
		ModbusErrorInfo err = modbusParseResponsePDU(&mb->master,
							  mb->ucMBAddress,
							  modbusMasterGetRequest(&mb->master),
							  modbusMasterGetRequestLength(&mb->master),
							  mb->pucMBFrame,
							  mb->usLength);

		if (!modbusIsOk(err))
		{
			PR_ERR("Error parsing request: %s(%s)\n",
				modbusErrorSourceStr(modbusGetErrorSource(err)),
				modbusErrorStr(modbusGetErrorCode(err)));
			ret_val = ESEND_RPINVAL;
		}
		else
		{
			PR_INFO("Response parsing successfully\n");
			ret_val = ESEND_NOERR;

			/* Copy the response out while still holding the master lock, so
			 * another thread's request can't overwrite usRspone first. */
			if (out != NULL && outLen != NULL)
			{
				USHORT bytes = (USHORT)(mb->ubRspLength * sizeof(*mb->usRspone));
				if (bytes > out_cap)
				{
					ret_val = ESEND_BUFSMALL;
				}
				else
				{
					memcpy(out, (char*)mb->usRspone, (size_t)bytes);
					*outLen = bytes;
				}
			}
		}
	}
out:
	/* 7. Relase the master's lock */
	mb->master_state = EM_IDLE;
	UNLOCK_MUTEX(mb->master_lock);
	return ret_val;
}

/**
 * @brief Higher-level API to initiate a Modbus request.
 * The response stays in the instance; fetch it with ModbusReceive().
 */
SendRetType ModbusSend(ModbusRS485 &inst, UCHAR Address, UCHAR function, USHORT startAddress, USHORT quantity, INT timeout)
{
	return modbus_transfer(&inst, Address, function, startAddress, quantity, timeout, NULL, NULL);
}

/**
 * @brief Sends a request and copies the response into the caller's buffer in
 * one atomic step (the master lock is held until the copy is done).
 */
SendRetType ModbusSendReceive(ModbusRS485 &inst, UCHAR Address, UCHAR function, USHORT startAddress, USHORT quantity, INT timeout, UCHAR *buffer, USHORT *length)
{
	if (buffer == NULL || length == NULL) return ESEND_RQINVAL;
	return modbus_transfer(&inst, Address, function, startAddress, quantity, timeout, buffer, length);
}

void ModbusReceive (ModbusRS485 &inst, UCHAR *buffer, USHORT *length)
{
    ModbusRS485 *mb = &inst;
    /* Lenght should be in number of bytes */
	PR_INFO("Read %d registers from user\n",mb->ubRspLength);
	*length = mb->ubRspLength * sizeof(*mb->usRspone);
	memcpy(buffer,(char*)mb->usRspone, (size_t)*length);
}

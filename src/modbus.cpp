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
#include "modbus.h"

/* -------------------------------------------------------------------------- */
/* Definitions									*/
/* -------------------------------------------------------------------------- */

#define MB_ADDRESS_BROADCAST 0
#define MAX_PDU_SIZE         253
#define MAX_VALUE_RESPONE	 10					/* We assume that no senosor hold value in more than 10 registers */

/* -------------------------------------------------------------------------- */
/* Global Variables                              */
/* -------------------------------------------------------------------------- */
/* Lock for master, only thread can use in time,
 * migth be many threads from user try to read on modbus bus
 * */
DEFINE_MUTEX(master_lock);
DEFINE_MUTEX(send_mutex);
DEFINE_COND(send_cond);

static ModbusMaster    master;
static ModbusErrorInfo err = MODBUS_NO_ERROR();

static UCHAR    ucMBAddress;    /* Target Slave Address for current transaction */
static ULONG    baudrate;       /* Stored baudrate for RTU initialization */

static USHORT					usRspone[MAX_VALUE_RESPONE];	/* Buffer holding return respone for device (using on data call back) */
static UINT8					ubRspLength;						/* Length of return respone */
static UCHAR					pucMBFrame[MAX_PDU_SIZE];		/* Buffer holding value from RTU Layer before put it into step parsing */
static USHORT					usLength;

static eMasterType     master_state = EM_IDLE;

static eMBErrorCode    eStatus = MB_ENOERR;
static eMBEventType    eEvent;

/* -------------------------------------------------------------------------- */
/* LightModbus Callbacks                          */
/* -------------------------------------------------------------------------- */

/**
 * @brief Handles successfully parsed data from a Slave response.
 * If request read more than 1 register, this callback is called each time.
 */
static ModbusError dataCallback(const ModbusMaster *master, const ModbusDataCallbackArgs *args)
{
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
	usRspone[ubRspLength++] = args->value;
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
static eMBErrorCode send_and_wait_logic(INT timeout_ms)
{
    BOOL timed_out = FALSE;

    /* Lock the mutex to protect state evaluation and condition wait */
    LOCK_MUTEX(send_mutex);

    /* Trigger the hardware/event operation */
    xMBPortEventPost(EV_MASTER_SEND_REQUEST);

    /*
     * Check condition before sleeping to prevent race conditions.
     * Loop handles potential spurious wakeups.
     */
    while (master_state != EM_PR && master_state != EM_PER) {
        COND_WAIT_TIMEOUT(send_cond, send_mutex, timeout_ms, timed_out);
        if (timed_out) {
            break; /* Timeout reached */
        }
    }

    /* Release lock after state check/wait completes */
    UNLOCK_MUTEX(send_mutex);

    return timed_out ? MB_ETIMEDOUT : MB_ENOERR;
}

/**
 * @brief Formats the Modbus PDU using LightModbus builder functions.
 */
static INT buildreq(ModbusMaster *master, UCHAR function, USHORT startAddress, USHORT quantity)
{
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
            /* Unsupported function codes */
            break;
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
static eMBErrorCode eMBMasterPoll( void )
{
    CHAR            Poll_log[17] = "MBMasterPoll";

    PR_INFO("%s: Event trigger\n", Poll_log);

    /* Check for events from the porting layer (Timer/Serial) */
    if( xMBPortEventGet( &eEvent ) == TRUE )
    {
        switch ( eEvent )
        {
            case EV_READY:
                break;

            case EV_MASTER_SEND_REQUEST:
                if(master_state != EM_IDLE)
                {
                    PR_ERR("%s: Something go wrong, previous request haven't completely handled\n", Poll_log);
                    eStatus = MB_EINVAL;
                }
                else
                {
                    PR_INFO("%s: Modbus master request send\n", Poll_log);
                    /* Dispatch via RTU Link Layer */
                    eMBRTUSend(
                        ucMBAddress,
                        modbusMasterGetRequest(&master),
                        modbusMasterGetRequestLength(&master)
                    ); 
                    master_state = EM_WFR;
                }
                break;

            case EV_FRAME_RECEIVED:
                /* Accquire lock to modify shread state safely,
                 * After send_wait_logic puted into wait queue safely*/
                LOCK_MUTEX(send_mutex); 
                /* Validation: Only accept frames when waiting for a reply */
				UCHAR ucRcvAddress;
                if(master_state != EM_WFR)
                {
                    PR_ERR("%s: EV_FRAME_RECEIVED: Unexpected frame, master not in WFR state\n", Poll_log);
                    eStatus = MB_EINVAL;
                }
                else
                {
                    PR_INFO("%s: EV_FRAME_RECEIVED: Received frame\n", Poll_log);
                    eStatus = eMBRTUReceive( &ucRcvAddress, pucMBFrame, &usLength );
					if (ucRcvAddress != ucMBAddress)
					{
						PR_INFO("%s: EV_FRAME_RECEIVED: Invalid address\n", Poll_log);
						master_state = EM_PER; /* Move to Processing Error Reply */
					}
					else
					{
						for(INT i = 0; i < usLength; i++)
						{
							PR_INFO("pucMBFrame[%d]=0x%x\n",i,pucMBFrame[i]);
						}
						if( eStatus == MB_ENOERR )
						{
							master_state = EM_PR; /* Move to Processing Reply */
						}
					}
                }
                COND_SIGNAL(send_cond);
                UNLOCK_MUTEX(send_mutex);
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
 * @brief Initializes the LightModbus Master stack and stores configuration.
 */
BOOL ModbusInit(ULONG baud)
{
    ModbusErrorInfo err = modbusMasterInit(
        &master,
        dataCallback,
        exceptionCallback,
        modbusDefaultAllocator,
        modbusMasterDefaultFunctions,
        modbusMasterDefaultFunctionCount);

    if (!modbusIsOk(err)) return FALSE;
    
    PR_INFO("ModBus: Init Master successfully\n");
    baudrate = baud;
    return TRUE;
}

/**
 * @brief Initializes the Porting Layer (RTU/Serial/Timer) and starts the stack.
 */
BOOL ModbusStart(void)
{
    eMBErrorCode eStatus = eMBRTUInit(baudrate);
    if (eStatus != MB_ENOERR) return FALSE;
    xMBPortEventInit();
    eMBRTUStart();
    return TRUE;
}

/**
 * @brief Frees resources and stops the Modbus stack.
 */
void ModbusDestroy(void)
{
    modbusMasterDestroy(&master);
    eMBRTUStop();
    vMBPortEventDeinit();
    PR_INFO("ModBus: Destroy successfully\n");
}

/**
 * @brief Will be call from tasklet handler. 
 */
void ModbusRun(void)
{
    eMBMasterPoll();
}

/**
 * @brief Higher-level API to initiate a Modbus request.
 */
SendRetType ModbusSend(UCHAR Address, UCHAR function, USHORT startAddress, USHORT quantity, INT timeout)
{
	SendRetType ret_val = ESEND_NOERR;
	/* 0. Accquire the master lock*/
	LOCK_MUTEX(master_lock);
	ubRspLength = 0; /* Reset response length before start read */
    /* 1. Build the PDU (Application Layer) */
    if(buildreq(&master, function, startAddress, quantity))
	{
		ret_val = ESEND_RQINVAL;	
		goto out;
	}
    
    /* 2. Cache metadata for the upcoming response validation */
    ucMBAddress = Address;

    /* 3. Prepare and wating to recive or timeout*/
	PR_INFO("ModbusSend: waiting\n");
	eMBErrorCode send_err = send_and_wait_logic(timeout);
    /* 4.  Process continues here after wake-up
	 * Parsing to read input 	
	 * Application Layer: Parse the received PDU
	 * */
	if (send_err == MB_ETIMEDOUT)
	{
		/* Internal timer woke you up */
		PR_INFO("Modbus Send: Request timeout\n");	
		ret_val = ESEND_TIMEOUT;
        goto out;
	}
	else
	{
		/* Wake up when receiving messes from slave */
		PR_INFO("usLength:%d\n",usLength);
		err = modbusParseResponsePDU(&master,
							  ucMBAddress,
							  modbusMasterGetRequest(&master),
							  modbusMasterGetRequestLength(&master),
							  pucMBFrame,
							  usLength);

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
		}
	}
out:
	/* 7. Relase the master's lock */
	master_state = EM_IDLE;
	UNLOCK_MUTEX(master_lock);
	return ret_val;
}

void ModbusReceive (UCHAR *buffer, USHORT *length)
{
	PR_INFO("Read %d registers from user\n",ubRspLength);
	*length = ubRspLength * sizeof(*usRspone);
	uiPortMemcpy(buffer,(char*)usRspone,*length);	
}

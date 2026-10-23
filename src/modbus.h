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
 */

/*
 * Public API for the Modbus master. This is the only header application
 * code (or a platform file that needs to call back into the master, e.g.
 * the event worker task) should include — everything under src/ is a
 * private implementation detail of this library.
 */

#ifndef _MODBUS_H
#define _MODBUS_H

#include "port.h"
#include "mbinst.h"

/* Result of a ModbusSend() call. */
typedef enum
{
    ESEND_NOERR = 0,         /*!< Request sent and response parsed successfully. */
    ESEND_RQINVAL,            /*!< Failed to build the request PDU. */
    ESEND_TIMEOUT,            /*!< No response received before timeout. */
    ESEND_RPINVAL,            /*!< Failed to parse the response PDU. */
    ESEND_BUFSMALL,           /*!< Caller's buffer too small for the response (ModbusSendReceive). */
    ESEND_EXCEPTION           /*!< Slave answered with an exception; its 1-byte code is the response data (length 1). */
} SendRetType;

/* Human-readable string for a SendRetType, e.g. for logging a ModbusSend() result. */
static inline const CHAR *ModbusErrToStr(SendRetType ret)
{
    switch (ret)
    {
        case ESEND_NOERR:   return "OK";
        case ESEND_RQINVAL: return "Invalid request";
        case ESEND_TIMEOUT: return "Timeout";
        case ESEND_RPINVAL: return "Invalid response";
        case ESEND_BUFSMALL: return "Buffer too small";
        case ESEND_EXCEPTION: return "Slave exception";
        default:            return "Unknown error";
    }
}

/* Every call below works on the ModbusRS485 instance the caller passes in, so
 * several RS485 buses can run at the same time, one instance each. The instance
 * must outlive the stack (declare it global/static, not on a short-lived stack). */

/* Initializes the LightModbus master stack and stores baud rate/UART TX-RX pins in mb for ModbusStart().
 * enPin is the RS485 transceiver's driver-enable (DE/RE) pin; pass -1 (default) if the
 * transceiver auto-directions itself and no pin needs to be toggled.
 * serial is the HardwareSerial port to run Modbus on (e.g. Serial1, Serial2). */
BOOL        ModbusInit(ModbusRS485 &mb, HardwareSerial &serial, ULONG baud, ModbusSerialConfig_t rtu_mode, INT txPin, INT rxPin, INT enPin = -1);

/* Initializes the porting layer (RTU/serial/timer) and starts the stack. */
BOOL        ModbusStart(ModbusRS485 &mb);

/* Frees resources and stops the Modbus stack. */
void        ModbusDestroy(ModbusRS485 &mb);

/* Runs one iteration of the master's event/poll loop; called from the event worker task. */
void        ModbusRun(ModbusRS485 &mb);

/* Builds and sends a request, blocking until a response or timeout. */
SendRetType ModbusSend(ModbusRS485 &mb, UCHAR Address, UCHAR function, USHORT startAddress, USHORT quantity, INT timeout);

/* Copies the last successfully parsed response out to the caller. The response lives in mb,
 * so with several threads sharing a bus another request may overwrite it between
 * ModbusSend() and ModbusReceive(); use ModbusSendReceive() in that case. */
void ModbusReceive (ModbusRS485 &mb, UCHAR *buffer, USHORT *length);

/* Builds and sends a request and copies the response into buffer before releasing the bus,
 * so no other thread can interleave between the send and the copy.
 * length: in = capacity of buffer in bytes, out = bytes copied (0 on failure).
 * Returns ESEND_BUFSMALL (nothing copied) if the response doesn't fit. */
SendRetType ModbusSendReceive(ModbusRS485 &mb, UCHAR Address, UCHAR function, USHORT startAddress, USHORT quantity, INT timeout, UCHAR *buffer, USHORT *length);

#endif /* _MODBUS_H */

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

#ifdef __cplusplus
PR_BEGIN_EXTERN_C
#endif

/* Result of a ModbusSend() call. */
typedef enum
{
    ESEND_NOERR = 0,         /*!< Request sent and response parsed successfully. */
    ESEND_RQINVAL,            /*!< Failed to build the request PDU. */
    ESEND_TIMEOUT,            /*!< No response received before timeout. */
    ESEND_RPINVAL             /*!< Failed to parse the response PDU. */
} SendRetType;

/* Initializes the LightModbus master stack and stores baud rate for ModbusStart(). */
BOOL        ModbusInit(ULONG baud);

/* Initializes the porting layer (RTU/serial/timer) and starts the stack. */
BOOL        ModbusStart(void);

/* Frees resources and stops the Modbus stack. */
void        ModbusDestroy(void);

/* Runs one iteration of the master's event/poll loop; called from the event worker task. */
void        ModbusRun(void);

/* Builds and sends a request, blocking until a response or timeout. */
SendRetType ModbusSend(UCHAR Address, UCHAR function, USHORT startAddress, USHORT quantity, INT timeout);

/* Copies the last successfully parsed response out to the caller. */
void        ModbusReceive(UCHAR *buffer, USHORT *length);

#ifdef __cplusplus
PR_END_EXTERN_C
#endif
#endif /* _MODBUS_H */

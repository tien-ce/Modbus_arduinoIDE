/* 
 * FreeModbus Libary: A portable Modbus implementation for Modbus ASCII/RTU.
 * Copyright (c) 2006-2018 Christian Walter <cwalter@embedded-solutions.at>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Modifications:
 * Copyright (c) 2026 Văn Tiến  <tien11102004@gmail.com>
 * - Add some new events for master.
 * - Change to read multiple bytes instead of one.
 * - Remove unsed callback
 */

#ifndef _MB_PORT_H
#define _MB_PORT_H

#include "port.h"

/* ----------------------- Type definitions ---------------------------------*/


typedef enum
{
	EM_IDLE,					/*!< Master is idle. */
	EM_WFR,						/*!< Master waiting for reply. */
	EM_PR,						/*!< Master processing reply. */
	EM_PER,						/*!< Master processing error reply. */
} eMasterType;

typedef enum
{
    EV_READY,                   /*!< Startup finished. */
    EV_FRAME_RECEIVED,          /*!< Frame received. */
	EV_BYTE_RECEIVED,			/*!< Byte received.	*/
    EV_EXECUTE,                 /*!< Execute function. */
    EV_FRAME_SENT,              /*!< Frame sent. */
	EV_MASTER_SEND_REQUEST,		/*!< Master request to send.*/
} eMBEventType;

/*! \ingroup modbus
 * \brief Parity used for characters in serial mode.
 *
 * The parity which should be applied to the characters sent over the serial
 * link. Please note that this values are actually passed to the porting
 * layer and therefore not all parity modes might be available.
 */
typedef enum
{
    MB_PAR_NONE,                /*!< No parity. */
    MB_PAR_ODD,                 /*!< Odd parity. */
    MB_PAR_EVEN                 /*!< Even parity. */
} eMBParity;

/* ----------------------- Supporting functions -----------------------------*/
BOOL            xMBPortEventInit( ModbusRS485 *mb );

BOOL            xMBPortEventPost( ModbusRS485 *mb, eMBEventType eEvent );

BOOL            xMBPortEventGet( ModbusRS485 *mb, /*@out@ */ eMBEventType * eEvent );

void			vMBPortEventDeinit( ModbusRS485 *mb );

/* ----------------------- Timers functions ---------------------------------*/
BOOL            xMBPortTimersInit( ModbusRS485 *mb, USHORT usTimeOut50us );

void            vMBPortTimersStart( ModbusRS485 *mb );

void            vMBPortTimersCancel( ModbusRS485 *mb );

/* ------------------------ Uart functions ----------------------------------*/
void            vMBPortUartRead ( ModbusRS485 *mb, UCHAR *buffer, INT *pLength);

void            vMBPortUartWrite ( ModbusRS485 *mb, UCHAR *buffer, USHORT usLength);

/* Opens mb->serial using the baud/config/pins stored in mb. */
BOOL            xMBPortUartInit( ModbusRS485 *mb );

#endif
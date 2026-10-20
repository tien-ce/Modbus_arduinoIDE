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
 */

#ifndef _MB_RTU_H
#define _MB_RTU_H
#include "mb.h"

/* Configuration (serial port, baud, pins...) is read from mb, filled in by ModbusInit(). */
eMBErrorCode    eMBRTUInit( ModbusRS485 *mb );
void            eMBRTUStart( ModbusRS485 *mb );
void            eMBRTUStop( ModbusRS485 *mb );
eMBErrorCode    eMBRTUReceive( ModbusRS485 *mb, UCHAR * pucRcvAddress, UCHAR * pucFrame, USHORT * pusLength );
eMBErrorCode 	eMBRTUSend( ModbusRS485 *mb, UCHAR ucSlaveAddress, const UCHAR * pucFrame, USHORT usLength );
void            xMBRTUReceiveFSM( ModbusRS485 *mb );
BOOL            xMBRTUTransmitSuccess( ModbusRS485 *mb );
BOOL            xMBRTUTimerT35Expired( ModbusRS485 *mb );

#endif

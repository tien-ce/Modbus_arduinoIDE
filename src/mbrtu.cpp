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
 *	Copyright (c) 2026 Văn Tiến <tien11102004@gmail.com>
 *	- Oriented to run only on link layer (RTU)
 *	- Seperated memory allocator for different layers
 */


/* ----------------------- Platform includes --------------------------------*/
#include "port.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mbport.h"
#include "mbcrc.h"
#include "mb.h"
#include "mbrtu.h"
#include "mbinst.h"
/* ----------------------- Defines ------------------------------------------*/
#define MB_SER_PDU_SIZE_MIN     4       /*!< Minimum size of a Modbus RTU frame. */
#define MB_SER_PDU_SIZE_CRC     2       /*!< Size of CRC field in PDU. */
#define MB_SER_PDU_ADDR_OFF     0       /*!< Offset of slave address in Ser-PDU. */
#define MB_SER_PDU_PDU_OFF      1       /*!< Offset of Modbus-PDU in Ser-PDU. */
#define MAX_PER_RECEIVE			MB_SER_PDU_SIZE_MAX		/*!< Maximun size of a number bytes reading per time. */

/* All RTU state lives in the ModbusRS485 instance (see mbinst.h). */

/* ----------------------- Start implementation -----------------------------*/
eMBErrorCode
eMBRTUInit(ModbusRS485 *mb)
{
    eMBErrorCode    eStatus = MB_ENOERR;
    ULONG           usTimerT35_50us;
    ULONG           ulBaudRate = mb->baudrate;

    ENTER_CRITICAL_SECTION(  );
	/* If baudrate > 19200 then we should use the fixed timer values
	 * t35 = 1750us. Otherwise t35 must be 3.5 times the character time.
	 */
	if( ulBaudRate > 19200 )
	{
		usTimerT35_50us = 35;       /* 1800us. */
	}
	else
	{
		/* The timer reload value for a character is given by:
		 *
		 * ChTimeValue = Ticks_per_1s / ( Baudrate / 11 )
		 *             = 11 * Ticks_per_1s / Baudrate
		 *             = 220000 / Baudrate
		 * The reload for t3.5 is 1.5 times this value and similary
		 * for t3.5.
		 */
		usTimerT35_50us = ( 7UL * 220000UL ) / ( 2UL * ulBaudRate );
	}
	if( xMBPortTimersInit( mb, ( USHORT ) usTimerT35_50us ) != TRUE )
	{
		eStatus = MB_EPORTERR;
	}

    if (xMBPortUartInit (mb) != TRUE )
    {
        eStatus = MB_EPORTERR;
    }

    EXIT_CRITICAL_SECTION(  );

    return eStatus;
}

void
eMBRTUStart( ModbusRS485 *mb )
{
    ENTER_CRITICAL_SECTION(  );
    /* Initially the receiver is in the state STATE_RX_INIT. we start
     * the timer and if no character is received within t3.5 we change
     * to STATE_RX_IDLE. This makes sure that we delay startup of the
     * modbus protocol stack until the bus is free.
     */
	mb->eRcvState = STATE_RX_INIT;
	vMBPortTimersStart( mb );
	EXIT_CRITICAL_SECTION(  );
}

void
eMBRTUStop( ModbusRS485 *mb )
{
	ENTER_CRITICAL_SECTION(  );
	vMBPortTimersCancel( mb );
	vMBPortUartDeinit( mb );
	PR_INFO("ModBusRTU: Destroy sucessfully\n");
	EXIT_CRITICAL_SECTION(  );
}

eMBErrorCode
eMBRTUReceive( ModbusRS485 *mb, UCHAR * pucRcvAddress, UCHAR * pucFrame, USHORT * pusLength )
{
	eMBErrorCode    eStatus = MB_ENOERR;

	ENTER_CRITICAL_SECTION(  );

	if( ( mb->usRcvBufferPos >= MB_SER_PDU_SIZE_MIN )
		&& ( usMBCRC16( ( UCHAR * ) mb->ucRTUReBuf, mb->usRcvBufferPos ) == 0 ) )
	{
		/* Save the address field. All frames are passed to the upper layed
		 * and the decision if a frame is used is done there.
		*/
		*pucRcvAddress = mb->ucRTUReBuf[MB_SER_PDU_ADDR_OFF];
		 /* Total length of Modbus-PDU is Modbus-Serial-Line-PDU minus
				 * size of address field and CRC checksum.
		 */
		*pusLength = ( USHORT )( mb->usRcvBufferPos - MB_SER_PDU_PDU_OFF - MB_SER_PDU_SIZE_CRC );
		 /* Copy from the start of the Modbus PDU to the caller. */
		 /* PDU frame is star form 1, not 0 (RTU) padding */
		uiPortMemcpy(pucFrame,(mb->ucRTUReBuf+1),*(pusLength));
	}
	else
	{
		PR_DEBUG(" usRcvBufferPos = %d\n",mb->usRcvBufferPos);
		PR_DEBUG( "ucRTUReBuf[0] = 0x%x\n", mb->ucRTUReBuf[0]);
		PR_DEBUG( "ucRTUReBuf[1] = 0x%x\n", mb->ucRTUReBuf[1]);
		PR_DEBUG( "ucRTUReBuf[2] = 0x%x\n", mb->ucRTUReBuf[2]);
		PR_DEBUG( "ucRTUReBuf[3] = 0x%x\n", mb->ucRTUReBuf[3]);
		PR_DEBUG( "ucRTUReBuf[4] = 0x%x\n", mb->ucRTUReBuf[4]);
		PR_DEBUG( "ucRTUReBuf[5] = 0x%x\n", mb->ucRTUReBuf[5]);
		eStatus = MB_EIO;
	}

	EXIT_CRITICAL_SECTION(  );
	return eStatus;
}

eMBErrorCode
eMBRTUSend( ModbusRS485 *mb, UCHAR ucSlaveAddress, const UCHAR * pucFrame, USHORT usLength )
{
	eMBErrorCode    eStatus = MB_ENOERR;
	USHORT          usCRC16;

	ENTER_CRITICAL_SECTION(  );
	/* First add the slave address for first byte of RTU Frame */
	mb->ucRTUSndBuf[MB_SER_PDU_ADDR_OFF] = ucSlaveAddress;
	mb->usSndBufferCount = 1;

	/* Copy usLength bytes from PDU (Protocol data unit) */
	UINT8 num_cpy = uiPortMemcpy((mb->ucRTUSndBuf+mb->usSndBufferCount),pucFrame, usLength);
	if (num_cpy == usLength)
	{
		mb->usSndBufferCount += usLength;
	}
	else
	{
		eStatus = MB_EPORTERR;
	}

	/* Calculate CRC16 checksum for Modbus-Serial-Line-PDU. */
	usCRC16 = usMBCRC16( ( UCHAR * ) mb->ucRTUSndBuf, mb->usSndBufferCount );

	/* In frame description of MODBUSRTU, the CRC field includes 2 bytes oredered [CRC low, CRC high]
	 * This oposite with PDU format (usually big endian format)
	 * */
	/* Activate the transmitter. */
	PR_INFO("Modbus request send\n");
	mb->ucRTUSndBuf[mb->usSndBufferCount++] = ( UCHAR )( usCRC16 & 0xFF );
	mb->ucRTUSndBuf[mb->usSndBufferCount++] = ( UCHAR )( usCRC16 >> 8 );
	for (INT i = 0; i < mb->usSndBufferCount; i++)
	{
		PR_INFO("Sndbuf[%d]: 0x%x\n",i,mb->ucRTUSndBuf[i]);
	}
	vMBPortUartWrite(mb, mb->ucRTUSndBuf, mb->usSndBufferCount);
    EXIT_CRITICAL_SECTION(  );
    return eStatus;
}

/* Run on ISR context, not print or sleep */

void
xMBRTUReceiveFSM( ModbusRS485 *mb )
{
	UCHAR			ucRTUTmpBuf[MAX_PER_RECEIVE];
	INT				usRTUReceiveCount;
    /* Read the characters. */
    vMBPortUartRead(mb, ucRTUTmpBuf,&usRTUReceiveCount);

    switch ( mb->eRcvState )
    {
        /* If we have received a character in the init state we have to
         * wait until the frame is finished.
         */
    case STATE_RX_INIT:
        vMBPortTimersStart( mb );
        break;

        /* In the error state we wait until all characters in the
         * damaged frame are transmitted.
         */
    case STATE_RX_ERROR:
        vMBPortTimersStart( mb );
        break;

        /* In the idle state we wait for a new character. If a character
         * is received the t1.5 and t3.5 timers are started and the
         * receiver is in the state STATE_RX_RECEIVCE.
         */
    case STATE_RX_IDLE:
        mb->usRcvBufferPos = 0;
		uiPortMemcpy(mb->ucRTUReBuf+mb->usRcvBufferPos,ucRTUTmpBuf,usRTUReceiveCount);
		mb->usRcvBufferPos += usRTUReceiveCount;
        mb->eRcvState = STATE_RX_RCV;
        /* Enable t3.5 timers. */
        vMBPortTimersStart( mb );
        break;

        /* We are currently receiving a frame. Reset the timer after
         * every character received. If more than the maximum possible
         * number of bytes in a modbus frame is received the frame is
         * ignored.
         */
    case STATE_RX_RCV:
        if( mb->usRcvBufferPos + usRTUReceiveCount < MB_SER_PDU_SIZE_MAX )
        {
			uiPortMemcpy(mb->ucRTUReBuf+mb->usRcvBufferPos,ucRTUTmpBuf,usRTUReceiveCount);
			mb->usRcvBufferPos += usRTUReceiveCount;
			mb->eRcvState = STATE_RX_RCV;
			/* Reset t3.5 timer so it measures silence since the LAST
			 * character, not just the first one of this frame. */
			vMBPortTimersStart( mb );
        }
        else
        {
            mb->eRcvState = STATE_RX_ERROR;
        }
        break;
    }
}

BOOL
xMBRTUTransmitSuccess ( ModbusRS485 *mb )
{
	/* Trigger successfull sent even */
	xMBPortEventPost(mb, EV_FRAME_SENT);
	return TRUE;
}

BOOL
xMBRTUTimerT35Expired( ModbusRS485 *mb )
{
    BOOL            xNeedPoll = FALSE;

    switch ( mb->eRcvState )
    {
        /* Timer t35 expired. Startup phase is finished. */
    case STATE_RX_INIT:
		xNeedPoll = xMBPortEventPost( mb, EV_READY );
        break;
        /* A frame was received and t35 expired. Notify the listener that
         * a new frame was received. */
    case STATE_RX_RCV:
        xNeedPoll = xMBPortEventPost( mb, EV_FRAME_RECEIVED );
        break;

        /* An error occured while receiving the frame. */
    case STATE_RX_ERROR:
        break;

        /* Function called in an illegal state. */
    default:
		break;
    }
	mb->eRcvState = STATE_RX_IDLE;
    return xNeedPoll;
}

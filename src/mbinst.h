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
 * ModbusRS485: every piece of state the library used to keep in file-scope
 * globals (application layer, RTU link layer, UART/timer/event porting layer).
 * The caller owns one instance per RS485 bus and passes it to every API call,
 * so several buses can run side by side.
 */

#ifndef _MBINST_H
#define _MBINST_H

#if !defined(ESP32)
#error "ModbusController only supports ESP32 (esp_timer + FreeRTOS + HardwareSerial::onReceive)"
#endif

#ifndef LIGHTMODBUS_MASTER_FULL
#define LIGHTMODBUS_MASTER_FULL
#endif
#include "lightmodbus/lightmodbus.h"

#include "port.h"
#include "mb.h"
#include "mbport.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>

#define MB_SER_PDU_SIZE_MAX     256     /*!< Maximum size of a Modbus RTU frame. */
#define MAX_PDU_SIZE            253
#define MAX_VALUE_RESPONE       10      /* We assume that no senosor hold value in more than 10 registers */

typedef enum
{
    STATE_RX_INIT,              /*!< Receiver is in initial state. */
    STATE_RX_IDLE,              /*!< Receiver is in idle state. */
    STATE_RX_RCV,               /*!< Frame is beeing received. */
    STATE_RX_ERROR              /*!< If the frame is invalid. */
} eMBRcvState;

typedef enum
{
    STATE_TX_IDLE,              /*!< Transmitter is in idle state. */
    STATE_TX_XMIT               /*!< Transmitter is in transfer state. */
} eMBSndState;

struct ModbusRS485
{
    /* ---- Configuration (set by ModbusInit) ---- */
    HardwareSerial         *serial = NULL;  /* UART port this bus runs on */
    ULONG                   baudrate = 0;
    ModbusSerialConfig_t    serial_config = RTU_8N1;
    INT                     txPin = -1;
    INT                     rxPin = -1;
    INT                     enPin = -1;     /* RS485 driver-enable (DE/RE) pin, -1 = unused */

    /* ---- Application layer (modbus.cpp) ---- */
    /* Lock for master, only one thread can use the bus at a time,
     * might be many threads from user try to read on modbus bus */
    pthread_mutex_t         master_lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t         send_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t          send_cond = PTHREAD_COND_INITIALIZER;

    ModbusMaster            master;
    UCHAR                   ucMBAddress = 0;    /* Target Slave Address for current transaction */
    USHORT                  usRspone[MAX_VALUE_RESPONE];    /* Return respone for device (filled by data callback) */
    UINT8                   ubRspLength = 0;                /* Length of return respone */
    UCHAR                   pucMBFrame[MAX_PDU_SIZE];       /* Value from RTU Layer before it goes into step parsing */
    USHORT                  usLength = 0;
    eMasterType             master_state = EM_IDLE;

    /* ---- RTU link layer (mbrtu.cpp) ---- */
    volatile eMBSndState    eSndState = STATE_TX_IDLE;
    volatile eMBRcvState    eRcvState = STATE_RX_INIT;
    UCHAR                   ucRTUReBuf[MB_SER_PDU_SIZE_MAX];
    UCHAR                   ucRTUSndBuf[MB_SER_PDU_SIZE_MAX];
    volatile USHORT         usSndBufferCount = 0;
    volatile USHORT         usSndBufferPos = 0;
    volatile USHORT         usRcvBufferPos = 0;

    /* ---- Timer porting layer (port_timer.cpp) ---- */
    esp_timer_handle_t      hrtimer = NULL;     /* High-resolution software timer handle */
    uint64_t                active_interval_us = 0;

    /* ---- Event porting layer (port_event.cpp) ---- */
    volatile eMBEventType   eQueuedEvent = EV_READY;
    volatile bool           xEventInQueue = false;
    TaskHandle_t            xMBTaskHandle = NULL;
};

#endif /* _MBINST_H */

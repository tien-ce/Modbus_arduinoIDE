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

#include "mbport.h"
#include "modbus.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/* Static variables */
static volatile eMBEventType eQueuedEvent;
static volatile bool xEventInQueue = false;
static TaskHandle_t xMBTaskHandle = NULL;

/**
 * @brief FreeRTOS task acting as the deferred execution handler ("Bottom Half").
 */
static void prvModbusWorkerTask(void *pvParameters)
{
    (void)pvParameters;
    ModbusRun();
}

bool xMBPortEventInit(void)
{
    xEventInQueue = false;

    /* Create worker task if it doesn't exist yet */
    if (xMBTaskHandle == NULL) {
        BaseType_t xReturned = xTaskCreate(
            prvModbusWorkerTask,   /* Task function */
            "MB_Worker",           /* Name */
            2048,                  /* Stack size in words */
            NULL,                  /* Task input parameter */
            configMAX_PRIORITIES - 1, /* High priority for event processing */
            &xMBTaskHandle         /* Task handle */
        );

        if (xReturned != pdPASS) {
            return false;
        }
    }

    Serial.println("Modbus Event: Init");
    return true;
}

bool xMBPortEventPost(eMBEventType eEvent)
{
    /* Critical section protection for variables */

    xEventInQueue = true;
    eQueuedEvent = eEvent;

    /* Wake up the worker task from an ISR or a normal thread */
    if (xMBTaskHandle != NULL) {
        if (xPortInIsrContext()) {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            vTaskNotifyGiveFromISR(xMBTaskHandle, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        } else {
            xTaskNotifyGive(xMBTaskHandle);
        }
    }

    return true;
}

bool xMBPortEventGet(eMBEventType *eEvent)
{
    bool xEventHappened = false;
    /* If event occur, (set by post) */
    if (xEventInQueue) {
        /* copy event type */
        *eEvent = eQueuedEvent;
        /* Clear flag to reset single entry queue */
        xEventInQueue = false;
        /* Set xEvent hapened = True */
        xEventHappened = true;
    }


    return xEventHappened;
}

/**
 * @brief Clean up task handle on de-initialization
 */
void vMBPortEventDeinit(void)
{
    if (xMBTaskHandle != NULL) {
        vTaskDelete(xMBTaskHandle);
        xMBTaskHandle = NULL;
    }
    Serial.println("Modbus Event: Destroy");
}

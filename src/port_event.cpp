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
#include "mbinst.h"
#include "modbus.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/* Worker Task Function; pvParameters is the owning ModbusRS485 */
static void prvModbusWorkerTask(void *pvParameters)
{
    ModbusRS485 *mb = (ModbusRS485 *)pvParameters;

    /* FreeRTOS tasks must run in an infinite loop */
    for (;;) 
    {
        /* 
         * Block indefinitely (portMAX_DELAY) until another task or ISR 
         * sends a notification to xMBTaskHandle via xTaskNotifyGive() 
         * or vTaskNotifyGiveFromISR().
         */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        /* Process pending Modbus events */
        ModbusRun(*mb);
    }
}

bool xMBPortEventInit(ModbusRS485 *mb)
{
    mb->xEventInQueue = false;

    /* 
     * Check if the worker task has already been created.
     * If xMBTaskHandle is NULL, the task does not exist yet, so we create it.
     * If xMBTaskHandle is NOT NULL, we skip creation to avoid duplicate tasks.
     */
    if (mb->xMBTaskHandle == NULL) {
        BaseType_t xReturned = xTaskCreate(
            prvModbusWorkerTask,      /* Task function entry point */
            "MB_Worker",              /* Text name for debugging */
            2048,                     /* Stack size in words */
            mb,                       /* Task input parameter */
            configMAX_PRIORITIES - 1, /* Highest user priority */
            &mb->xMBTaskHandle        /* Stores the created task's handle here */
        );

        /* Return false if memory allocation failed for the task */
        if (xReturned != pdPASS) {
            return false;
        }
    }

    Serial.println("Modbus Event: Init");
    return true;
}

bool xMBPortEventPost(ModbusRS485 *mb, eMBEventType eEvent)
{
    /* Critical section protection for state variables */
    mb->xEventInQueue = true;
    mb->eQueuedEvent = eEvent;

    /* 
     * Ensure the task actually exists before trying to send a notification.
     * If xMBTaskHandle is NULL, xTaskCreate hasn't run or failed.
     */
    if (mb->xMBTaskHandle != NULL) {
        
        /* Check if we are currently inside an Interrupt Service Routine (ISR) */
        if (xPortInIsrContext()) {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            
            /* Send notification safely from ISR context */
            vTaskNotifyGiveFromISR(mb->xMBTaskHandle, &xHigherPriorityTaskWoken);
            
            /* Force context switch if a higher priority task was woken */
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        } else {
            /* Send notification from a standard task/thread context */
            xTaskNotifyGive(mb->xMBTaskHandle);
        }
    }

    return true;
}

bool xMBPortEventGet(ModbusRS485 *mb, eMBEventType *eEvent)
{
    bool xEventHappened = false;
    /* If event occur, (set by post) */
    if (mb->xEventInQueue) {
        /* copy event type */
        *eEvent = mb->eQueuedEvent;
        /* Clear flag to reset single entry queue */
        mb->xEventInQueue = false;
        /* Set xEvent hapened = True */
        xEventHappened = true;
    }
    return xEventHappened;
}

/**
 * @brief Clean up task handle on de-initialization
 */
void vMBPortEventDeinit(ModbusRS485 *mb)
{
    if (mb->xMBTaskHandle != NULL) {
        vTaskDelete(mb->xMBTaskHandle);
        mb->xMBTaskHandle = NULL;
    }
    Serial.println("Modbus Event: Destroy");
}

/*
 * port_timer.c
 *
 *  Created on: Apr 9, 2026
 *      Author: vantien
 *
 * License: MIT
 * Copyright (c) 2026 Văn Tiến
 */

#include "port.h"
#include "mbport.h"
#include "mbrtu.h"
#include "mbinst.h"
#include <Arduino.h>

#include <esp_timer.h>

/* Timer callback handler triggered upon expiration; arg is the owning ModbusRS485 */
static void IRAM_ATTR test_hrtimer_handler(void *arg) {
    /* Modbus RTU Logic: T35 silence expired */
    xMBRTUTimerT35Expired((ModbusRS485 *)arg);
}

bool xMBPortTimersInit(ModbusRS485 *mb, uint16_t usTim1Timerout50us) {
    uint32_t timeout_us = (uint32_t)usTim1Timerout50us * 50;
    mb->active_interval_us = timeout_us;

    /* Define timer configuration */
    const esp_timer_create_args_t timer_args = {
        .callback = &test_hrtimer_handler,
        .arg = mb,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "modbus_t35_timer"
    };

    /* Delete existing timer if re-initialized */
    if (mb->hrtimer != NULL) {
        esp_timer_stop(mb->hrtimer);
        esp_timer_delete(mb->hrtimer);
        mb->hrtimer = NULL;
    }

    /* Create high-resolution timer */
    if (esp_timer_create(&timer_args, &mb->hrtimer) != ESP_OK) {
        return false;
    }

    Serial.printf("Modbus Timer: Initialized with %lu us interval.\n", timeout_us);
    return true;
}

void vMBPortTimersStart(ModbusRS485 *mb) {
    if (mb->hrtimer != NULL) {
        /* esp_timer_start_once() errors (ESP_ERR_INVALID_STATE) if the timer
         * is already running instead of restarting it — stop it first so
         * every call (e.g. one per received byte) truly resets the t3.5
         * countdown instead of silently failing once it's already armed. */
        esp_timer_stop(mb->hrtimer);
        esp_timer_start_once(mb->hrtimer, mb->active_interval_us);
    }
}

void vMBPortTimersCancel(ModbusRS485 *mb) {
    if (mb->hrtimer != NULL) {
        /* Stop the timer synchronously */
        esp_timer_stop(mb->hrtimer);
    }
}

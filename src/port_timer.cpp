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
#include <Arduino.h>

#if defined(ESP32)
#include <esp_timer.h>

/* High-resolution software timer handle */
static esp_timer_handle_t my_hrtimer = NULL;
static uint64_t active_interval_us = 0;

/* Timer callback handler triggered upon expiration */
static void IRAM_ATTR test_hrtimer_handler(void *arg) {
    /* Modbus RTU Logic: T35 silence expired */
    xMBRTUTimerT35Expired();
}

bool xMBPortTimersInit(uint16_t usTim1Timerout50us) {
    uint32_t timeout_us = (uint32_t)usTim1Timerout50us * 50;
    active_interval_us = timeout_us;

    /* Define timer configuration */
    const esp_timer_create_args_t timer_args = {
        .callback = &test_hrtimer_handler,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "modbus_t35_timer"
    };

    /* Delete existing timer if re-initialized */
    if (my_hrtimer != NULL) {
        esp_timer_stop(my_hrtimer);
        esp_timer_delete(my_hrtimer);
    }

    /* Create high-resolution timer */
    if (esp_timer_create(&timer_args, &my_hrtimer) != ESP_OK) {
        return false;
    }

    Serial.printf("Modbus Timer: Initialized with %lu us interval.\n", timeout_us);
    return true;
}

void vMBPortTimersStart(void) {
    if (my_hrtimer != NULL) {
        /* esp_timer_start_once() errors (ESP_ERR_INVALID_STATE) if the timer
         * is already running instead of restarting it — stop it first so
         * every call (e.g. one per received byte) truly resets the t3.5
         * countdown instead of silently failing once it's already armed. */
        esp_timer_stop(my_hrtimer);
        esp_timer_start_once(my_hrtimer, active_interval_us);
    }
}

void vMBPortTimersCancel(void) {
    if (my_hrtimer != NULL) {
        /* Stop the timer synchronously */
        esp_timer_stop(my_hrtimer);
    }
}

#else

/* Portable Software Fallback for AVR / Standard Arduino Boards */
static unsigned long active_interval_us = 0;
static unsigned long timer_start_us = 0;
static bool timer_running = false;

bool xMBPortTimersInit(uint16_t usTim1Timerout50us) {
    active_interval_us = (uint32_t)usTim1Timerout50us * 50;
    timer_running = false;
    return true;
}

void vMBPortTimersStart(void) {
    timer_start_us = micros();
    timer_running = true;
}

void vMBPortTimersCancel(void) {
    timer_running = false;
}

/* Call this function inside loop() for non-ESP32 boards */
void vMBPortTimersPoll(void) {
    if (timer_running && (micros() - timer_start_us >= active_interval_us)) {
        timer_running = false;
        xMBRTUTimerT35Expired();
    }
}

#endif

# ESP32 Software Architecture & Layer Dependency Guide

This document explains how low-level POSIX APIs (`pthread`), the Arduino framework, ESP-IDF, and FreeRTOS interact on the ESP32 platform.

---

## 1. Architecture Overview (What Builds On What)

```
+-------------------------------------------------------------------+
|               Application Code (Modbus / C / C++)                 |
+---------------------------------+---------------------------------+
|     Arduino Framework Core      |     POSIX pthread Wrapper       |
|   (setup, loop, Serial, etc.)   |  (<pthread.h>, mutexes, conds)  |
+---------------------------------+---------------------------------+
|            ESP-IDF (Espressif IoT Development Framework)           |
+-------------------------------------------------------------------+
|               FreeRTOS Kernel (Dual-Core Task Scheduler)           |
+-------------------------------------------------------------------+
|               Hardware Platform (ESP32 / ESP32-S3)                |
+-------------------------------------------------------------------+
```

---

## 2. Detailed Architecture Diagram

Below is the detailed visual representation of the layer hierarchy:

![ESP32 Architecture Diagram](esp32_architecture.svg)

---

## 3. Layer Breakdown

### Layer 1: Hardware Platform
* **Components:** ESP32, ESP32-S3, ESP32-C3 microcontrollers (Xtensa LX7 / RISC-V cores).
* **Role:** Provides physical memory, hardware interrupts, timers, and execution units.

### Layer 2: FreeRTOS Kernel
* **Built On Top Of:** Raw hardware instructions.
* **Role:** Manages preemptive multi-tasking, context switching, semaphore primitives, and core pinning.
* **Core APIs:** `xTaskCreatePinnedToCore()`, `xSemaphoreTake()`, `xSemaphoreGive()`.

### Layer 3: ESP-IDF
* **Built On Top Of:** FreeRTOS Kernel.
* **Role:** Primary system development framework created by Espressif. It wraps FreeRTOS with hardware drivers (UART, SPI, Wi-Fi) and provides a C standard library implementation (`newlib`) that includes the `pthread` wrapper layer.

### Layer 4: POSIX pthread & Arduino Abstractions
* **POSIX pthread Wrapper:**
  * **Built On Top Of:** ESP-IDF / FreeRTOS primitives.
  * **Role:** Translates standard POSIX calls directly to FreeRTOS calls.
  * **Example:**
    $$\text{pthread\_mutex\_lock()} \longrightarrow \text{xSemaphoreTake()}$$
    $$\text{pthread\_mutex\_unlock()} \longrightarrow \text{xSemaphoreGive()}$$
* **Arduino Framework Core:**
  * **Built On Top Of:** ESP-IDF component APIs.
  * **Role:** Simplifies hardware interaction via `setup()`, `loop()`, and `Serial.printf()`. Runs as a single FreeRTOS task on Core 1 by default.

### Layer 5: Application Code
* **Built On Top Of:** Arduino APIs + POSIX C/C++ libraries.
* **Role:** Your user application, state machines, protocol stacks (e.g., Modbus RTU/TCP), and domain logic.

---

## 4. API Mapping Table

| POSIX API (`<pthread.h>`) | ESP-IDF Implementation | FreeRTOS Underneath |
| :--- | :--- | :--- |
| `pthread_create()` | `esp_pthread_init()` | `xTaskCreatePinnedToCore()` |
| `pthread_mutex_lock()` | `pthread_mutex_lock()` | `xSemaphoreTake()` |
| `pthread_mutex_unlock()` | `pthread_mutex_unlock()` | `xSemaphoreGive()` |
| `pthread_cond_wait()` | `pthread_cond_wait()` | `xTaskNotifyWait()` / Event Groups |

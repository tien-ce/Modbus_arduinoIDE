/*
 * Copyright (C) <2026> <vantien>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef PORT_H_
#define PORT_H_

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

/**
 * LOGGING
 * Kernel code used pr_info()/pr_err(); ported to Arduino's Serial output.
 *
 * PR_* can be called from more than one FreeRTOS task (e.g. the MB_Worker
 * task and the application task), and the underlying printf()/Serial.printf()
 * is not re-entrant, so two interleaved calls tear each other's output apart.
 * print_mutex() is a plain (non-static) inline function: every translation
 * unit that includes this header gets the identical definition, and the
 * linker merges them into one, so the function-local static mutex below is a
 * single, shared instance for the whole program rather than a private copy
 * per .cpp file.
 * */
inline pthread_mutex_t *print_mutex(void)
{
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    return &mutex;
}

#define PR_LOCKED(stmt) \
    do { \
        pthread_mutex_lock(print_mutex()); \
        stmt; \
        pthread_mutex_unlock(print_mutex()); \
    } while (0)

#ifdef ARDUINO
#include <Arduino.h>
#define PR_OUT(fmt, ...)    Serial.printf(fmt, ##__VA_ARGS__)
#else
#include <stdio.h>
#define PR_OUT(fmt, ...)    printf(fmt, ##__VA_ARGS__)
#endif

#define PR_ERR(fmt, ...)    PR_LOCKED(PR_OUT("[ERROR] " fmt, ##__VA_ARGS__))

/*
 * PR_INFO/PR_DEBUG are the high-volume, per-byte/per-poll traces used while
 * bringing up the RTU link — a library consumer including this in their own
 * project shouldn't get that on their Serial console by default. They compile
 * to nothing unless the consumer opts in with -D MODBUS_DEBUG_LOG (or
 * #define MODBUS_DEBUG_LOG before including this header).
 */
#ifdef MODBUS_DEBUG_LOG
#define PR_INFO(fmt, ...)   PR_LOCKED(PR_OUT("[INFO] " fmt, ##__VA_ARGS__))
#define PR_DEBUG(fmt, ...)  PR_LOCKED(PR_OUT("[DEBUG] " fmt, ##__VA_ARGS__))
#else
#define PR_INFO(fmt, ...)   do { } while (0)
#define PR_DEBUG(fmt, ...)  do { } while (0)
#endif

/* Use C-standard inline attribute */
#define INLINE                      inline
#define DEFINE_MUTEX(name)      pthread_mutex_t name = PTHREAD_MUTEX_INITIALIZER
#define LOCK_MUTEX(name)        pthread_mutex_lock(&(name))
#define UNLOCK_MUTEX(name)      pthread_mutex_unlock(&(name))
#define DEFINE_COND(name)       pthread_cond_t name = PTHREAD_COND_INITIALIZER;

/*
 * COND_WAIT_TIMEOUT - wait on cond/mutex until signaled or timeout_ms elapses.
 * mutex must already be held. Sets timed_out_flag (a BOOL lvalue) to TRUE if
 * the wait timed out, FALSE if it was signaled.
 */
#define COND_WAIT_TIMEOUT(cond, mutex, timeout_ms, timed_out_flag) \
    do { \
        struct timespec ts; \
        clock_gettime(CLOCK_REALTIME, &ts); \
        ts.tv_sec  += (timeout_ms) / 1000; \
        ts.tv_nsec += ((timeout_ms) % 1000) * 1000000L; \
        if (ts.tv_nsec >= 1000000000L) { \
            ts.tv_sec  += 1; \
            ts.tv_nsec -= 1000000000L; \
        } \
        (timed_out_flag) = (pthread_cond_timedwait(&(cond), &(mutex), &ts) == ETIMEDOUT) ? TRUE : FALSE; \
    } while (0)

#define COND_SIGNAL(cond)  pthread_cond_signal(&(cond))

/* -------------------------------------------------------------------------- */
/* UART Configuration Defines (Data - Parity - Stop)                          */
/* -------------------------------------------------------------------------- */
typedef enum {
    RTU_8N1,
    RTU_8N2,
    RTU_8E1,
    RTU_8E2,
    RTU_8O1,
    RTU_8O2,
    RTU_7N1,
    RTU_7N2,
    RTU_7E1,
    RTU_7E2,
    RTU_7O1,
    RTU_7O2
} ModbusSerialConfig_t;
/**
 * CRITICAL SECTION  
 * */
//#define ENTER_CRITICAL_SECTION(flag)   local_irq_save(flag)
//#define EXIT_CRITICAL_SECTION(flag)    local_irq_restore(flag)

#define ENTER_CRITICAL_SECTION()   
#define EXIT_CRITICAL_SECTION()    
/* Boolean definitions - Kernel uses bool, true, false from <linux/types.h> */
typedef int				INT;
typedef bool            BOOL;
typedef unsigned char   UCHAR;
typedef char            CHAR;
typedef uint16_t        USHORT;
typedef int16_t         SHORT;
typedef uint32_t        ULONG;
typedef int32_t         LONG;
typedef uint8_t         UINT8;

#ifndef TRUE
#define TRUE            true
#endif

#ifndef FALSE
#define FALSE           false
#endif

/* One instance per RS485 bus, owned by the caller (full definition in mbinst.h) */
struct ModbusRS485;

/* Port function using kernel memcpy */
static inline USHORT uiPortMemcpy(UCHAR *des, const UCHAR *src, USHORT length)
{
    memcpy(des, src, length);
    return length;
}

#endif /* PORT_H_ */

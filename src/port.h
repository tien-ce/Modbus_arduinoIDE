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
 * */
#ifdef ARDUINO
#include <Arduino.h>
#define PR_INFO(fmt, ...)   Serial.printf("[INFO] " fmt, ##__VA_ARGS__)
#define PR_ERR(fmt, ...)    Serial.printf("[ERROR] " fmt, ##__VA_ARGS__)
#else
#include <stdio.h>
#define PR_INFO(fmt, ...)   printf("[INFO] " fmt, ##__VA_ARGS__)
#define PR_ERR(fmt, ...)    printf("[ERROR] " fmt, ##__VA_ARGS__)
#endif

/* Use C-standard inline attribute */
#define INLINE                      inline
#define DEFINE_MUTEX(name)      pthread_mutex_t name = PTHREAD_MUTEX_INITIALIZER
#define LOCK_MUTEX(name)   pthread_mutex_lock(&(name))
#define UNLOCK_MUTEX(name) pthread_mutex_unlock(&(name))
#define DEFINE_COND(name)  pthread_cond_t name = PTHREAD_COND_INITIALIZER;

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
/* Extern C is usually unnecessary in kernel unless strictly linking with C++ */
#ifdef __cplusplus
#define PR_BEGIN_EXTERN_C           extern "C" {
#define PR_END_EXTERN_C             }
#else
#define PR_BEGIN_EXTERN_C
#define PR_END_EXTERN_C
#endif

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

/* Port function using kernel memcpy */
static inline UINT8 uiPortMemcpy(UCHAR *des, const UCHAR *src, UINT8 length)
{
    memcpy(des, src, length);
    return length;
}

#endif /* PORT_H_ */

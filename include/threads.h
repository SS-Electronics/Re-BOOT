/* 
File:        threads.h
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      Thread  
Info:        Multithreading related operations and APIs           
Dependency:  None

This file is part of Re-BOOT Project.

Re-BOOT is free software: you can redistribute it and/or 
modify it under the terms of the GNU General Public License 
as published by the Free Software Foundation, either version 
3 of the License, or (at your option) any later version.

Re-BOOT is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of 
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
GNU General Public License for more details.

You should have received a copy of the GNU General Public License 
along with FreeRTOS-KERNEL. If not, see <https://www.gnu.org/licenses/>.
*/

/**
 * @file thread.h
 * @brief Cross-platform thread abstraction layer for Linux and Windows.
 *
 * This library provides a minimal portable threading interface that
 * wraps POSIX threads on Linux and Win32 threads on Windows.
 *
 * Supported features:
 * - Thread creation and joining
 * - Thread exit
 * - Mutex locking primitives
 *
 * The goal is to provide a small and dependency-free threading API
 * usable in embedded and system-level applications.
 *
 * @author
 * Subhajit Roy
 *
 */


#ifndef __THREADS_H__
#define __THREADS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "app_types.h"

#if defined(__linux__)

#include <pthread.h>

#elif defined(_WIN32) || defined(_WIN64)

#include <windows.h>

#endif

/**
 * @brief Thread function prototype
 */
typedef void* (*thread_func_t)(void *arg);


/**
 * @brief Thread object
 */
typedef struct
{
#if defined(__linux__)
    pthread_t handle;
#elif defined(_WIN32) || defined(_WIN64)
    HANDLE handle;
    DWORD id;
#endif
} thread_t;


/**
 * @brief Mutex object
 */
typedef struct
{
#if defined(__linux__)
    pthread_mutex_t handle;
#elif defined(_WIN32) || defined(_WIN64)
    CRITICAL_SECTION handle;
#endif
} mutex_t;


/**
 * @brief Condition variable
 */
typedef struct
{
#if defined(__linux__)
    pthread_cond_t handle;
#elif defined(_WIN32) || defined(_WIN64)
    CONDITION_VARIABLE handle;
#endif
} cond_t;




/**
 * @brief Create a thread
 *
 * @param thread Thread object
 * @param func Thread entry function
 * @param arg Argument passed to thread
 *
 * @return 0 on success
 */
int thread_create(thread_t *thread, thread_func_t func, void *arg);


/**
 * @brief Join a thread
 */
int thread_join(thread_t *thread);


/**
 * @brief Detach thread
 */
int thread_detach(thread_t *thread);


/**
 * @brief Sleep for milliseconds
 */
void thread_sleep(uint32_t ms);


/**
 * @brief Yield processor
 */
void thread_yield(void);


/**
 * @brief Initialize mutex
 */
int mutex_init(mutex_t *m);


/**
 * @brief Lock mutex
 */
void mutex_lock(mutex_t *m);


/**
 * @brief Unlock mutex
 */
void mutex_unlock(mutex_t *m);


/**
 * @brief Destroy mutex
 */
void mutex_destroy(mutex_t *m);


/**
 * @brief Initialize condition variable
 */
int cond_init(cond_t *c);


/**
 * @brief Wait on condition
 */
void cond_wait(cond_t *c, mutex_t *m);


/**
 * @brief Signal one waiting thread
 */
void cond_signal(cond_t *c);


/**
 * @brief Broadcast to all threads
 */
void cond_broadcast(cond_t *c);


/**
 * @brief Destroy condition variable
 */
void cond_destroy(cond_t *c);


#ifdef __cplusplus
}
#endif

#endif /* __THREADS_H__ */
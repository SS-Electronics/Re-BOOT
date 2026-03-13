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

/**
 * @file threads.h
 * @brief Cross-platform threading abstraction layer.
 *
 * This module provides a minimal portable threading API that works on
 * both Linux (POSIX pthread) and Windows (Win32 threads).
 *
 * It hides platform specific details and exposes a small set of APIs
 * suitable for system software, embedded frameworks, and infrastructure
 * components.
 *
 * Features:
 * - Thread creation and management
 * - Mutex synchronization
 * - Condition variables
 * - Thread sleep and yield
 *
 * Supported Platforms:
 * - Linux (POSIX Threads)
 * - Windows (Win32 API)
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
 * @defgroup thread Thread API
 * @brief Portable thread abstraction
 * @{
 */


/**
 * @brief Thread entry function prototype.
 *
 * Every thread must start execution from a function matching this
 * prototype.
 *
 * @param arg Pointer passed during thread creation.
 *
 * @return Optional return value returned when thread exits.
 */
typedef void* (*thread_func_t)(void *arg);


/**
 * @brief Thread object.
 *
 * This structure represents a thread handle in a platform-independent
 * format.
 */
typedef struct
{
#if defined(__linux__)
    pthread_t handle;        /**< POSIX thread handle */
#elif defined(_WIN32) || defined(_WIN64)
    HANDLE handle;           /**< Windows thread handle */
    DWORD id;                /**< Windows thread identifier */
#endif
} thread_t;


/**
 * @brief Mutex object.
 *
 * Provides mutual exclusion between multiple threads.
 */
typedef struct
{
#if defined(__linux__)
    pthread_mutex_t handle;  /**< POSIX mutex */
#elif defined(_WIN32) || defined(_WIN64)
    CRITICAL_SECTION handle; /**< Windows critical section */
#endif
} mutex_t;


/**
 * @brief Condition variable object.
 *
 * Used for thread signaling and synchronization between producer
 * and consumer threads.
 */
typedef struct
{
#if defined(__linux__)
    pthread_cond_t handle;       /**< POSIX condition variable */
#elif defined(_WIN32) || defined(_WIN64)
    CONDITION_VARIABLE handle;   /**< Windows condition variable */
#endif
} cond_t;


/**
 * @brief Create a new thread.
 *
 * This function creates a new thread and begins execution at the
 * specified thread entry function.
 *
 * @param thread Pointer to thread object.
 * @param func Thread entry function.
 * @param arg Argument passed to the thread function.
 *
 * @return
 * - 0 : Success
 * - Negative value : Failure
 */
int thread_create(thread_t *thread, thread_func_t func, void *arg);


/**
 * @brief Wait for a thread to terminate.
 *
 * This function blocks the calling thread until the specified thread
 * exits.
 *
 * @param thread Pointer to thread object.
 *
 * @return
 * - 0 : Success
 * - Negative value : Failure
 */
int thread_join(thread_t *thread);


/**
 * @brief Detach a thread.
 *
 * After detaching, system resources used by the thread are released
 * automatically when the thread terminates.
 *
 * @param thread Pointer to thread object.
 *
 * @return
 * - 0 : Success
 * - Negative value : Failure
 */
int thread_detach(thread_t *thread);


/**
 * @brief Sleep the current thread.
 *
 * Suspends execution of the current thread for a specified number
 * of milliseconds.
 *
 * @param ms Duration in milliseconds.
 */
void thread_sleep(uint32_t ms);


/**
 * @brief Yield the processor.
 *
 * Causes the current thread to voluntarily yield execution so that
 * another runnable thread may run.
 */
void thread_yield(void);


/**
 * @brief Initialize a mutex.
 *
 * @param m Pointer to mutex object.
 *
 * @return
 * - 0 : Success
 * - Negative value : Failure
 */
int mutex_init(mutex_t *m);


/**
 * @brief Lock a mutex.
 *
 * Blocks the calling thread until the mutex becomes available.
 *
 * @param m Pointer to mutex object.
 */
void mutex_lock(mutex_t *m);


/**
 * @brief Unlock a mutex.
 *
 * Releases a previously locked mutex.
 *
 * @param m Pointer to mutex object.
 */
void mutex_unlock(mutex_t *m);


/**
 * @brief Destroy a mutex.
 *
 * Releases resources associated with the mutex.
 *
 * @param m Pointer to mutex object.
 */
void mutex_destroy(mutex_t *m);


/**
 * @brief Initialize a condition variable.
 *
 * @param c Pointer to condition variable object.
 *
 * @return
 * - 0 : Success
 * - Negative value : Failure
 */
int cond_init(cond_t *c);


/**
 * @brief Wait on a condition variable.
 *
 * Atomically releases the mutex and blocks the thread until the
 * condition variable is signaled.
 *
 * @param c Pointer to condition variable.
 * @param m Pointer to associated mutex.
 */
void cond_wait(cond_t *c, mutex_t *m);


/**
 * @brief Signal a waiting thread.
 *
 * Wakes one thread waiting on the condition variable.
 *
 * @param c Pointer to condition variable.
 */
void cond_signal(cond_t *c);


/**
 * @brief Wake all waiting threads.
 *
 * Wakes every thread waiting on the condition variable.
 *
 * @param c Pointer to condition variable.
 */
void cond_broadcast(cond_t *c);


/**
 * @brief Destroy condition variable.
 *
 * Releases resources associated with the condition variable.
 *
 * @param c Pointer to condition variable.
 */
void cond_destroy(cond_t *c);


/** @} */ /* end of thread group */

#ifdef __cplusplus
}
#endif

#endif /* __THREADS_H__ */
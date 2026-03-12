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




/**
 * @defgroup thread_api Thread API
 * @{
 */
/**
 * @brief Platform detection : Linux
 */
#if defined(__linux__)

#include <pthread.h>

/**
 * @brief Thread handle type
 */
typedef pthread_t thread_handle_t;

/**
 * @brief Thread ID type
 */
typedef pthread_t thread_id_t;

#endif

/**
 * @brief Platform detection : Windows
 */
#if defined(_WIN32) || defined(_WIN64)

#include <windows.h>

/**
 * @brief Thread handle type
 */
typedef HANDLE thread_handle_t;

/**
 * @brief Thread ID type
 */
typedef DWORD thread_id_t;

#endif



/**
 * @brief Thread function prototype
 *
 * All threads must use this signature.
 *
 * @param arg User supplied argument
 * @return Thread return value
 */

typedef void *(*thread_func_t)(void *);



/**
 * @brief Create a new thread
 *
 * @param handle Pointer to thread handle
 * @param func Thread entry function
 * @param arg Argument passed to thread
 *
 * @return 0 on success, negative on failure
 */
int thread_create(thread_handle_t *handle,
                  thread_func_t func,
                  void *arg);


/**
 * @brief Wait for a thread to finish
 *
 * @param handle Thread handle
 *
 * @return 0 on success
 */
int thread_join(thread_handle_t handle);


/**
 * @brief Terminate the calling thread
 *
 * @param retval Return value
 */
void thread_exit(void *retval);

/**
 * @}
 */


 /**
 * @defgroup mutex_api Mutex API
 * @{
 */

/**
 * @brief Mutex type
 */

#if defined(__linux__)

typedef pthread_mutex_t mutex_t;

#endif

#if defined(_WIN32) || defined(_WIN64)

typedef CRITICAL_SECTION mutex_t;

#endif 


/**
 * @brief Initialize mutex
 *
 * @param m Mutex object
 *
 * @return 0 on success
 */
int mutex_init(mutex_t *m);


/**
 * @brief Lock mutex
 *
 * @param m Mutex object
 *
 * @return 0 on success
 */
int mutex_lock(mutex_t *m);


/**
 * @brief Unlock mutex
 *
 * @param m Mutex object
 *
 * @return 0 on success
 */
int mutex_unlock(mutex_t *m);


/**
 * @brief Destroy mutex
 *
 * @param m Mutex object
 *
 * @return 0 on success
 */
int mutex_destroy(mutex_t *m);


/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __THREADS_H__ */
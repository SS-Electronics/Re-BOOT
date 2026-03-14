/* 
File:        queues.h
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      Thread  
Info:        Thread Safe Queue implementation            
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
along with Re-BOOT. If not, see <https://www.gnu.org/licenses/>.
*/

/**
 * @file queues.h
 * @brief Thread-safe queue implementation.
 *
 * This module provides a portable thread-safe FIFO queue designed
 * for inter-thread communication.
 *
 * The queue internally uses:
 * - mutex for mutual exclusion
 * - condition variable for blocking waits
 *
 * Typical use cases include:
 *
 * - Producer / Consumer systems
 * - Thread pools
 * - Message passing frameworks
 * - Work scheduling
 *
 * Features:
 * - Thread-safe push/pop operations
 * - Blocking pop
 * - Non-blocking pop
 * - Queue size query
 */

#ifndef __QUEUES_H__
#define __QUEUES_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "app_types.h"
#include "threads.h"

/**
 * @defgroup queue Thread-safe Queue
 * @brief FIFO queue for inter-thread communication.
 * @{
 */


/**
 * @brief Queue node structure.
 *
 * Each node stores a pointer to user data and a pointer to the next node.
 */
typedef struct queue_node
{
    void *data;                 /**< Pointer to user data */
    struct queue_node *next;    /**< Pointer to next node */
} queue_node_t;


/**
 * @brief Thread-safe queue object.
 *
 * This structure represents a FIFO queue that can safely be used
 * by multiple producer and consumer threads.
 *
 * Synchronization is achieved using a mutex and condition variable.
 */
typedef struct
{
    queue_node_t *head;         /**< Pointer to first node */
    queue_node_t *tail;         /**< Pointer to last node */

    size_t size;                /**< Number of elements in queue */

    mutex_t mutex;              /**< Queue mutex */
    cond_t cond;                /**< Condition variable for blocking pop */

} queue_t;


/**
 * @brief Initialize a queue.
 *
 * Must be called before using the queue.
 *
 * @param q Pointer to queue object.
 *
 * @return
 * - 0 on success
 * - negative value on failure
 */
int queue_init(queue_t *q);


/**
 * @brief Destroy a queue.
 *
 * Frees all remaining nodes and releases synchronization resources.
 *
 * @warning
 * Queue must not be used by other threads when calling this function.
 *
 * @param q Pointer to queue object.
 */
void queue_destroy(queue_t *q);


/**
 * @brief Push a message into the queue.
 *
 * Adds a new element at the tail of the queue.
 *
 * This operation is thread-safe.
 *
 * If consumer threads are waiting, one of them will be awakened.
 *
 * @param q Pointer to queue.
 * @param msg Pointer to user data.
 */
void queue_push(queue_t *q, void *msg);


/**
 * @brief Pop a message from the queue.
 *
 * Removes and returns the first element from the queue.
 *
 * If the queue is empty, the calling thread blocks until
 * a new element is pushed.
 *
 * @param q Pointer to queue.
 *
 * @return Pointer to message data.
 */
void* queue_pop(queue_t *q);


/**
 * @brief Try to pop a message without blocking.
 *
 * If the queue is empty the function immediately returns.
 *
 * @param q Pointer to queue.
 * @param msg Output pointer receiving message.
 *
 * @return
 * - true  : message successfully retrieved
 * - false : queue was empty
 */
bool queue_try_pop(queue_t *q, void **msg);


/**
 * @brief Get number of elements in queue.
 *
 * This operation is thread-safe.
 *
 * @param q Pointer to queue.
 *
 * @return Number of elements currently in the queue.
 */
size_t queue_size(queue_t *q);


/** @} */ /* end of queue group */

#ifdef __cplusplus
}
#endif

#endif
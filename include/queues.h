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
along with FreeRTOS-KERNEL. If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef __QUEUES_H__
#define __QUEUES_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "app_types.h"
#include "threads.h"


/**
 * @brief Queue node
 */
typedef struct queue_node
{
    void *data;
    struct queue_node *next;
} queue_node_t;


/**
 * @brief Thread-safe queue
 */
typedef struct
{
    queue_node_t *head;
    queue_node_t *tail;

    size_t size;

    mutex_t mutex;
    cond_t cond;

} queue_t;


/**
 * @brief Initialize queue
 */
int queue_init(queue_t *q);


/**
 * @brief Destroy queue
 */
void queue_destroy(queue_t *q);


/**
 * @brief Push message
 */
void queue_push(queue_t *q, void *msg);


/**
 * @brief Pop message (blocking)
 */
void* queue_pop(queue_t *q);


/**
 * @brief Try pop (non blocking)
 */
bool queue_try_pop(queue_t *q, void **msg);


/**
 * @brief Get queue size
 */
size_t queue_size(queue_t *q);

#ifdef __cplusplus
}
#endif

#endif
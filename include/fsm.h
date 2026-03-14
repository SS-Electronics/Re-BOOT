/* 
File:        fsm.c
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      Init  
Info:        Finite State Machine related functions           
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

* @file fsm.h
* @brief Generic Finite State Machine (FSM) Framework
*
* This module implements a lightweight event-driven finite state machine.
* It supports:
*
* * Hierarchical states
* * Entry/Exit handlers
* * Event based transitions
* * Action callbacks
* * Internal event queue
*
*/

#ifndef __FSM_H__
#define __FSM_H__

#include "app_types.h"
#include "pipeline.h"
#include "threads.h"
#include "queues.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct fsm_state;
struct fsm;

/**
* @brief FSM event object
*
* Events are dispatched to the FSM to trigger state transitions.
* Events can optionally carry user data.
*/
typedef struct fsm_event
{
    uint32_t type;            /**< Event identifier */
    void *data;               /**< Optional user payload */
    struct fsm_event *next;   /**< Internal queue linkage */

} fsm_event_t;


/**
* @brief Transition action callback
*
* Called when a transition occurs.
*
* @param event  Pointer to triggering event
* @param fsm    FSM instance
*/
typedef void (*fsm_action_t)(fsm_event_t *event, struct fsm *fsm);


/**
* @brief State entry/exit callback
*
* Entry is called when entering the state.
* Exit is called when leaving the state.
*
* @param state  Current state
* @param fsm    FSM instance
*/
typedef void (*fsm_entry_exit_t)(struct fsm_state *state, struct fsm *fsm);


/**
* @brief FSM state descriptor
*
* Represents a single state in the state machine.
* States can optionally have a parent to support hierarchical FSM design.
  */
typedef struct fsm_state
{
    uint32_t id;                    /**< Unique state identifier */

    struct fsm_state *parent;       /**< Parent state (for hierarchical FSM) */

    fsm_entry_exit_t on_entry;      /**< Entry handler */
    fsm_entry_exit_t on_exit;       /**< Exit handler */

}fsm_state_t;


/**
* @brief FSM transition table entry
*
* Defines a transition from one state to another when a specific
* event is received.
  */
typedef struct
{
    fsm_state_t *from;      /**< Source state */
    uint32_t event;         /**< Trigger event ID */
    fsm_state_t *to;        /**< Destination state */

    fsm_action_t action;    /**< Optional transition action */

} fsm_transition_t;


/**
* @brief FSM runtime context
*
* Maintains the runtime state of the finite state machine.
  */
typedef struct fsm
{
    int32_t fsm_running;
    fsm_state_t *current_state;        /**< Current active state */

    const fsm_transition_t *table;     /**< Transition table */
    size_t table_size;                 /**< Number of transitions */

    void *user_data;                   /**< User-defined context */

    /* Internal event queue */
    fsm_event_t *queue_head;           /**< Event queue head */
    fsm_event_t *queue_tail;           /**< Event queue tail */

#if(USE_THREAD_SAFE_FSM == 1)
    mutex_t lock;
#endif

} fsm_t;

/**

* @brief Create a new FSM event
*
* Allocates and initializes an event object.
*
* @param type Event identifier
* @param data Optional event payload
*
* @return Pointer to created event
  */
  fsm_event_t* fsm_event_create(uint32_t type, void *data);

/**

* @brief Free an FSM event
*
* Releases memory allocated for an event.
*
* @param event Event to free
  */
  void fsm_event_free(fsm_event_t *event);

/**

* @brief Initialize FSM instance
*
* Sets initial state and transition table.
*
* @param fsm            FSM instance
* @param initial_state  Initial state
* @param table          Transition table
* @param table_size     Number of transitions
* @param user_data      Optional user context
  */
  void fsm_init(fsm_t *fsm,
                fsm_state_t *initial_state,
                const fsm_transition_t *table,
                size_t table_size,
                void *user_data);

/**

* @brief Dispatch event to FSM
*
* Queues the event for processing by the state machine.
*
* @param fsm   FSM instance
* @param event Event to dispatch
  */
  void fsm_dispatch(fsm_t *fsm, fsm_event_t *event);

/**

* @brief Run FSM event processing loop
*
* Processes queued events and performs transitions.
* Typically called inside a worker thread or main loop.
*
* @param fsm FSM instance
  */
  void fsm_run(fsm_t *fsm);



#ifdef __cplusplus
}
#endif

#endif /* __FSM_H__ */

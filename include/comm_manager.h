/* 
File:        comm_manager.h
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      Init  
Info:        manage TX and RX communication           
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
 * @file comm_manager.h
 * @brief Communication manager for transport layer packet handling.
 *
 * This module provides the communication infrastructure responsible for
 * receiving packets from the transport layer and forwarding them to the
 * internal processing pipeline through message queues.
 *
 * The communication manager is designed to work in a multi-threaded
 * environment where a dedicated RX thread continuously receives packets
 * from the transport layer and pushes them into a queue for further
 * processing by other system components (e.g., FSM, protocol handlers).
 *
 * Features:
 * - Asynchronous packet reception
 * - Thread-safe queue communication
 * - Sliding window protocol management
 * - Integration with transport drivers
 *
 * This module is part of the **Re-BOOT firmware update framework**.
 */

#ifndef __COMM_MANAGER_H__
#define __COMM_MANAGER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "app_types.h"
#include "global.h"

#include "threads.h"
#include "queues.h"
#include "transport_layer.h"


/**
 * @defgroup comm_manager Communication Manager
 * @brief Communication and packet handling subsystem.
 *
 * This module manages packet communication between the host and
 * the target system using a transport abstraction layer.
 *
 * Responsibilities include:
 * - Receiving packets from transport drivers
 * - Maintaining sliding window state
 * - Pushing packets to processing queues
 * - Supporting concurrent RX thread execution
 *
 * @{
 */


/**
 * @brief Sliding window protocol state.
 *
 * This structure maintains the state variables required for the
 * sliding window protocol used during firmware transfer.
 *
 * The sliding window mechanism allows multiple packets to be sent
 * before receiving acknowledgments, improving communication
 * throughput and reducing latency.
 */
typedef struct
{
    uint32_t window_size;   /**< Maximum number of packets allowed in flight */
    uint32_t in_flight;     /**< Current number of packets awaiting acknowledgment */
    uint32_t next_seq;      /**< Sequence number of the next packet to transmit */
    uint32_t ack_seq;       /**< Last acknowledged packet sequence number */

} sliding_window_t;


/**
 * @brief Communication RX thread.
 *
 * This thread continuously receives packets from the transport layer
 * and forwards them to the system processing queue.
 *
 * Typical workflow:
 * - Wait for incoming packets from transport
 * - Allocate packet structure
 * - Push packet to RX queue
 * - Signal processing subsystem
 *
 * This thread runs concurrently with the main application and other
 * worker threads.
 *
 * @param arg
 * Optional user argument (unused in most cases).
 *
 * @return
 * Pointer return value required by thread interface.
 */
void* comm_rx_thread(void *arg);


/** @} */ /* end of comm_manager group */


#ifdef __cplusplus
}
#endif

#endif /* __COMM_MANAGER_H__ */

/* 
File:        comm_manager.c
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

#include "comm_manager.h"

/**
 * @brief Temporary packet buffer used for reception.
 *
 * This buffer stores the packet received from the transport layer
 * before it is copied into dynamically allocated memory for queue
 * insertion.
 */
static comm_packet_t receive_info_packet;


/**
 * @brief Communication receive worker thread.
 *
 * This thread continuously listens for incoming packets from the
 * transport layer and forwards them to the internal message queue
 * for further processing by other subsystems (e.g., FSM dispatcher).
 *
 * The RX thread runs independently of the main application thread
 * and ensures that incoming packets are captured and processed
 * asynchronously.
 *
 * Workflow:
 * @code
 * Transport Layer
 *       │
 *       ▼
 * transport_receive()
 *       │
 *       ▼
 * Allocate packet memory
 *       │
 *       ▼
 * Copy received packet
 *       │
 *       ▼
 * queue_push()
 *       │
 *       ▼
 * FSM / Processing Threads
 * @endcode
 *
 * Memory Handling:
 * - Each received packet is dynamically allocated before pushing
 *   into the queue.
 * - The consumer thread (e.g., FSM signal generator) must free
 *   the packet after processing.
 *
 * Thread Behavior:
 * - Runs in an infinite loop.
 * - Blocks or polls depending on the implementation of
 *   `transport_receive()`.
 *
 * @param arg
 * Optional user argument passed during thread creation.
 * Currently unused.
 *
 * @return
 * Always returns NULL when the thread terminates.
 *
 * @note
 * This thread should normally run for the entire lifetime of the
 * communication system.
 *
 * @warning
 * Ensure the consumer of the queue releases the allocated packet
 * memory to avoid memory leaks.
 */
void* comm_rx_thread(void *arg)
{
    while(true)
    {
        comm_packet_t pkt;

        /* Receive one packet from transport layer */
        if(transport_receive(&pkt) > 0)
        {
            /* Allocate memory for packet to be stored in queue */
            comm_packet_t *packet_ptr = (comm_packet_t*)malloc(sizeof(comm_packet_t));
            if(packet_ptr == NULL)
                continue;

            /* Copy received packet into allocated memory */
            memcpy(packet_ptr, &pkt, sizeof(comm_packet_t));

            /* Push packet pointer to receive queue */
            queue_push(&handle_queue_receive_packets, packet_ptr);
        }
    }

    return NULL;
}

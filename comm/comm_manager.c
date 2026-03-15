/* 
File:        comm_manager.c
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Module:      Comm  
Info:        Manage TX and RX communication           
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
 * @file comm_manager.c
 * @brief Communication receive thread for the Re-BOOT host application.
 *
 * This module owns the dedicated RX thread that runs for the entire
 * lifetime of a firmware update session.  Its only job is to call
 * @c transport_receive() in a tight loop, heap-allocate a copy of each
 * successfully parsed packet, and push that copy into the shared
 * @c handle_queue_receive_packets queue for the FSM signal generator to
 * consume.
 *
 * @par Threading model
 * @code
 *   ┌─────────────────┐      push       ┌──────────────────────┐
 *   │  comm_rx_thread │ ─────────────►  │  RX packet queue     │
 *   │  (this file)    │                 │  handle_queue_...    │
 *   └─────────────────┘                 └──────────────────────┘
 *          │ transport_receive()                   │ queue_try_pop()
 *          ▼                                       ▼
 *   serial / TCP driver              act_fsm_signal_generation()
 *                                         │
 *                                         ▼
 *                                      fsm_dispatch()
 * @endcode
 *
 * @par Memory ownership
 * Each packet pushed into the queue is a @c malloc'd @ref comm_packet_t.
 * Ownership transfers to the queue consumer (@c act_fsm_signal_generation)
 * which is responsible for calling @c free() after processing.
 *
 * @par Error handling
 * @c transport_receive() returns negative values for framing errors
 * (-3) and CRC mismatches (-5).  Both are logged here with the raw
 * return code so that serial line problems become visible during
 * debugging.  The thread simply loops back and waits for the next
 * valid frame — no further action is needed because the host protocol
 * is stop-and-wait and the target will retransmit on timeout.
 */

#include "comm_manager.h"

/**
 * @brief Temporary stack packet used during reception.
 *
 * @c transport_receive() writes the parsed packet into this stack
 * variable.  A heap copy is made before the packet is queued so that
 * the next call to @c transport_receive() cannot overwrite a packet
 * that is still being processed by the FSM thread.
 *
 * This variable is @b not shared between calls (it is re-used each
 * iteration) and is @b not accessed by any other thread, so no
 * synchronisation is needed for the variable itself.
 */
static comm_packet_t receive_info_packet;


/**
 * @brief Communication receive worker thread.
 *
 * Runs continuously until @c *thread_running_flag is cleared.  On
 * each iteration it blocks inside @c transport_receive() until either
 * a complete packet arrives or the thread is asked to stop.
 *
 * @par Happy path (one iteration)
 *  1. @c transport_receive() returns a positive payload length.
 *  2. A @ref comm_packet_t is allocated on the heap.
 *  3. The received packet is copied into the heap allocation.
 *  4. The pointer is pushed into @c handle_queue_receive_packets.
 *  5. Loop back to step 1.
 *
 * @par Error path
 *  - Return value @c -3 means the payload length field in the frame
 *    exceeded @c COMM_MAX_DATA.  The parser has already reset itself.
 *  - Return value @c -5 means the CRC16 did not match.  The parser has
 *    already reset itself.
 *  - Both cases are logged and the loop continues immediately.
 *  - Return value @c -1 means the thread stop flag was cleared; the
 *    loop exits.
 *
 * @param arg  Pointer to an @c int32_t thread-running flag.  The thread
 *             exits when @c *arg becomes 0.
 *
 * @return NULL always (required by the thread interface).
 *
 * @note The consumer of the queue (@c act_fsm_signal_generation) must
 *       call @c free() on every packet pointer it pops, otherwise heap
 *       memory will be leaked.
 */
void* comm_rx_thread(void *arg)
{
    int32_t *thread_running_flag = (int32_t *)arg;

    while (*thread_running_flag)
    {
        /* ---- Block until a frame arrives or the thread is stopped --- */
        int ret = transport_receive(&receive_info_packet, thread_running_flag);

        if (ret > 0)
        {
            /* ---- Valid packet received — queue it for the FSM ------- */

            comm_packet_t *packet_ptr =
                (comm_packet_t *)malloc(sizeof(comm_packet_t));

            if (packet_ptr == NULL)
            {
                /* Out of memory — skip this packet and keep running.
                   The FSM will time out waiting for the ACK and the
                   target will retransmit. */
                printf("[ RX ] malloc failed — dropping cmd=0x%02X\n",
                       receive_info_packet.command);
                continue;
            }

            /* Copy the stack packet into the heap allocation */
            memcpy(packet_ptr, &receive_info_packet, sizeof(comm_packet_t));

            /* Transfer ownership to the queue */
            queue_push(&handle_queue_receive_packets, packet_ptr);

            printf("[ RX ] Queued cmd=0x%02X  len=%u\n",
                   packet_ptr->command, packet_ptr->length);
        }
        else if (ret == -5)
        {
            /* CRC mismatch — transport_receive already printed details.
               Parser has been reset; just loop back and wait for the
               next frame.  The host will time out and resend its last
               command if needed. */
            printf("[ RX ] CRC error — waiting for next frame\n");
        }
        else if (ret == -3)
        {
            /* Framing error (payload too large) — same recovery. */
            printf("[ RX ] Framing error — waiting for next frame\n");
        }
        /* ret == -1 means thread was stopped — the while condition
           will catch that on the next iteration and exit cleanly. */
    }

    printf("[ RX ] comm_rx_thread exiting\n");
    return NULL;
}
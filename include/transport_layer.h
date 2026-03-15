/* 
File:        transport_layer.c
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      Comm  
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
 * @file transport_layer.h
 * @brief Generic transport layer interface
 *
 * This module provides a transport abstraction used by the
 * application and protocol layers. It hides the underlying
 * communication driver implementation (Serial or TCP) and
 * provides a unified interface for sending and receiving
 * communication packets.
 *
 * Supported drivers:
 *  - Serial (drv_serial)
 *  - TCP    (drv_tcp)
 *
 * The transport layer is responsible for:
 *  - Initializing the communication driver
 *  - Sending serialized packets
 *  - Receiving packets from the driver
 *  - Closing the communication interface
 *
 * Typical architecture:
 *
 * @code
 *          FSM / Protocol Layer
 *                 │
 *                 ▼
 *           Transport Layer
 *         ┌────────┴────────┐
 *         ▼                 ▼
 *     Serial Driver      TCP Driver
 * @endcode
 */

#ifndef __TRANSPORT_LAYER_H__
#define __TRANSPORT_LAYER_H__

#include "app_types.h"
#include "drv_serial.h"
#include "drv_tcp.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @defgroup TRANSPORT_LAYER Transport Layer
 * @brief Generic communication transport abstraction
 *
 * Provides driver-independent packet communication.
 *
 * @{
 */


/**
 * @brief Initialize transport layer
 *
 * Initializes the communication transport based on
 * command line arguments or configuration parameters.
 *
 * Depending on the configuration this function may:
 *
 * - Open a serial port
 * - Connect to a TCP server
 *
 * @param[in] cmds
 * Pointer to command argument structure containing
 * configuration such as:
 *  - serial port
 *  - baudrate
 *  - tcp ip
 *  - tcp port
 *
 * @return int
 * @retval 0  Initialization successful
 * @retval -1 Initialization failed
 */
int transport_init(cmd_args_t *cmds);


/**
 * @brief Close transport layer
 *
 * Closes the active communication driver and releases
 * associated resources.
 *
 * @param[in] cmds
 * Pointer to command argument structure used during
 * initialization.
 */
void transport_close(cmd_args_t *cmds);


/**
 * @brief Send a communication packet through the active transport.
 *
 * The packet is encoded into the following frame format before
 * transmission:
 *
 * @code
 *  -----------------------------------------------------------
 *  | ':' | CMD | LEN_H | LEN_L | DATA ... | CRC_H | CRC_L |
 *  -----------------------------------------------------------
 *    1B    1B     1B      1B       N B        1B      1B
 * @endcode
 *
 * Where:
 *  - ':'       : Start delimiter
 *  - CMD       : Command identifier
 *  - LEN_H/LEN_L : Payload length (big endian)
 *  - DATA      : Payload bytes
 *  - CRC       : CRC16-CCITT calculated over CMD + LEN + DATA
 *
 * The function automatically routes the frame to the active
 * driver (Serial or TCP).
 *
 * @param[in] pkt Pointer to communication packet structure.
 *
 * @return int
 * @retval >0 Number of bytes transmitted
 * @retval -1 Invalid packet or driver not initialized
 */
int transport_send(comm_packet_t *pkt);


/**
 * @brief Receive a communication packet from the active transport.
 *
 * This function implements a byte-wise state machine parser that
 * reconstructs a packet from the transport stream.
 *
 * Expected frame format:
 *
 * @code
 *  -----------------------------------------------------------
 *  | ':' | CMD | LEN_H | LEN_L | DATA ... | CRC_H | CRC_L |
 *  -----------------------------------------------------------
 * @endcode
 *
 * The function validates the CRC before returning the packet.
 *
 * The parser operates continuously while the thread running flag
 * remains set.
 *
 * @param[out] pkt Pointer to packet structure where received
 *                 data will be stored.
 * @param[in]  thread_running_flag Pointer to thread control flag.
 *                                 Reception continues while this
 *                                 value is non-zero.
 *
 * @return int
 * @retval >0  Payload length of the received packet
 * @retval -1  Invalid argument or thread stopped
 * @retval -3  Payload length exceeds buffer size
 * @retval -5  CRC verification failed
 */
int transport_receive(comm_packet_t *pkt, int32_t * thread_running_flag);

/**
 * @brief Flush the serial receive buffer.
 *
 * Continuously reads and discards bytes from the serial driver
 * until no more data is available. This is useful for clearing
 * stale or corrupted data before starting a new communication
 * session.
 *
 * @return int
 * @retval 0 Flush completed
 */
int transport_flush(void);


/** @} */


#ifdef __cplusplus
}
#endif

#endif /* __TRANSPORT_LAYER_H__ */
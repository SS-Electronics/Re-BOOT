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
along with FreeRTOS-KERNEL. If not, see <https://www.gnu.org/licenses/>.
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
 * @brief Send communication packet
 *
 * Serializes and transmits a communication packet using
 * the currently active transport driver.
 *
 * @param[in] pkt
 * Pointer to communication packet structure.
 *
 * @return int
 * @retval >0 Number of bytes transmitted
 * @retval -1 Transmission failed
 */
int transport_send(comm_packet_t *pkt);


/**
 * @brief Receive communication packet
 *
 * Receives and parses a packet from the underlying
 * communication driver.
 *
 * @param[out] pkt
 * Pointer to packet structure that will contain the
 * received data.
 *
 * @return int
 * @retval >0 Number of bytes received
 * @retval 0  No data available
 * @retval -1 Receive error
 */
int transport_receive(comm_packet_t *pkt);


/** @} */


#ifdef __cplusplus
}
#endif

#endif /* __TRANSPORT_LAYER_H__ */
/* 
File:        drv_tcp.h
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      Driver  
Info:        TCP Read and Write Functions           
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
 * @file drv_tcp.h
 * @brief Cross-platform TCP socket driver interface
 *
 * This module provides a platform-independent abstraction for
 * TCP socket communication. It allows higher layers (transport,
 * protocol stack, FSM, etc.) to communicate over TCP without
 * dealing with OS-specific socket APIs.
 *
 * Supported platforms:
 *  - Linux / POSIX sockets
 *  - Windows / WinSock
 *
 * The driver provides APIs for:
 *  - Creating a TCP connection
 *  - Closing the connection
 *  - Transmitting data
 *  - Receiving data
 *
 * Example usage:
 *
 * @code
 * drv_tcp_t tcp;
 *
 * if (drv_tcp_open(&tcp, "192.168.1.100", 9000) == 0)
 * {
 *     uint8_t msg[] = "HELLO";
 *     drv_tcp_tx(&tcp, msg, sizeof(msg));
 *
 *     uint8_t buffer[128];
 *     int len = drv_tcp_rx(&tcp, buffer, sizeof(buffer));
 *
 *     drv_tcp_close(&tcp);
 * }
 * @endcode
 */

#ifndef DRV_TCP_H
#define DRV_TCP_H

#include "app_types.h"

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @defgroup TCP_DRIVER TCP Driver
 * @brief Cross-platform TCP socket communication driver
 *
 * This module implements a TCP socket abstraction used by
 * communication frameworks or transport layers.
 *
 * @{
 */


/**
 * @brief TCP driver context
 *
 * Holds the underlying socket descriptor used for TCP communication.
 */
typedef struct
{
#if defined(_WIN32) || defined(_WIN64)

    /**
     * @brief Windows socket handle
     */
    SOCKET socket_fd;

#elif defined(__linux__)

    /**
     * @brief POSIX socket file descriptor
     */
    int socket_fd;

#endif

} drv_tcp_t;


/**
 * @brief Open a TCP connection
 *
 * Establishes a TCP client connection to a remote server.
 *
 * @param[in,out] ctx
 * Pointer to TCP driver context.
 *
 * @param[in] ip
 * Remote server IP address (example: "192.168.1.10").
 *
 * @param[in] port
 * Remote server TCP port.
 *
 * @return int
 * @retval 0  Connection successful
 * @retval -1 Connection failed
 */
int drv_tcp_open(drv_tcp_t *ctx,
                 const char *ip,
                 uint16_t port);


/**
 * @brief Close TCP connection
 *
 * Closes the socket and releases associated resources.
 *
 * @param[in,out] ctx
 * Pointer to TCP driver context.
 */
void drv_tcp_close(drv_tcp_t *ctx);


/**
 * @brief Transmit TCP data
 *
 * Sends a buffer of data through the TCP connection.
 *
 * @param[in] ctx
 * Pointer to initialized TCP driver context.
 *
 * @param[in] data
 * Pointer to transmit buffer.
 *
 * @param[in] length
 * Number of bytes to transmit.
 *
 * @return int
 * @retval >0 Number of bytes transmitted
 * @retval -1 Transmission error
 */
int drv_tcp_tx(drv_tcp_t *ctx,
               const uint8_t *data,
               uint16_t length);


/**
 * @brief Receive TCP data
 *
 * Reads data from the TCP socket into the provided buffer.
 *
 * @param[in] ctx
 * Pointer to initialized TCP driver context.
 *
 * @param[out] buffer
 * Buffer to store received data.
 *
 * @param[in] max_len
 * Maximum number of bytes that can be stored in buffer.
 *
 * @return int
 * @retval >0 Number of bytes received
 * @retval 0  Connection closed
 * @retval -1 Receive error
 */
int drv_tcp_rx(drv_tcp_t *ctx,
               uint8_t *buffer,
               uint16_t max_len);


/** @} */

#ifdef __cplusplus
}
#endif

#endif /* DRV_TCP_H */
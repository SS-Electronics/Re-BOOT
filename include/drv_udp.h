/*
File:        drv_udp.h
Author:      Subhajit Roy
             subhajitroy005@gmail.com

Moudle:      Driver
Info:        UDP Read and Write Functions
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
 * @file drv_udp.h
 * @brief Cross-platform UDP socket driver interface
 *
 * This module provides a platform-independent abstraction for
 * UDP datagram communication. It allows higher layers (transport,
 * protocol stack, FSM, etc.) to communicate over UDP without
 * dealing with OS-specific socket APIs.
 *
 * Supported platforms:
 *  - Linux / POSIX sockets
 *  - Windows / WinSock
 *
 * The driver provides APIs for:
 *  - Opening a UDP socket and binding to a local port
 *  - Closing the socket
 *  - Transmitting datagrams to a remote peer
 *  - Receiving datagrams from a remote peer
 *
 * Because UDP is connectionless, the remote peer address is stored
 * inside the driver context at open time and reused by every
 * @c drv_udp_tx() call.  @c drv_udp_rx() accepts datagrams from
 * any sender; the source address of the last received datagram is
 * cached in @c drv_udp_t::peer so that replies can be sent back
 * with @c drv_udp_tx().
 *
 * Example usage:
 *
 * @code
 * drv_udp_t udp;
 *
 * if (drv_udp_open(&udp, "192.168.1.100", 9000) == 0)
 * {
 *     uint8_t msg[] = "HELLO";
 *     drv_udp_tx(&udp, msg, sizeof(msg));
 *
 *     uint8_t buffer[128];
 *     int len = drv_udp_rx(&udp, buffer, sizeof(buffer));
 *
 *     drv_udp_close(&udp);
 * }
 * @endcode
 */

#ifndef DRV_UDP_H
#define DRV_UDP_H

#include "app_types.h"

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @defgroup UDP_DRIVER UDP Driver
 * @brief Cross-platform UDP datagram communication driver
 *
 * This module implements a UDP socket abstraction used by
 * communication frameworks or transport layers.
 *
 * @{
 */


/**
 * @brief UDP driver context
 *
 * Holds the socket descriptor and the pre-resolved peer address
 * so that the transport layer can call @c drv_udp_tx() without
 * repeating address-resolution on every packet.
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

    /**
     * @brief Remote peer address
     *
     * Populated by @c drv_udp_open() from the @p ip and @p port
     * arguments.  Used by @c drv_udp_tx() as the destination for
     * every outgoing datagram.
     */
    struct sockaddr_in peer;

} drv_udp_t;


/**
 * @brief Open a UDP socket
 *
 * Creates a UDP socket, resolves the remote peer address, and
 * binds to an ephemeral local port so that incoming datagrams
 * can be received.
 *
 * @param[in,out] ctx
 * Pointer to UDP driver context.
 *
 * @param[in] ip
 * Remote peer IP address (example: "192.168.1.10").
 *
 * @param[in] port
 * Remote peer UDP port.
 *
 * @return int
 * @retval 0  Socket opened successfully
 * @retval -1 Failed to create or configure socket
 */
int drv_udp_open(drv_udp_t *ctx,
                 const char *ip,
                 uint16_t port);


/**
 * @brief Close the UDP socket
 *
 * Closes the socket file descriptor and releases associated
 * resources.
 *
 * @param[in,out] ctx
 * Pointer to UDP driver context.
 */
void drv_udp_close(drv_udp_t *ctx);


/**
 * @brief Transmit a UDP datagram
 *
 * Sends a buffer of data as a single UDP datagram to the peer
 * address stored in the context.
 *
 * @param[in] ctx
 * Pointer to initialized UDP driver context.
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
int drv_udp_tx(drv_udp_t *ctx,
               const uint8_t *data,
               uint16_t length);


/**
 * @brief Receive a UDP datagram
 *
 * Reads one datagram from the socket into the provided buffer.
 * The source address of the received datagram is cached in
 * @c ctx->peer so that a subsequent @c drv_udp_tx() replies to
 * the correct sender.
 *
 * @param[in] ctx
 * Pointer to initialized UDP driver context.
 *
 * @param[out] buffer
 * Buffer to store received data.
 *
 * @param[in] max_len
 * Maximum number of bytes that can be stored in buffer.
 *
 * @return int
 * @retval >0 Number of bytes received
 * @retval 0  No data (non-blocking socket returned immediately)
 * @retval -1 Receive error
 */
int drv_udp_rx(drv_udp_t *ctx,
               uint8_t *buffer,
               uint16_t max_len);


/** @} */

#ifdef __cplusplus
}
#endif

#endif /* DRV_UDP_H */

/*
File:        drv_udp.c
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
 * @file drv_udp.c
 * @brief Cross-platform UDP driver implementation
 *
 * This module implements the UDP socket abstraction defined in
 * drv_udp.h. It allows the transport layer to communicate using
 * UDP datagrams without dealing with platform-specific APIs.
 *
 * Supported platforms:
 *  - Linux / POSIX sockets
 *  - Windows / WinSock
 */

#include "drv_udp.h"

#if defined(_WIN32) || defined(_WIN64)

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#elif defined(__linux__)

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

#endif


/**
 * @brief Open a UDP socket
 *
 * Creates a UDP datagram socket, resolves the remote peer address
 * from @p ip and @p port, stores it in @c ctx->peer for use by
 * @c drv_udp_tx(), and sets a receive timeout of 100 ms so that
 * @c drv_udp_rx() behaves non-blocking under normal operation.
 *
 * @param[in,out] ctx
 * Pointer to UDP driver context
 *
 * @param[in] ip
 * Remote peer IP address (example: "192.168.1.10")
 *
 * @param[in] port
 * Remote peer UDP port
 *
 * @return int
 * @retval 0  Socket opened successfully
 * @retval -1 Failed to create or configure socket
 */
int drv_udp_open(drv_udp_t *ctx,
                 const char *ip,
                 uint16_t port)
{

#if defined(_WIN32) || defined(_WIN64)

    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

#endif

    ctx->socket_fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (ctx->socket_fd < 0)
        return -1;

    /* Resolve and cache the remote peer address */
    memset(&ctx->peer, 0, sizeof(ctx->peer));
    ctx->peer.sin_family = AF_INET;
    ctx->peer.sin_port   = htons(port);

#if defined(_WIN32) || defined(_WIN64)
    ctx->peer.sin_addr.s_addr = inet_addr(ip);
#elif defined(__linux__)
    if (inet_pton(AF_INET, ip, &ctx->peer.sin_addr) <= 0)
    {
        close(ctx->socket_fd);
        return -1;
    }
#endif

    /* Set receive timeout (100 ms) so drv_udp_rx() does not block
       indefinitely — matches the behaviour of the serial driver
       (VTIME=1 → 100 ms) and drv_tcp_rx() in non-blocking usage. */
#if defined(_WIN32) || defined(_WIN64)

    DWORD timeout_ms = 100;
    setsockopt(ctx->socket_fd,
               SOL_SOCKET,
               SO_RCVTIMEO,
               (const char *)&timeout_ms,
               sizeof(timeout_ms));

#elif defined(__linux__)

    struct timeval tv;
    tv.tv_sec  = 0;
    tv.tv_usec = 100000;    /* 100 ms */
    setsockopt(ctx->socket_fd,
               SOL_SOCKET,
               SO_RCVTIMEO,
               &tv,
               sizeof(tv));

#endif

    return 0;
}


/**
 * @brief Close the UDP socket
 *
 * Closes the underlying socket descriptor and releases any
 * platform-specific resources acquired by @c drv_udp_open().
 *
 * @param[in,out] ctx
 * Pointer to UDP driver context
 */
void drv_udp_close(drv_udp_t *ctx)
{

#if defined(_WIN32) || defined(_WIN64)

    closesocket(ctx->socket_fd);
    WSACleanup();

#elif defined(__linux__)

    close(ctx->socket_fd);

#endif
}


/**
 * @brief Transmit a UDP datagram
 *
 * Sends @p length bytes from @p data as a single UDP datagram to
 * the peer address stored in @c ctx->peer.
 *
 * @param[in] ctx
 * UDP driver context
 *
 * @param[in] data
 * Buffer containing data to transmit
 *
 * @param[in] length
 * Number of bytes to send
 *
 * @return int
 * @retval >0 Number of bytes transmitted
 * @retval -1 Transmission error
 */
int drv_udp_tx(drv_udp_t *ctx,
               const uint8_t *data,
               uint16_t length)
{
    return (int)sendto(ctx->socket_fd,
                       (const char *)data,
                       length,
                       0,
                       (struct sockaddr *)&ctx->peer,
                       sizeof(ctx->peer));
}


/**
 * @brief Receive a UDP datagram
 *
 * Reads one incoming datagram from the socket into @p buffer.
 * The source address of the received datagram is updated in
 * @c ctx->peer so that a subsequent @c drv_udp_tx() call replies
 * to the correct sender.
 *
 * The socket has a 100 ms receive timeout configured by
 * @c drv_udp_open(), so this call returns 0 (no data) rather
 * than blocking indefinitely when no datagram arrives.
 *
 * @param[in] ctx
 * UDP driver context
 *
 * @param[out] buffer
 * Buffer where received data will be stored
 *
 * @param[in] max_len
 * Maximum bytes to read
 *
 * @return int
 * @retval >0 Number of bytes received
 * @retval 0  Timeout — no datagram arrived within 100 ms
 * @retval -1 Receive error
 */
int drv_udp_rx(drv_udp_t *ctx,
               uint8_t *buffer,
               uint16_t max_len)
{

#if defined(_WIN32) || defined(_WIN64)

    int peer_len = sizeof(ctx->peer);

#elif defined(__linux__)

    socklen_t peer_len = sizeof(ctx->peer);

#endif

    int n = (int)recvfrom(ctx->socket_fd,
                          (char *)buffer,
                          max_len,
                          0,
                          (struct sockaddr *)&ctx->peer,
                          &peer_len);

    /* Normalize timeout / EAGAIN to 0 so the transport layer's
       "if (n <= 0) continue" loop behaves the same as for serial. */
    if (n < 0)
        return 0;

    return n;
}

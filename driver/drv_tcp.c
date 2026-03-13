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
along with FreeRTOS-KERNEL. If not, see <https://www.gnu.org/licenses/>.
*/

/**
 * @file drv_tcp.c
 * @brief Cross-platform TCP driver implementation
 *
 * This module implements the TCP socket abstraction defined in
 * drv_tcp.h. It allows the transport layer to communicate using
 * TCP sockets without dealing with platform-specific APIs.
 *
 * Supported platforms:
 *  - Linux / POSIX sockets
 *  - Windows / WinSock
 */

#include "drv_tcp.h"

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
 * @brief Open TCP connection
 *
 * Creates a TCP socket and connects to the specified server.
 *
 * @param[in,out] ctx
 * Pointer to TCP driver context
 *
 * @param[in] ip
 * Server IP address (example: "192.168.1.10")
 *
 * @param[in] port
 * Server port
 *
 * @return int
 * @retval 0  Connection established
 * @retval -1 Connection failed
 */
int drv_tcp_open(drv_tcp_t *ctx,
                 const char *ip,
                 uint16_t port)
{

#if defined(_WIN32) || defined(_WIN64)

    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);

#endif

    ctx->socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (ctx->socket_fd < 0)
        return -1;

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_port   = htons(port);

#if defined(_WIN32) || defined(_WIN64)
    server.sin_addr.s_addr = inet_addr(ip);
#elif defined(__linux__)
    inet_pton(AF_INET, ip, &server.sin_addr);
#endif

    if (connect(ctx->socket_fd,
                (struct sockaddr *)&server,
                sizeof(server)) < 0)
    {
        return -1;
    }

    return 0;
}


/**
 * @brief Close TCP connection
 *
 * Closes the underlying socket descriptor.
 *
 * @param[in,out] ctx
 * Pointer to TCP driver context
 */
void drv_tcp_close(drv_tcp_t *ctx)
{

#if defined(_WIN32) || defined(_WIN64)

    closesocket(ctx->socket_fd);
    WSACleanup();

#elif defined(__linux__)

    close(ctx->socket_fd);

#endif
}


/**
 * @brief Transmit TCP data
 *
 * Sends data over the TCP connection.
 *
 * @param[in] ctx
 * TCP driver context
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
int drv_tcp_tx(drv_tcp_t *ctx,
               const uint8_t *data,
               uint16_t length)
{
    return send(ctx->socket_fd, (const char*)data, length, 0);
}


/**
 * @brief Receive TCP data
 *
 * Reads incoming data from the TCP socket.
 *
 * @param[in] ctx
 * TCP driver context
 *
 * @param[out] buffer
 * Buffer where received data will be stored
 *
 * @param[in] max_len
 * Maximum bytes to read
 *
 * @return int
 * @retval >0 Number of bytes received
 * @retval 0  Connection closed
 * @retval -1 Receive error
 */
int drv_tcp_rx(drv_tcp_t *ctx,
               uint8_t *buffer,
               uint16_t max_len)
{
    return recv(ctx->socket_fd, (char*)buffer, max_len, 0);
}
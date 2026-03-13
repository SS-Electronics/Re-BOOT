/* 
File:        drv_serial.c
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      Init  
Info:        Serial Read and Write Functions           
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
 * @file drv_serial.c
 * @brief Cross-platform Serial driver implementation
 *
 * This file implements the serial driver abstraction defined in
 * drv_serial.h. It provides a unified API for serial communication
 * across multiple operating systems.
 *
 * Supported platforms:
 *  - Windows (Win32 COM API)
 *  - Linux (POSIX termios API)
 *
 * The implementation uses conditional compilation to select the
 * appropriate platform-specific code.
 */

#include "drv_serial.h"

#if defined(_WIN32) || defined(_WIN64)

/**
 * @brief Open and configure a serial port (Windows implementation)
 *
 * This function opens a COM port using the Windows Win32 API and
 * configures the port parameters such as baudrate, data bits,
 * stop bits, and parity.
 *
 * @param[in,out] ctx
 * Pointer to serial driver context.
 *
 * @param[in] port
 * COM port name (example: "COM3").
 *
 * @param[in] baudrate
 * Desired baudrate for communication.
 *
 * @return int
 * @retval 0  Serial port successfully opened
 * @retval -1 Failed to open or configure the serial port
 */
int drv_serial_open(drv_serial_t *ctx,
                    const char *port,
                    uint32_t baudrate)
{
    ctx->handle = CreateFileA(port,
                              GENERIC_READ | GENERIC_WRITE,
                              0,
                              NULL,
                              OPEN_EXISTING,
                              0,
                              NULL);

    if (ctx->handle == INVALID_HANDLE_VALUE)
        return -1;

    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);

    /* Retrieve current configuration */
    if (!GetCommState(ctx->handle, &dcb))
        return -1;

    /* Configure serial parameters */
    dcb.BaudRate = baudrate;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity   = NOPARITY;

    if (!SetCommState(ctx->handle, &dcb))
        return -1;

    return 0;
}


/**
 * @brief Close the serial port (Windows implementation)
 *
 * Releases the COM port handle associated with the driver context.
 *
 * @param[in,out] ctx
 * Pointer to serial driver context.
 */
void drv_serial_close(drv_serial_t *ctx)
{
    CloseHandle(ctx->handle);
}


/**
 * @brief Transmit data over serial port (Windows implementation)
 *
 * Sends a buffer of bytes through the COM port.
 *
 * @param[in] ctx
 * Pointer to initialized serial driver context.
 *
 * @param[in] data
 * Pointer to data buffer to transmit.
 *
 * @param[in] length
 * Number of bytes to send.
 *
 * @return int
 * Number of bytes transmitted, or -1 on failure.
 */
int drv_serial_tx(drv_serial_t *ctx,
                  const uint8_t *data,
                  uint16_t length)
{
    DWORD written;

    if (!WriteFile(ctx->handle, data, length, &written, NULL))
        return -1;

    return (int)written;
}


/**
 * @brief Receive data from serial port (Windows implementation)
 *
 * Reads bytes from the COM port into the provided buffer.
 *
 * @param[in] ctx
 * Pointer to initialized serial driver context.
 *
 * @param[out] buffer
 * Destination buffer for received data.
 *
 * @param[in] max_len
 * Maximum number of bytes to read.
 *
 * @return int
 * Number of bytes received, or -1 on failure.
 */
int drv_serial_rx(drv_serial_t *ctx,
                  uint8_t *buffer,
                  uint16_t max_len)
{
    DWORD read_bytes;

    if (!ReadFile(ctx->handle, buffer, max_len, &read_bytes, NULL))
        return -1;

    return (int)read_bytes;
}

#elif defined(__linux__)   /* ================= Linux Implementation ================= */

#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

/**
 * @brief Open and configure a serial port (Linux implementation)
 *
 * Opens a serial device and configures it using POSIX termios API.
 *
 * @param[in,out] ctx
 * Pointer to serial driver context.
 *
 * @param[in] port
 * Serial device path.
 * Example:
 *  - "/dev/ttyUSB0"
 *  - "/dev/ttyS0"
 *
 * @param[in] baudrate
 * Communication baudrate (currently fixed internally).
 *
 * @return int
 * @retval 0  Serial port successfully opened
 * @retval -1 Failed to open or configure serial device
 */
int drv_serial_open(drv_serial_t *ctx,
                    const char *port,
                    uint32_t baudrate)
{
    ctx->fd = open(port, O_RDWR | O_NOCTTY);

    if (ctx->fd < 0)
        return -1;

    struct termios tty;

    if (tcgetattr(ctx->fd, &tty) != 0)
        return -1;

    /* NOTE: Currently fixed to 115200 */
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag |= CS8;

    if (tcsetattr(ctx->fd, TCSANOW, &tty) != 0)
        return -1;

    return 0;
}


/**
 * @brief Close serial device (Linux implementation)
 *
 * Closes the file descriptor associated with the serial device.
 *
 * @param[in,out] ctx
 * Pointer to serial driver context.
 */
void drv_serial_close(drv_serial_t *ctx)
{
    close(ctx->fd);
}


/**
 * @brief Transmit serial data (Linux implementation)
 *
 * Writes a buffer to the serial device.
 *
 * @param[in] ctx
 * Pointer to initialized serial driver context.
 *
 * @param[in] data
 * Buffer containing data to transmit.
 *
 * @param[in] length
 * Number of bytes to transmit.
 *
 * @return int
 * Number of bytes written or -1 on failure.
 */
int drv_serial_tx(drv_serial_t *ctx,
                  const uint8_t *data,
                  uint16_t length)
{
    return write(ctx->fd, data, length);
}


/**
 * @brief Receive serial data (Linux implementation)
 *
 * Reads bytes from the serial device into the buffer.
 *
 * @param[in] ctx
 * Pointer to initialized serial driver context.
 *
 * @param[out] buffer
 * Buffer where received data will be stored.
 *
 * @param[in] max_len
 * Maximum number of bytes to read.
 *
 * @return int
 * Number of bytes received or -1 on failure.
 */
int drv_serial_rx(drv_serial_t *ctx,
                  uint8_t *buffer,
                  uint16_t max_len)
{
    return read(ctx->fd, buffer, max_len);
}

#endif
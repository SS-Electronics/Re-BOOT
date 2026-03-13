/* 
File:        drv_serial.h
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      Driver  
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
 * @file drv_serial.h
 * @brief Cross-platform Serial Port Driver Interface
 *
 * This module provides a platform-independent serial communication
 * interface used by the transport layer or communication framework.
 *
 * Supported Platforms:
 *  - Linux using POSIX termios API
 *  - Windows using Win32 COM API
 *
 * The driver exposes a unified API for:
 *  - Opening serial ports
 *  - Closing serial ports
 *  - Transmitting data
 *  - Receiving data
 *
 * The implementation hides all platform-specific details behind a
 * common abstraction layer.
 *
 * Example usage:
 *
 * @code
 * drv_serial_t serial;
 *
 * if (drv_serial_open(&serial, "/dev/ttyUSB0", 115200) == 0)
 * {
 *     uint8_t msg[] = "Hello";
 *     drv_serial_tx(&serial, msg, sizeof(msg));
 *
 *     uint8_t buffer[64];
 *     int len = drv_serial_rx(&serial, buffer, sizeof(buffer));
 *
 *     drv_serial_close(&serial);
 * }
 * @endcode
 */

#ifndef __DRV_SERIAL_H__
#define __DRV_SERIAL_H__

#include "app_types.h"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @defgroup SERIAL_DRIVER Serial Driver
 * @brief Cross-platform serial communication driver
 *
 * This module implements a hardware abstraction layer (HAL)
 * for serial port communication across different operating systems.
 *
 * @{
 */


/**
 * @brief Serial driver context structure
 *
 * This structure holds the platform-specific serial port handle.
 * It must be initialized by calling drv_serial_open() before use.
 */
typedef struct
{

#if defined(_WIN32) || defined(_WIN64)

    /**
     * @brief Windows COM port handle
     */
    HANDLE handle;

#elif defined(__linux__)

    /**
     * @brief Linux file descriptor for serial device
     */
    int fd;

#endif

} drv_serial_t;


/**
 * @brief Open and configure a serial port
 *
 * Initializes and opens a serial port device with the specified
 * configuration parameters.
 *
 * Platform behavior:
 * - Linux: Uses POSIX `open()` and `termios`
 * - Windows: Uses `CreateFile()` and COM configuration APIs
 *
 * @param[in,out] ctx
 * Pointer to serial driver context.
 *
 * @param[in] port
 * Serial device name.
 *
 * Example:
 * - Linux  : "/dev/ttyUSB0"
 * - Linux  : "/dev/ttyS1"
 * - Windows: "COM3"
 *
 * @param[in] baudrate
 * Communication baudrate.
 *
 * Typical values:
 * - 9600
 * - 19200
 * - 38400
 * - 57600
 * - 115200
 *
 * @return int
 * @retval 0  Serial port opened successfully
 * @retval -1 Failed to open or configure serial port
 */
int drv_serial_open(drv_serial_t *ctx,
                    const char *port,
                    uint32_t baudrate);


/**
 * @brief Close an open serial port
 *
 * Releases the serial port handle and frees any associated
 * resources allocated during drv_serial_open().
 *
 * After this call the context becomes invalid and must not
 * be used until reopened.
 *
 * @param[in,out] ctx
 * Pointer to serial driver context
 */
void drv_serial_close(drv_serial_t *ctx);


/**
 * @brief Transmit data over serial port
 *
 * Sends a buffer of data through the serial interface.
 *
 * This function is typically called by the transport TX worker.
 *
 * @param[in] ctx
 * Pointer to initialized serial driver context
 *
 * @param[in] data
 * Pointer to transmit buffer
 *
 * @param[in] length
 * Number of bytes to transmit
 *
 * @return int
 * @retval >0 Number of bytes successfully transmitted
 * @retval -1 Transmission failed
 */
int drv_serial_tx(drv_serial_t *ctx,
                  const uint8_t *data,
                  uint16_t length);


/**
 * @brief Receive data from serial port
 *
 * Reads data from the serial interface into the provided buffer.
 *
 * The function may block depending on driver configuration.
 *
 * This function is typically executed by the RX worker thread.
 *
 * @param[in] ctx
 * Pointer to initialized serial driver context
 *
 * @param[out] buffer
 * Buffer to store received data
 *
 * @param[in] max_len
 * Maximum number of bytes that can be stored in buffer
 *
 * @return int
 * @retval >0 Number of bytes received
 * @retval 0  No data available
 * @retval -1 Receive error
 */
int drv_serial_rx(drv_serial_t *ctx,
                  uint8_t *buffer,
                  uint16_t max_len);


/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __DRV_SERIAL_H__ */
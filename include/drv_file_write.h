/* 
File:        drv_file_write.h
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      Driver  
Info:        Different File Write Operation Drivers           
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
 * @file drv_file_write.h
 * @brief Cross-platform file write abstraction layer.
 *
 * This module provides a minimal portable file API
 * for Linux and Windows systems.
 *
 * Supported Features:
 * - Open file
 * - Write data
 * - Append data
 * - Flush buffer
 * - Close file
 *
 * It hides platform specific differences between
 * POSIX and Win32 file systems.
 */

#ifndef __DRV_FILE_WRITE_H__
#define __DRV_FILE_WRITE_H__

#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__linux__)
#include <stdio.h>
#elif defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <stdio.h>
#endif


/**
 * @defgroup fileio File I/O API
 * @brief Portable file write abstraction
 * @{
 */


/**
 * @brief File open mode
 */
typedef enum
{
    FILEIO_WRITE,     /**< Create or overwrite file */
    FILEIO_APPEND     /**< Append to existing file */
} fileio_mode_t;


/**
 * @brief File object
 *
 * Platform independent file handle wrapper.
 */
typedef struct
{
#if defined(__linux__)
    FILE *handle;
#elif defined(_WIN32) || defined(_WIN64)
    FILE *handle;
#endif

} fileio_t;


/**
 * @brief Open a file.
 *
 * @param f File object
 * @param path File path
 * @param mode Write or append mode
 *
 * @return
 * - 0 success
 * - negative error
 */
int fileio_open(fileio_t *f, const char *path, fileio_mode_t mode);


/**
 * @brief Write data to file.
 *
 * @param f File object
 * @param data Pointer to buffer
 * @param size Number of bytes
 *
 * @return
 * number of bytes written
 */
size_t fileio_write(fileio_t *f, const void *data, size_t size);


/**
 * @brief Write formatted text.
 *
 * Works like printf().
 *
 * @param f File object
 * @param fmt Format string
 *
 * @return number of characters written
 */
int fileio_printf(fileio_t *f, const char *fmt, ...);


/**
 * @brief Flush file buffer.
 *
 * Forces buffered data to be written to disk.
 *
 * @param f File object
 *
 * @return
 * - 0 success
 * - negative error
 */
int fileio_flush(fileio_t *f);


/**
 * @brief Close file.
 *
 * @param f File object
 */
void fileio_close(fileio_t *f);


/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __DRV_FILE_WRITE_H__ */

/* 
File:        drv_file_write.c
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

#include "drv_file_write.h"

/**
 * @brief Open a file for writing or appending.
 *
 * This function opens a file using the specified mode and initializes
 * the file handle inside the file object.
 *
 * Supported modes:
 * - FILEIO_WRITE  : Creates a new file or truncates an existing file.
 * - FILEIO_APPEND : Opens an existing file and appends data to the end.
 *
 * The file is opened in **binary mode** to ensure consistent behavior
 * across Linux and Windows platforms.
 *
 * @param f
 * Pointer to the file object that will store the file handle.
 *
 * @param path
 * Path to the file to be opened.
 *
 * @param mode
 * File opening mode (write or append).
 *
 * @return
 * - 0  : File opened successfully.
 * - -1 : Failed to open file.
 *
 * @note
 * The caller must close the file using ::fileio_close() when finished.
 */
int fileio_open(fileio_t *f, const char *path, fileio_mode_t mode)
{
    const char *m = NULL;

    if(mode == FILEIO_WRITE)
        m = "wb";
    else
        m = "ab";

    f->handle = fopen(path, m);

    if(!f->handle)
        return -1;

    return 0;
}


/**
 * @brief Write raw data to a file.
 *
 * This function writes a block of memory directly to the file.
 * It is suitable for writing binary data, buffers, or serialized
 * structures.
 *
 * @param f
 * Pointer to the file object.
 *
 * @param data
 * Pointer to the memory buffer containing data to write.
 *
 * @param size
 * Number of bytes to write.
 *
 * @return
 * Number of bytes successfully written to the file.
 *
 * @retval 0
 * Returned if the file handle is invalid or no data was written.
 *
 * @note
 * This function does not automatically flush the file buffer.
 * Use ::fileio_flush() if immediate disk write is required.
 */
size_t fileio_write(fileio_t *f, const void *data, size_t size)
{
    if(!f || !f->handle)
        return 0;

    return fwrite(data, 1, size, f->handle);
}


/**
 * @brief Write formatted text to a file using fwrite().
 *
 * This function behaves similarly to printf(), but instead of writing
 * directly using vfprintf(), it first formats the output into an
 * intermediate buffer and then writes the buffer using ::fileio_write().
 *
 * This design ensures that all file output goes through the same
 * low-level write mechanism (fwrite), which can simplify logging,
 * buffering, and debugging in system frameworks.
 *
 * @param f
 * Pointer to the file object.
 *
 * @param fmt
 * Format string (same syntax as printf).
 *
 * @return
 * Number of bytes written to the file.
 *
 * @retval -1
 * Returned if the file handle is invalid.
 *
 * @note
 * The formatted string size is limited by the internal buffer.
 */
int fileio_printf(fileio_t *f, const char *fmt, ...)
{
    if(!f || !f->handle)
        return -1;

    char buffer[1024];

    va_list args;
    va_start(args, fmt);

    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);

    va_end(args);

    if(len <= 0)
        return -1;

    return fwrite(buffer, 1, len, f->handle);
}


/**
 * @brief Flush the file output buffer.
 *
 * Forces any buffered data associated with the file to be written
 * immediately to the storage device.
 *
 * This is useful in logging systems where data must be persisted
 * immediately.
 *
 * @param f
 * Pointer to the file object.
 *
 * @return
 * - 0  : Flush successful.
 * - -1 : Invalid file handle.
 */
int fileio_flush(fileio_t *f)
{
    if(!f || !f->handle)
        return -1;

    return fflush(f->handle);
}


/**
 * @brief Close an open file.
 *
 * This function closes the file associated with the file object
 * and releases the underlying system resources.
 *
 * After calling this function the file handle is reset to NULL
 * to prevent accidental reuse.
 *
 * @param f
 * Pointer to the file object.
 *
 * @note
 * Always close files opened with ::fileio_open() to avoid
 * resource leaks.
 */
void fileio_close(fileio_t *f)
{
    if(!f || !f->handle)
        return;

    fclose(f->handle);
    f->handle = NULL;
}

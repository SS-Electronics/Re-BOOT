/* 
File:        app_types.h
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      app_types.h 
Info:        All data type definition           
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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <app_config.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>

/* ERROR List: https://www.chromium.org/chromium-os/developer-library/reference/linux-constants/errnos/*/
#include <errno.h>

#define FLAG_SET    1
#define FLAG_RESET  0

#ifndef __APP_TYPES_H__
#define __APP_TYPES_H__

/**
 * @brief Structure to a generic memorypool
 */
typedef struct
{
    /**
     * @brief Pointer to dynamically allocated memory
     */
    uint8_t * data;

    /**
     * @brief Total size of allocated memory
     */
    uint32_t size;

} mem_pool_t;

/**
 * @brief Structure representing one Intel HEX record
 *
 * Each record corresponds to a single line from the HEX file.
 * It stores the address, number of data bytes, actual data,
 * and checksum from that record.
 */
typedef struct
{
    /**
     * @brief Start address of the record
     *
     * This is the absolute flash address after applying
     * the extended linear address (type 04).
     */
    uint32_t address;

    /**
     * @brief Number of valid data bytes
     */
    uint8_t length;

    /**
     * @brief Data bytes contained in the record
     *
     * Maximum Intel HEX record data length is 255 bytes.
     */
    uint8_t data[255];

    /**
     * @brief Record type
     *
     * Common types:
     * 00 = Data record
     * 01 = End of file
     * 04 = Extended linear address
     */
    uint8_t type;

    /**
     * @brief Record checksum
     */
    uint8_t checksum;

} hex_record_t;


/**
 * @brief Structure to get the command line arguments
 */
typedef struct
{
    /**
     * @brief Fields
     */
    char *file_path;   /**< HEX file path (-f) */
    int   node_id;     /**< Node ID (-n) */
    int   n_retry;     /**< Number of retries (-t) */
    int   reset;       /**< Reset flag (-r) */
    int   verbose;     /**< verbose level (-r) */

} cmd_args_t;


#endif /* __APP_TYPES_H__ */
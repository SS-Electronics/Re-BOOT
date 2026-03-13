/* 
File:        global.h
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      Init  
Info:        Global handles           
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

#ifndef __GLOBAL_H__
#define __GLOBAL_H__


#include "app_types.h"
#include "threads.h"
#include "queues.h"
#include "drv_file_write.h"




#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Firmware update FSM states.
 *
 * These states represent the progress of the firmware
 * transfer procedure.
 */
typedef enum
{
    ST_INIT = 0,        /**< Initial state before update starts */
    ST_SEND_RESET,      /**< Send reset command to target device */
    ST_BUILD_PIPELINE,  /**< Build sector transfer pipeline */
    ST_SEND_WINDOW,     /**< Send segmented data window */
    ST_VERIFY,          /**< Verify sector CRC */
    ST_NEXT_SECTOR,     /**< Move to next sector */
    ST_DONE             /**< Firmware update completed */
} state_id_t;


/**
 * @brief Firmware update FSM events.
 *
 * Events trigger state transitions inside the FSM.
 */
typedef enum
{
    EVT_START = 1,      /**< Start update process */
    EVT_TARGET_INFO,    /**< Target responded with device info */
    EVT_SEG_ACK,        /**< A segment was acknowledged */
    EVT_SECTOR_END,     /**< Last segment of sector sent */
    EVT_CRC_OK,         /**< CRC verification successful */
    EVT_APP_ACK         /**< Application acknowledged completion */
} event_id_t;

/**
 * @brief Log file write handle
 */
extern fileio_t handle_log_file;






#ifdef __cplusplus
}
#endif

#endif /* __GLOBAL_H__ */
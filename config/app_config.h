/* 
File:        app_config.c
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      Config  
Info:        Application related configuration           
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


#ifndef __APP_CONFIG_H__
#define __APP_CONFIG_H__

/**
 * @brief verbose levels
 */
#define VERBOSE_LEVEL_1 1
#define VERBOSE_LEVEL_2 2
#define VERBOSE_LEVEL_3 3


/**
 * @brief Communication Types
 */
#define SERIAL 1
#define TCP 2


/**
 * @brief Maximum character in a HEX file each line ':' to ':' length
 */
#define MAX_CHAR_PER_LINE   260

/**
 * @brief Max Payload to transport layer
 */
#define COMM_MAX_DATA  64

/**
 * @brief FSM Event queue Size
 */
#define QSIZE 128


/**
 * @brief Mutex implementation in FSM
 */
#define USE_THREAD_SAFE_FSM 1

/**
 * @brief Maximum retries if the user didn't provide any input
 */
#define MAX_RETRY              2


#endif /* __APP_CONFIG_H__ */
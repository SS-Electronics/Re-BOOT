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
 * @brief Log file write handle
 */
extern fileio_t handle_log_file;







#ifdef __cplusplus
}
#endif

#endif /* __GLOBAL_H__ */
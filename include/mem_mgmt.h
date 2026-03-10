/* 
File:        mem_mgmt.h
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      mem_mgmt.h  
Info:        API export related to memory management           
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

#include "app_types.h"



#ifndef __MEM_MGMT_H__
#define __MEM_MGMT_H__


#ifdef __cplusplus
extern "C" {
#endif


int32_t init_mem_pool(mem_pool_t * const buffer, uint32_t size);












#ifdef __cplusplus
}
#endif


#endif /* __MEM_MGMT_H__ */

/* 
File:        file_mgmt.h
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      Utility  
Info:        File related operations           
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

#include "app_types.h"



#ifndef __FILE_MGMT_H__
#define __FILE_MGMT_H__


#ifdef __cplusplus
extern "C" {
#endif


int parse_arguments(int argc, char *argv[], cmd_args_t *args);

int32_t get_file_size(char* const file_name);

int read_hex_file(char * const filename,
                  mem_pool_t * const buffer,
                  int32_t line_len,
                  int32_t * hex_base_address,
                  uint32_t * hex_end_address,
                  cmd_args_t *args);


#ifdef __cplusplus
}
#endif

#endif /* __MEM_MGMT_H__ */
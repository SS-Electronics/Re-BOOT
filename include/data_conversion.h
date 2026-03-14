/* 
File:        data_conversion.h
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      Utility  
Info:        Convert the data based on formats           
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



#ifndef __DATA_CONVERSION_H__
#define __DATA_CONVERSION_H__


#ifdef __cplusplus
extern "C" {
#endif


uint32_t hex_to_int(char * const hex, int len);


uint32_t crc32_update(uint32_t crc, uint8_t data);








#ifdef __cplusplus
}
#endif


#endif /* __DATA_CONVERSION_H__ */
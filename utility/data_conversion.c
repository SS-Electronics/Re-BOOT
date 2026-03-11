/* 
File:        data_conversion.c
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      data_conversion.c  
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
along with FreeRTOS-KERNEL. If not, see <https://www.gnu.org/licenses/>.
*/

#include "data_conversion.h"




/**
 * @brief Convert ASCII hex string to integer
 *
 * @param hex Pointer to hex string
 * @param len Number of characters to convert
 *
 * @return uint32_t Converted integer value
 */
uint32_t hex_to_int(char * const hex, int len)
{
    char buffer[16];

    strncpy(buffer, hex, len);
    buffer[len] = '\0';

    return strtoul(buffer, NULL, 16);
}
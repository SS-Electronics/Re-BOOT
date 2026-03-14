/* 
File:        data_conversion.c
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


uint32_t crc32(uint8_t *data,uint32_t len)
{
    uint32_t crc=0xFFFFFFFF;

    for(uint32_t i=0;i<len;i++)
    {
        crc ^= data[i];

        for(int j=0;j<8;j++)
            crc = (crc>>1) ^
            (0xEDB88320 & -(crc&1));
    }

    return ~crc;
}
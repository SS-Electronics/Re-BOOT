/* 
File:        transport_layer.c
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      Comm  
Info:        Switching Communication betwwen any 
             communication protocol, allowing a generic structure            
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

#include "transport_layer.h"



uint32_t build_packet(hex_record_t *rec, uint8_t *out)
{
    uint32_t idx = 0;

    out[idx++] = 0x01; // WRITE_CMD
    out[idx++] = rec->length;

    out[idx++] = (rec->address >> 24) & 0xFF;
    out[idx++] = (rec->address >> 16) & 0xFF;
    out[idx++] = (rec->address >> 8) & 0xFF;
    out[idx++] = rec->address & 0xFF;

    for(int i = 0; i < rec->length; i++)
        out[idx++] = rec->data[i];

    uint8_t crc = 0;
    for(int i = 0; i < idx; i++)
        crc ^= out[i];

    out[idx++] = crc;

    return idx;
}
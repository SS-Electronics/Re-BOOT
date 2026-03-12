/* 
File:        transport_layer.c
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      Comm  
Info:        Finite State Machine related functions           
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

#ifndef __TRANSPORT_LAYER_H__
#define __TRANSPORT_LAYER_H__


#ifdef __cplusplus
extern "C" {
#endif



int transport_connect(void);

int transport_send(uint8_t *data, uint32_t len);

int transport_receive(uint8_t *data, uint32_t max);

int transport_send_reset(void);

int transport_send_start(void);

int pipeline_send(hex_record_t *records,
                  int32_t total_lines,
                  uint32_t *current_line,
                  uint32_t sector_size);

uint32_t pipeline_calculate_crc(hex_record_t *records,
                                uint32_t start,
                                uint32_t end);


#ifdef __cplusplus
}
#endif


#endif /* __TRANSPORT_LAYER_H__ */
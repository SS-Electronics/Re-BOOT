/* 
File:        pipeline.h
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      Utility  
Info:        HEX Send pipeline utility           
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

#ifndef __PIPELINE_H__
#define __PIPELINE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "app_types.h"


typedef struct
{
    uint32_t base_addr;

    uint8_t *data;
    uint8_t *valid;

    uint32_t size;

} sector_pipeline_t;

typedef struct
{
    sector_pipeline_t *sectors;

    uint32_t sector_count;
    uint32_t sector_size;

} pipeline_builder_t;

void pipeline_build(
        pipeline_builder_t *pb,
        hex_record_t *records,
        uint32_t record_count,
        uint32_t sector_size);

int pipeline_next_segment(
        pipeline_builder_t *pb,
        uint32_t sector_index,
        uint32_t *offset,
        uint32_t segment_size,
        uint32_t *addr,
        uint8_t *data);

uint32_t pipeline_sector_crc(
        pipeline_builder_t *pb,
        uint32_t sector_index);



#ifdef __cplusplus
}
#endif

#endif /* __PIPELINE_H__ */
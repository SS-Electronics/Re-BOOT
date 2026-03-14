/* 
File:        pipeline.c
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

#include "pipeline.h"

static uint32_t sector_base(uint32_t addr,uint32_t size)
{
    return addr - (addr % size);
}

static sector_pipeline_t* find_sector(
        pipeline_builder_t *pb,
        uint32_t addr)
{
    uint32_t base = sector_base(addr,pb->sector_size);

    for(uint32_t i=0;i<pb->sector_count;i++)
        if(pb->sectors[i].base_addr==base)
            return &pb->sectors[i];

    pb->sectors = realloc(
        pb->sectors,
        sizeof(sector_pipeline_t)*(pb->sector_count+1));

    sector_pipeline_t *s =
        &pb->sectors[pb->sector_count++];

    s->base_addr = base;
    s->size = pb->sector_size;

    s->data = malloc(pb->sector_size);
    s->valid = malloc(pb->sector_size);

    memset(s->data,0xFF,pb->sector_size);
    memset(s->valid,0,pb->sector_size);

    return s;
}

static void insert_record(
        pipeline_builder_t *pb,
        hex_record_t *rec)
{
    if(rec->type!=0x00)
        return;

    for(uint32_t i=0;i<rec->length;i++)
    {
        uint32_t addr = rec->address+i;

        sector_pipeline_t *sec =
            find_sector(pb,addr);

        uint32_t off = addr-sec->base_addr;

        sec->data[off] = rec->data[i];
        sec->valid[off] = 1;
    }
}

void pipeline_build(
        pipeline_builder_t *pb,
        hex_record_t *records,
        uint32_t count,
        uint32_t sector)
{
    pb->sector_size = sector;
    pb->sector_count = 0;
    pb->sectors = NULL;

    for(uint32_t i=0;i<count;i++)
        insert_record(pb,&records[i]);
}

int pipeline_next_segment(
        pipeline_builder_t *pb,
        uint32_t sector_index,
        uint32_t *offset,
        uint32_t seg,
        uint32_t *addr,
        uint8_t *data)
{
    sector_pipeline_t *sec =
        &pb->sectors[sector_index];

    while(*offset < sec->size)
    {
        if(sec->valid[*offset])
            break;

        (*offset)++;
    }

    if(*offset >= sec->size)
        return -1;

    uint32_t len = seg;

    if(*offset+len > sec->size)
        len = sec->size-*offset;

    memcpy(data,&sec->data[*offset],len);

    *addr = sec->base_addr+*offset;

    *offset += len;

    return len;
}
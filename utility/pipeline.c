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

/* 
File:        pipeline.c
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Module:      Utility  
Info:        HEX Send pipeline utility           
Dependency:  None
...
*/

/**
 * @file pipeline.c
 * @brief Firmware transfer pipeline — sector organisation and segment
 *        extraction for the Re-BOOT host application.
 *
 * This module converts a flat array of parsed Intel HEX records into a
 * set of flash-sector-aligned data buffers (the "pipeline").  Once
 * built, the pipeline is consumed segment by segment during the
 * firmware upload sequence.
 *
 * @par Data model
 * @code
 *   pipeline_builder_t
 *     └── sectors[]  (dynamically grown with realloc)
 *           └── sector_pipeline_t
 *                 ├── data[]   (sector_size bytes, 0xFF where unprogrammed)
 *                 └── valid[]  (sector_size bytes, 1 = byte was in the HEX file)
 * @endcode
 *
 * Bytes not covered by any HEX data record are pre-filled with 0xFF,
 * matching the erased state of most flash memories.
 */

#include "pipeline.h"

/* ====================================================================
   Static helpers
   ==================================================================== */

/**
 * @brief Compute the base address of the sector that contains @p addr.
 *
 * Rounds @p addr down to the nearest multiple of @p size.
 *
 * @param addr  Absolute flash address.
 * @param size  Sector size in bytes.
 *
 * @return Base address of the containing sector.
 */
static uint32_t sector_base(uint32_t addr, uint32_t size)
{
    return addr - (addr % size);
}

/**
 * @brief Find or create the sector that owns @p addr.
 *
 * Walks the existing sector array looking for a sector whose
 * base address matches the one computed from @p addr.  If no
 * match is found a new @ref sector_pipeline_t is appended via
 * @c realloc and initialised with @c 0xFF data and zeroed valid map.
 *
 * @param pb    Pointer to the pipeline builder.
 * @param addr  Absolute flash address whose owning sector is needed.
 *
 * @return Pointer to the matching (or newly created) sector.
 *
 * @warning Returns a pointer into a @c realloc-managed array.  Any
 *          previously obtained pointer to a sector in the same
 *          builder may be invalidated after this call if the array
 *          is grown.  Do not cache sector pointers across calls.
 */
static sector_pipeline_t *find_sector(pipeline_builder_t *pb, uint32_t addr)
{
    uint32_t base = sector_base(addr, pb->sector_size);

    /* Search existing sectors for a matching base address */
    for (uint32_t i = 0; i < pb->sector_count; i++)
    {
        if (pb->sectors[i].base_addr == base)
            return &pb->sectors[i];
    }

    /* No match — allocate a new sector slot at the end of the array */
    pb->sectors = realloc(
        pb->sectors,
        sizeof(sector_pipeline_t) * (pb->sector_count + 1)
    );

    sector_pipeline_t *s = &pb->sectors[pb->sector_count++];

    s->base_addr = base;
    s->size      = pb->sector_size;

    /* Allocate and initialise data and validity buffers */
    s->data  = malloc(pb->sector_size);
    s->valid = malloc(pb->sector_size);

    /* Pre-fill data with 0xFF (erased flash state) */
    memset(s->data,  0xFF, pb->sector_size);

    /* Mark every byte as invalid until a HEX record writes it */
    memset(s->valid, 0,    pb->sector_size);

    return s;
}

/**
 * @brief Insert one HEX data record into the pipeline.
 *
 * Only type-0x00 (data) records are processed.  Each byte in the
 * record is placed at its absolute flash address within the
 * appropriate sector buffer and marked valid.
 *
 * @param pb   Pointer to the pipeline builder.
 * @param rec  Pointer to the HEX record to insert.
 */
static void insert_record(pipeline_builder_t *pb, hex_record_t *rec)
{
    /* Skip non-data records (extended address, EOF, etc.) */
    if (rec->type != 0x00)
        return;

    for (uint32_t i = 0; i < rec->length; i++)
    {
        uint32_t addr = rec->address + i;

        /* Locate (or create) the sector that owns this address */
        sector_pipeline_t *sec = find_sector(pb, addr);

        /* Offset of this byte within the sector */
        uint32_t off = addr - sec->base_addr;

        sec->data[off]  = rec->data[i];
        sec->valid[off] = 1;
    }
}

/* ====================================================================
   Public API
   ==================================================================== */

/**
 * @brief Build the sector pipeline from an array of HEX records.
 *
 * Iterates over every record and calls @c insert_record() to place
 * data bytes into sector-aligned buffers.  After this call the
 * builder's @c sectors array contains one entry per distinct flash
 * sector touched by the firmware image, sorted in the order the
 * sectors were first encountered in the HEX file.
 *
 * @param pb            Pointer to the pipeline builder to initialise.
 * @param records       Array of parsed HEX records.
 * @param count         Number of records in the array.
 * @param sector_size   Flash sector size in bytes (received from target).
 */
void pipeline_build(
        pipeline_builder_t *pb,
        hex_record_t       *records,
        uint32_t            count,
        uint32_t            sector_size)
{
    pb->sector_size  = sector_size;
    pb->sector_count = 0;
    pb->sectors      = NULL;

    for (uint32_t i = 0; i < count; i++)
        insert_record(pb, &records[i]);
}

/**
 * @brief Fetch the next segment from a sector for transmission.
 *
 * Scans forward from @p *offset within the sector identified by
 * @p sector_index, skipping any leading invalid (unprogrammed) bytes.
 * Once a valid byte is found, up to @p seg bytes are copied into
 * @p data and the absolute flash address of the first byte is written
 * to @p addr.
 *
 * @p *offset is advanced by the number of bytes returned, so repeated
 * calls automatically walk through the entire sector.
 *
 * @param pb            Pointer to the pipeline builder.
 * @param sector_index  Index of the sector to read from.
 * @param offset        In/out: byte offset within the sector.
 *                      Advanced by the number of bytes returned.
 * @param seg           Maximum segment size (bytes) to copy.
 * @param addr          Output: absolute flash address of the first byte.
 * @param data          Output buffer; must be at least @p seg bytes.
 *
 * @return Number of bytes copied into @p data (1 … @p seg),
 *         or -1 if no more valid bytes remain in the sector.
 */
int pipeline_next_segment(
        pipeline_builder_t *pb,
        uint32_t            sector_index,
        uint32_t           *offset,
        uint32_t            seg,
        uint32_t           *addr,
        uint8_t            *data)
{
    sector_pipeline_t *sec = &pb->sectors[sector_index];

    /* Skip leading invalid (unprogrammed) bytes */
    while (*offset < sec->size)
    {
        if (sec->valid[*offset])
            break;

        (*offset)++;
    }

    /* Sector exhausted — signal the caller to move to verify */
    if (*offset >= sec->size)
        return -1;

    /* Clamp segment length to what remains in the sector */
    uint32_t len = seg;

    if (*offset + len > sec->size)
        len = sec->size - *offset;

    /* Copy segment data and record the starting flash address */
    memcpy(data, &sec->data[*offset], len);
    *addr    = sec->base_addr + *offset;
    *offset += len;

    return (int)len;
}

/**
 * @brief Compute the CRC32 checksum of a complete sector.
 *
 * Calculates a standard CRC32 (polynomial 0xEDB88320, reflected,
 * initial value 0xFFFFFFFF, final XOR 0xFFFFFFFF) over the entire
 * sector data buffer including bytes marked invalid (which hold
 * the 0xFF erase value).  This matches what the target will compute
 * after it reads back the programmed sector from flash.
 *
 * @param pb            Pointer to the pipeline builder.
 * @param sector_index  Index of the sector to checksum.
 *
 * @return CRC32 of the sector's @c data buffer.
 */
uint32_t pipeline_sector_crc(pipeline_builder_t *pb, uint32_t sector_index)
{
    sector_pipeline_t *sec = &pb->sectors[sector_index];

    uint32_t crc = 0xFFFFFFFFu;

    for (uint32_t i = 0; i < sec->size; i++)
    {
        crc ^= (uint32_t)sec->data[i];

        /* Process each bit with the reflected CRC32 polynomial */
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if (crc & 1u)
                crc = (crc >> 1) ^ 0xEDB88320u;   /* standard CRC32 poly */
            else
                crc >>= 1;
        }
    }

    /* Apply final XOR to produce the standard CRC32 result */
    return crc ^ 0xFFFFFFFFu;
}
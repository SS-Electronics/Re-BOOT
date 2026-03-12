/* 
File:        file_mgmt.c
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
along with FreeRTOS-KERNEL. If not, see <https://www.gnu.org/licenses/>.
*/

#include "app_types.h"

#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <data_conversion.h>

/**
 * @brief Local Variables
 */
static struct stat hex_file_stat;

static FILE * hex_file_ptr;

/**
 * @brief Parse command line arguments
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @param args Pointer to structure storing parsed values
 *
 * @retval 0 Success
 * @retval -1 Missing mandatory arguments
 */
int parse_arguments(int argc, char *argv[], cmd_args_t *args)
{
    int i;

    /* Initialize defaults */
    args->file_path = NULL;
    args->node_id = -1;
    args->n_retry = 0;
    args->reset = 0;

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-f") == 0)
        {
            if (i + 1 < argc)
                args->file_path = argv[++i];
        }
        else if (strcmp(argv[i], "-n") == 0)
        {
            if (i + 1 < argc)
                args->node_id = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-t") == 0)
        {
            if (i + 1 < argc)
                args->n_retry = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-r") == 0)
        {
            if (i + 1 < argc)
                args->reset = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-v") == 0)
        {
            if (i + 1 < argc)
                args->verbose = atoi(argv[++i]);
        }
    }

    /* Validate mandatory arguments */
    if (args->file_path == NULL)
    {
        printf("[ ERR ] Hexfile Not Provided!\n");
        
        return -1;
    }
    
    if(args->node_id == -1)
    {
        printf("[ ERR ] Node Number Not Provided!\n");

        return -1;
    }

    return 0;
}


/**
 * @file  file_mgmt.c
 * @brief 
 *
 * @param file_name File name with path
 * @param fileStat_ptr struct stat pointer to receive the metadata
 *
 * @return int
 * @retval 0 Operation success
 *          
 */
int32_t get_file_size(char* const file_name)
{
    char ch;
    uint32_t lines = 0;
    
    hex_file_ptr = fopen(file_name, "r");

    if( file_name != NULL)
    {
        if( stat(file_name, &hex_file_stat) < 0)
        {
            printf("[ ERR ] File stat info not found!\n");
            
            return 0;
        }
        else
        {
            printf("File: %s\n", file_name);
            printf("File Size: %ld bytes\n", hex_file_stat.st_size);
            printf("Last modification: %s", ctime(&hex_file_stat.st_mtime));
            
            if((hex_file_stat.st_mode & S_IRUSR) == FLAG_RESET)
            {
                printf("[ ERR ] User has no file read prmission!\n");

                return 0;
            }
            else
            {
                if(hex_file_ptr != NULL)
                {
                    while ((ch = fgetc(hex_file_ptr)) != EOF)
                    {
                        if (ch == '\n')
                        {
                            lines++;
                        }    
                    }
                }

                fclose(hex_file_ptr);

                printf("Total Lines %u\n", lines);
                return (int32_t)lines;
            }
        }
    }
    else
    {
        fclose(hex_file_ptr);

        return 0;
    }

    
}


/**
 * @file  main.c
 * @brief Memory pool Init
 *
 * @param buffer mem_pool_t constant pointer to container
 * @param size size of the memory in bytes
 *
 * @return int
 * @retval 0 Operation success
 *          
 */
int read_hex_file(char * const filename,
                  mem_pool_t * const buffer,
                  int32_t line_len,
                  int32_t * hex_base_address,
                  uint32_t * hex_end_address,
                  cmd_args_t *args)
{
    char        line[MAX_CHAR_PER_LINE];
    uint32_t    line_count = 0;
    uint32_t    length;
    uint32_t    address, base_address = 0;
    uint32_t    type;
    uint32_t    byte_counter = 0;
    uint8_t     first_seg_add_flag = 1;

   uint32_t max_records = buffer->size / sizeof(hex_record_t);

    /** buffer->data -> allocated memory block
        Treat that block as hex_record_t arr[]
    */
    hex_record_t *record_arr = (hex_record_t *)buffer->data;

    /** Open the file  */
    hex_file_ptr = fopen(filename, "r");

    if(hex_file_ptr == NULL)
    {
         
        return EAGAIN;
    }

    while(fgets(line, sizeof(line), hex_file_ptr))
    {
        if(line[0] != ':')
        {
            continue; /** Not a valid hex record line */
        }

        
        /** : ll aaaa tt [dd...] cc*/
        length  = hex_to_int(line + 1, 2);
        address = hex_to_int(line + 3, 4);
        type    = hex_to_int(line + 7, 2);

        /** Fill record structure */
        record_arr[line_count].length = length;
        record_arr[line_count].type   = type;
        record_arr[line_count].checksum =
            hex_to_int(line + 9 + (length * 2), 2);

        /** Update extended address first */
        if(type == 0x04)
        {
            base_address = (hex_to_int(line + 9, 4) << 16);

            /** Only Store the first address flag */
            if(first_seg_add_flag == 1)
            {
                first_seg_add_flag = 0;
            }
        }

        /** Absolute address */
        record_arr[line_count].address = base_address + address;

        /** Track end address (only for data records) */
        if(type == 0x00)
        {
            /** Copy data bytes */
            for(uint32_t i = 0; i < length; i++)
            {
                record_arr[line_count].data[i] =
                    hex_to_int(line + 9 + (i * 2), 2);
                    byte_counter++;
            }

            uint32_t end_addr =
                record_arr[line_count].address + length;

            if(end_addr > *hex_end_address)
            {
                *hex_end_address = end_addr;
            }
        }

        if(args->verbose == VERBOSE_LEVEL_3)
        {
             printf("**************************************\n");
             printf("Line[%u]       %s", line_count, line);
             printf("Record[%u]     %02X 0x%08X %u %02X\n", \
                            line_count, \
                            record_arr[line_count].type, \
                            record_arr[line_count].address, \
                            record_arr[line_count].length, \
                            record_arr[line_count].checksum);
        }

        /** Next record */
        line_count++;

        // /** Prevent buffer overflow */
        // TODO: Need to implement based on buffer size
    }

    printf("Total Bytes: %u [ 0x%x -> 0x%x] \n\r", byte_counter, *hex_base_address, *hex_end_address);

    /** Close the file */
    fclose(hex_file_ptr);
    return 0;
}
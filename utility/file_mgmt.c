/* 
File:        file_mgmt.c
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      file_mgmt.c  
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

/**
 * @brief Local Variables
 */
static struct stat hex_file_stat;




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
                return (int32_t)hex_file_stat.st_size;
            }
        }
    }
    else
    {
        return 0;
    }
}


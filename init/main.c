/* 
File:        main.c
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      main.c  
Info:        Entry Point of the firmware           
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
#include "file_mgmt.h"
#include "mem_mgmt.h"

/**
 * @brief Operation related local variables
 */
static int32_t          status;
static cmd_args_t       cmds;

/**
 * @brief Memory related local variables
 */

static mem_pool_t mem_pool_hex_file_head;















/**
 * @file  main.c
 * @brief Main entry point of the program
 *
 * @param argc Number of command line arguments
 * @param argv Array containing command line arguments
 *
 * @return int
 * @retval 0 Program executed successfully
 *         1 Error 
 */
int main(int argc, char *argv[])
{
    /** Parse commadline arguments */
    status = parse_arguments(argc, argv, &cmds);

    if(status == 0) /** No error in commands so proceed */
    {

        get_file_size(cmds.file_path);


        /** Memory pool create for store hexfile based on HEX file size */
    // status = init_mem_pool(&mem_pool_hex_file_head, 100);



    }
    return 0;
}
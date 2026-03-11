/* 
File:        mem_mgmt.c
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      mem_mgmt.c  
Info:        Memory related operations           
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

#include "mem_mgmt.h"

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
int32_t create_mem_pool(mem_pool_t * const buffer, uint32_t size)
{
    buffer->data = (uint8_t *)malloc(size);
    buffer->size = size;

    if(buffer->data == NULL)
    {
        printf("[ ERR ] HEX file memory pool creation failed!\n");
        
        return ENOMEM;
    }

    return 0;
}



/**
 * @file  main.c
 * @brief Memory pool free
 *
 * @param buffer mem_pool_t constant pointer to container
 *
 * @return none
 *          
 */
void free_mem_pool(mem_pool_t * const buffer)
{
    if(buffer->data != NULL)
    {
        free(buffer->data);
    }    
}
/* 
File:        main.c
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      Init  
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
#include "threads.h"
#include "queues.h"
#include "file_mgmt.h"
#include "mem_mgmt.h"
#include "fsm.h"


/**
 * @brief Operation related local variables
 */
static int32_t          status;
static cmd_args_t       cmds;
static int32_t          hex_file_lines;

static uint32_t         hex_base_address;
static uint32_t         hex_end_address;

static uint32_t         current_line = 0;
static uint32_t         pipeline_sector_size = 0;
static uint32_t         target_address = 0;
static uint32_t         pipeline_bytes = 0;



/**
 * @brief Memory related local variables
 */

static mem_pool_t mem_pool_hex_file_head;


queue_t q;

void* producer(void *arg)
{
    int counter = 0;

    while (1)
    {
        char *msg = malloc(64);

        sprintf(msg, "Message %d", counter++);

        printf("Producer: pushing %s\n", msg);

        queue_push(&q, msg);

        thread_sleep(1000); // produce every 1 second
    }

    return NULL;
}


void* consumer(void *arg)
{
    void *msg;

    while (1)
    {
        if (queue_try_pop(&q, &msg))
        {
            printf("Consumer received: %s\n", (char*)msg);
            free(msg);
        }
        else
        {
            /* Queue empty */
            printf("Queue empty\n");
        }

        thread_sleep(300);
    }

    return NULL;
}








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

    thread_t prod_thread;
    thread_t cons_thread;

    queue_init(&q);

    /* Create threads */
    thread_create(&prod_thread, producer, NULL);
    thread_create(&cons_thread, consumer, NULL);

    /* Wait for threads (runs forever) */
    thread_join(&prod_thread);
    thread_join(&cons_thread);

    queue_destroy(&q);


    /** Parse commadline arguments */
    status = parse_arguments(argc, argv, &cmds);

    if(status == 0) /** No error in commands so proceed */
    {
        /** Get the file size and based on that the memory pool will be created */
        hex_file_lines = get_file_size(cmds.file_path);


        /** Memory pool create for store each hex line recors */
        status = create_mem_pool( &mem_pool_hex_file_head, (hex_file_lines * sizeof(hex_record_t)) );
        
        if(status == 0)
        {
            status = read_hex_file(cmds.file_path, 
                        &mem_pool_hex_file_head,
                        hex_file_lines,
                        &hex_base_address, &hex_end_address,
                        &cmds);
        }
        
        if( status == 0)
        {
            printf("HEX records created! ...");
        }


        //   bootloader_run(hex_records,
        //            hex_file_lines,
        //            hex_base_address,
        //            hex_end_address);



    }



    /** Free the allocated memory */
    free_mem_pool(&mem_pool_hex_file_head);

    return 0;
}
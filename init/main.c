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
along with Re-BOOT. If not, see <https://www.gnu.org/licenses/>.
*/



#include "app_types.h"
#include "global.h"

#include "drv_file_write.h"

#include "file_mgmt.h"
#include "mem_mgmt.h"

#include "threads.h"
#include "queues.h"

#include "fsm.h"
#include "fsm_table.h"
#include "comm_manager.h"
#include "transport_layer.h"

/**
 * @brief Operation related local variables
 */
static int32_t          app_running = FLAG_SET;
static int32_t          status;
static cmd_args_t       cmds;
static int32_t          hex_file_lines;
static thread_t         handle_comm_rx_thread;
static int32_t          comm_thread_running;
         

static uint32_t         hex_base_address;
static uint32_t         hex_end_address;

static uint32_t         current_line = 0;
static uint32_t         pipeline_sector_size = 0;
static uint32_t         target_address = 0;
static uint32_t         pipeline_bytes = 0;

/**
 * @brief Memory related local variables
 */

static fsm_t booloader_fsm;
/**
 * @brief Memory related local variables
 */

static mem_pool_t mem_pool_hex_file_head;

/**
 * @brief Function definitions
 */




 /**
 * @brief Bootloader context accross all states
 */
static struct
{
    hex_record_t *records;
    uint32_t record_count;
    pipeline_builder_t pipeline;
    uint16_t sector_size;
    uint16_t segment_size;
    uint32_t current_sector;
    uint32_t offset;
    uint32_t in_flight;
    uint8_t  thread_running;
} bootloader_context;


void handle_sigint(int sig)
{
    app_running = FLAG_RESET;
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
    /** Add signal interrupt from OS  */
    signal(SIGINT, handle_sigint);

    /** Parse commadline arguments */
    status = parse_arguments(argc, argv, &cmds);

    if(status == 0) /** No error in commands so proceed */
    {
        /** Transport Layer Driver Initialization  */
        status = transport_init(&cmds);
        transport_flush();

        if(status != 0)
        {
            printf("[ ERR ] Not able to open comm port %s %s ...\n", cmds.interface, cmds.ip);
            printf("Exiting...\n");

            goto exit;
        }

        printf("hello");
        /** Start the log file writing  */
        if( fileio_open(&handle_log_file, "./re-boot.log", FILEIO_WRITE) != EXIT_SUCCESS )
        {
            printf("[ ERR ] Log file can not be created! ...\n"); 

            goto exit;
        }

        /** Get the file size and based on that the memory pool will be created */
        hex_file_lines = get_file_size(cmds.file_path);

        /** Memory pool create for store each hex line recors */
        status = create_mem_pool( &mem_pool_hex_file_head, (hex_file_lines * sizeof(hex_record_t)) );

        if(status != 0 )
        {
            goto exit;
        }
        
        /* Inilialize the queues */
        queue_init(&handle_queue_receive_packets);

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
            printf("HEX records created! ...\n");
            fileio_printf(&handle_log_file,"HEX records created! ...\n");
        }
        else
        {
            printf("HEX records not created! Exiting ...\n");
            fileio_printf(&handle_log_file,"HEX records not created! Exiting ...\n");

            goto exit;
        }

        /** Initialization of Threads */
        comm_thread_running = FLAG_SET;
        thread_create(&handle_comm_rx_thread, comm_rx_thread, &comm_thread_running);

        /* Init state machine */
        fsm_init(&booloader_fsm,
                &ST_INIT_STATE,
                fsm_table,
                fsm_table_size,
                &bootloader_context);

        /* Start Initialization state */
        fsm_dispatch(&booloader_fsm, fsm_event_create(EVT_START, NULL));

        while ( (booloader_fsm.fsm_running == FLAG_SET) && (app_running == FLAG_SET) )
        {
            act_fsm_signal_generation(&booloader_fsm);
            fsm_run(&booloader_fsm);      /* process FSM */
        }
    }
    else
    {
        printf("Invalid Options... Exititng...\n");
        printf("For help ebter re-boot -h\n");

    }
    
    printf("Close Interrupt received! Exiting...\n");
    fileio_printf(&handle_log_file,"Close Interrupt received! Exiting...\n");

    /** Any case it will come this point to exit */
    exit:

    /** Wait for child thread to exit */
    comm_thread_running = FLAG_RESET;
    thread_join(&handle_comm_rx_thread);



    /** Close Tansport Layer  */
    transport_close(&cmds);

    /* Terminate the queues */
    queue_destroy(&handle_queue_receive_packets);

    /** Free the allocated memory */
    free_mem_pool(&mem_pool_hex_file_head);

    /** Close the log file  */
    if(handle_log_file.handle != NULL)
    {
        fileio_close(&handle_log_file);
    }

    printf("Exit Done!\n");

    return EXIT_SUCCESS;
}
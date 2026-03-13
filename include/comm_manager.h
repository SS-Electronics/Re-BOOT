
#ifndef __COMM_MANAGER_H__
#define __COMM_MANAGER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "app_types.h"
#include "threads.h"
#include "queues.h"
#include "transport_layer.h"


/**
 * @defgroup Global Variables and handles
 * @brief Portable thread abstraction
 * @{
 */

typedef struct
{
    uint32_t window_size;
    uint32_t in_flight;
    uint32_t next_seq;
    uint32_t ack_seq;

} sliding_window_t;




/** @} */ /* end of thread group */

#ifdef __cplusplus
}
#endif

#endif /* __COMM_MANAGER_H__ */
#ifndef __included_timer_h__
#define __included_timer_h__

#include "vppinfra/types.h"
#include "vppinfra/tw_timer_2t_1w_2048sl.h"

#include "plugins/flow_table/core/include/flow.h"
#include <stdio.h>

#define TIMER_SLOT_INTERVAL 1
#define FLOW_EXPIRARION_TIMER_ID 1

typedef tw_timer_wheel_2t_1w_2048sl_t flow_timer_wheel_t;

static inline void flow_expired_timers_dispatch (u32 * expired_timers)
{
    int i;
    u32 flow_index;

    for (i = 0; i < vec_len (expired_timers); i++) {
        flow_index = expired_timers[i] & 0x0FFFFFFF;
        printf("timeout flow index: %d\n", flow_index);
    }
}

static inline void flow_expiration_timer_set (flow_timer_wheel_t *tw, flow_t *flow, u64 seconds)
{
    flow->stop_timer_handle = tw_timer_start_2t_1w_2048sl(
        tw, flow->elt_index, FLOW_EXPIRARION_TIMER_ID, seconds);
}

static inline void flow_expiration_timer_reset (flow_timer_wheel_t * tw, flow_t * flow)
{
    if (flow->stop_timer_handle == ~0) {
        tw_timer_stop_2t_1w_2048sl(tw, flow->stop_timer_handle);
    }
}

static inline void flow_expiration_timer_update (flow_timer_wheel_t * tw, flow_t * flow, u64 seconds)
{
#if 1
    printf("update flow: %u seconds %lu\n", flow->elt_index, seconds);
#endif
    if (flow->stop_timer_handle == ~0) {
        flow_expiration_timer_set(tw, flow, seconds);
    } else {
        tw_timer_update_2t_1w_2048sl(tw, flow->stop_timer_handle, seconds);
    }
}

void flow_table_timer_pre_input_node_enable ();

#endif

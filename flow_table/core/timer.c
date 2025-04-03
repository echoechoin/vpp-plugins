#include "vlib/vlib.h"
#include "vlib/main.h"
#include "vppinfra/clib_error.h"
#include "plugins/flow_table/core/include/timer.h"

static inline uword
flow_table_timer_update_time (vlib_main_t * vm, vlib_node_runtime_t * node,
	vlib_frame_t * frame) {
	flow_table_main_t *ftm = &flow_table_main;
	u32 worker_id = vm->thread_index;
	flow_table_wrk_ctx_t *wrk_ctx = &ftm->wrk_ctx[worker_id];
	f64 now = vlib_time_now(vm);
	tw_timer_expire_timers_2t_1w_2048sl(&wrk_ctx->tw, now);
	return 0;
}

VLIB_REGISTER_NODE (flow_table_timer) = {
    .function = flow_table_timer_update_time,
    .type = VLIB_NODE_TYPE_PRE_INPUT,
    .name = "flow-table-timer-main",
};

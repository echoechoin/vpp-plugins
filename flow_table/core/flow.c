#include "vnet/vnet.h"
#include "vnet/plugin/plugin.h"
#include "vppinfra/clib_error.h"
#include "threads.h"

#include "vlib/cli.h"
#include "vlib/vlib.h"
#include "vlib/init.h"
#include "vppinfra/hash.h"
#include "vppinfra/pool.h"
#include "vppinfra/types.h"
#include "vppinfra/vec_bootstrap.h"
#include "vnet/ip/format.h"

#include "plugins/flow_table/core/include/flow.h"
#include "plugins/flow_table/core/include/protocol.h"
#include "plugins/flow_table/core/include/timer.h"

flow_table_main_t flow_table_main;

#define FLOW_TABLE_MAX_FLOWS 1000000

clib_error_t *flow_table_main_init(vlib_main_t *vm)
{
	clib_error_t *error = 0;
	flow_table_wrk_ctx_t *wrk;
	flow_table_main_t *ftm;
	u32 num_threads, prealloc_flows_per_wrk, n_workers;
	int thread;

	vlib_thread_main_t *vtm = vlib_get_thread_main ();
	ftm = &flow_table_main;

	num_threads = 1 /* main thread */  + vtm->n_threads;
	n_workers = num_threads == 1 ? 1 : vtm->n_threads;
	vec_validate (ftm->wrk_ctx, num_threads);
	prealloc_flows_per_wrk = FLOW_TABLE_MAX_FLOWS / n_workers;

	/* Initialize flow table for each worker */
	for (thread = 0; thread < num_threads; thread++) {
		wrk = &ftm->wrk_ctx[thread];
		wrk->vm = vlib_get_main_by_index (thread);

		/* init timer wheel */
		tw_timer_wheel_init_2t_1w_2048sl(&wrk->tw,
			flow_expired_timers_dispatch, 
			TIMER_SLOT_INTERVAL, ~0);
		wrk->tw.last_run_time = vlib_time_now (vm);
		
		/* Main thread will not be used if n_threads > 1 */
		if (num_threads > 1 && thread == 0)
			continue;

		pool_init_fixed(wrk->flows, prealloc_flows_per_wrk);
		memset(wrk->flows, 0, sizeof(flow_t) * prealloc_flows_per_wrk);
		wrk->flows_hash_table = hash_create(0, sizeof(flow_entry_t *));
	}

	return error;
}

u8 *format_flow_state (u8 * s, va_list * args) {
	flow_t *flow = va_arg (*args, flow_t *);
	u8 * state = (u8 *)protocol_handler_get(flow->protocol)->format_state(flow->state);
	s = format (s, "%s", state);
	return s;
}

u8 *format_flow_entry (u8 * s, va_list * args) {
	flow_t *flow = va_arg (*args, flow_t *);
	if (flow->ip_version == 4)
		s = format (s, "sw_if_index %d, src %U, dst %U, protocol %U, state %U\n",
					flow->sw_if_index, format_ip4_address, &flow->flow_entry[0].src.ip.ip4.as_u8,
					format_ip4_address, &flow->flow_entry[0].dst.ip.ip4.as_u8,
					format_ip_protocol, flow->protocol, format_flow_state, flow);
	else
		s = format (s, "sw_if_index %d, src %U, dst %U, protocol %U, state %U\n",
					flow->sw_if_index, format_ip6_address, &flow->flow_entry[0].src.ip.ip6,
					format_ip6_address, &flow->flow_entry[0].dst.ip.ip6,
					format_ip_protocol, flow->protocol, format_flow_state, flow);
	return s;
}

static clib_error_t *
flow_table_display_command_fn (vlib_main_t *vm,
							unformat_input_t *input,
							vlib_cli_command_t *cmd __attribute__((unused)))
{
	clib_error_t *error = 0;
	u32 num_threads;
	int thread;
	flow_table_main_t *ftm;
	flow_table_wrk_ctx_t *wrk;
	flow_t *flow;
	ftm = &flow_table_main;
	vlib_thread_main_t *vtm = vlib_get_thread_main ();
	num_threads = 1 /* main thread */  + vtm->n_threads;

	vlib_worker_thread_barrier_sync (vm);
	for (thread = 0; thread < num_threads; thread++) {
		wrk = &ftm->wrk_ctx[thread];
		/* Main thread will not be used if n_threads > 1 */
		if (thread == 0 && num_threads > 1)
			continue;

		vec_foreach (flow, wrk->flows) {
			if (flow->refcount == 0)
				continue;
			vlib_cli_output(vm, "%U", format_flow_entry, flow);
		}
	}
	vlib_worker_thread_barrier_release (vm);
	return error;
}

VLIB_CLI_COMMAND(flow_table_display_command, static) = {
	.path = "show flow-table",
	.short_help = "show flow-table",
	.function = flow_table_display_command_fn,
};

VLIB_MAIN_LOOP_ENTER_FUNCTION(flow_table_main_init);

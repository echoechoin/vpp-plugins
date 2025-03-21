#include <vnet/vnet.h>
#include <vnet/plugin/plugin.h>
#include <vppinfra/clib_error.h>

#include "include/flow.h"
#include "vppinfra/hash.h"
#include "vppinfra/pool.h"
#include "vppinfra/types.h"
#include "vppinfra/vec_bootstrap.h"

flow_table_main_t flow_table_main;

#define FLOW_TABLE_MAX_FLOWS 1000000

clib_error_t *flow_table_main_init(vlib_main_t *vm)
{
	clib_error_t *error = 0;
	flow_table_wrk_ctx_t *wrk;
	flow_table_main_t *ftm;
	u32 num_threads, prealloc_flows_per_wrk;
	int thread;

	vlib_thread_main_t *vtm = vlib_get_thread_main ();
	ftm = &flow_table_main;
	num_threads = vtm->n_threads;

	/* Initialize flow table for each worker */
  	vec_validate (ftm->wrk_ctx, num_threads);
	if (num_threads > 1)
		prealloc_flows_per_wrk = FLOW_TABLE_MAX_FLOWS / (num_threads - 1);
	else
		prealloc_flows_per_wrk = FLOW_TABLE_MAX_FLOWS;
	
	for (thread = 0; thread < vec_len(ftm->wrk_ctx); thread++) {
		wrk->vm = vlib_get_main_by_index (thread);
		wrk = &ftm->wrk_ctx[thread];
		if ((num_threads == 1) || (num_threads > 1 && thread != 0)) {
			pool_init_fixed(wrk->flows, prealloc_flows_per_wrk);
			wrk->flows_hash_table = hash_create(0, 0);
		}
	}

	return error;
}

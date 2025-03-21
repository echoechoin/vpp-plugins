
#include "vppinfra/types.h"
#include "vppinfra/vec.h"
#include "vnet/vnet.h"
#include "vnet/plugin/plugin.h"
#include "vlib/node_funcs.h"
#include "vpp/app/version.h"
#include "cpu_loading/cpu_loading.h"
#include <string.h>

cpu_loading_main_t cpu_loading_main;

/**
 * @brief  caculate cpu loading per <sampling_time> seconds
 **/
static uword cpu_loading_process (vlib_main_t * vm, vlib_node_runtime_t * rt, vlib_frame_t * f)
{
	cpu_loading_main_t *cpm = &cpu_loading_main;
	while (1) {
		vlib_process_wait_for_event_or_clock(vm, cpm->sampling_time);
		if (vlib_process_get_events(vm, 0) == 0) {
			break;
		}

		vlib_worker_thread_barrier_sync (vm);
		for (int i = 0; i < vlib_get_n_threads(); i++) {
			u64 total_idle_time = 0;
			for (int j = 0; j < POLLING_NODE_MAX; j++) {
				total_idle_time += cpm->idle_time[j][i];
				cpm->idle_time[j][i] = 0;
			}
			cpm->last_cpu_loading[i] = (float)100 - (float)((float)total_idle_time * 100.0 / (float)cpm->total_time[i]);
			cpm->total_time[i] = 0;
		}
		vlib_worker_thread_barrier_release (vm);
	}
	return 0;
}

/**
 * @brief statistics idle time and total time
 **/
static uword cpu_loading_time_statistics (
	struct vlib_main_t *vm, struct vlib_node_runtime_t *node, struct vlib_frame_t *frame)
{
	int i;
	uword n_vectors;
	vlib_node_t *n;
	u64 cpu_time_now, delta;
	cpu_loading_main_t *cpm = &cpu_loading_main;

	cpu_time_now = clib_cpu_time_now();
	n_vectors = node->function(vm, node, frame);
	delta = clib_cpu_time_now() - cpu_time_now;

	cpm->total_time[vm->thread_index] += delta;
	n = vlib_get_node(vm, node->node_index);

	/* Record idle time for polling node */
	if (n_vectors == 0) {
		for (i = 0; i < POLLING_NODE_MAX; i++) {
			if (cpm->polling_node_index[i][vm->thread_index] == n->index) {
				cpm->idle_time[i][vm->thread_index] += delta;
				break;
			}
		}
	}

	return n_vectors;
}

/**
 * @brief initialize cpu loading plugin
 **/
static clib_error_t *cpu_loading_init(vlib_main_t * vm)
{
	cpu_loading_main_t *cmp = &cpu_loading_main;
	clib_error_t *error = 0;
	cmp->vlib_main = vm;
	cmp->sampling_time = DEFAULT_SAMPLE_TIME;
	return error;
}

/**
 * @brief init memory; resgister node function decoration wrapper
 **/
static clib_error_t *cpu_loading_last_init(struct vlib_main_t * vm)
{
	int i, j;
	u32 vm_counts;
	cpu_loading_main_t *cmp = &cpu_loading_main;
	clib_error_t *error = 0;

	vm_counts = vlib_get_n_threads();
	vec_resize(cmp->total_time, vm_counts);
	vec_resize(cmp->last_cpu_loading, vm_counts);

	for (i = 0; i < POLLING_NODE_MAX; i++) {
		vec_resize(cmp->idle_time[i], vm_counts);
		vec_resize(cmp->polling_node_index[i], vm_counts);
	}

	for (i = 0; i < vm_counts; i++) {
		vlib_main_t *vm = vlib_get_main_by_index(i);
		vlib_node_t *polling_node;
		for (j = 0; j < POLLING_NODE_MAX; j++) {
			cmp->idle_time[j][i] = 0;
			polling_node = vlib_get_node_by_name(vm, (u8 *)poll_type_str[j]);
			if (polling_node)
				cmp->polling_node_index[j][i] = polling_node->index;
			else
				cmp->polling_node_index[j][i] = ~0;
		}

		cmp->total_time[i] = 0;
		cmp->last_cpu_loading[i] = 0;

		vlib_node_set_dispatch_wrapper(vm, cpu_loading_time_statistics);
	}
	return error;
}

VLIB_INIT_FUNCTION(cpu_loading_init);

VLIB_MAIN_LOOP_ENTER_FUNCTION(cpu_loading_last_init);

VLIB_REGISTER_NODE (cpu_loading_caculating_node) = {
  .function = cpu_loading_process,
  .type = VLIB_NODE_TYPE_PROCESS,
  .name = "cpu-loading-process",
};

VLIB_PLUGIN_REGISTER() = {
	.version = VPP_BUILD_VER,
	.description = "Caculate CPU loading",
};

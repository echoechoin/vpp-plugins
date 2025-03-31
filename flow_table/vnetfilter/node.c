#include <vlib/vlib.h>
#include <vnet/vnet.h>
#include <vnet/plugin/plugin.h>
#include <vnet/pg/pg.h>
#include <vppinfra/error.h>
#include <vnet/l2/l2_in_out_feat_arc.h>
#include <vlib/node_funcs.h>

#include "include/vnetfilter.h"
#include "include/hook.h"
#include "vnet/feature/feature.h"


typedef struct {
	u32 sw_if_index;
	u32 next_index;
} vnetfilter_hook_trace_t;

/**
 * @brief packet trace format function 
 *
 * TODO: add more details
 **/
static u8 * vnetfilter_format_hook_trace (u8 * s, va_list * args)
{
	CLIB_UNUSED (vlib_main_t * vm) = va_arg (*args, vlib_main_t *);
	CLIB_UNUSED (vlib_node_t * node) = va_arg (*args, vlib_node_t *);
	vnetfilter_hook_trace_t * t = va_arg (*args, vnetfilter_hook_trace_t *);

	s = format (s, "sw_if_index %d, next index %d\n",
				t->sw_if_index, t->next_index);
	return s;
}

#define vnetfilter_trace_buffer(b, n) do {						\
	if ((b)->flags & VLIB_BUFFER_IS_TRACED) {				    \
		vnetfilter_hook_trace_t *t =						    \
			vlib_add_trace (vm, node, (b), sizeof (*t));	    \
		t->sw_if_index = vnet_buffer(b)->sw_if_index[VLIB_RX];  \
		t->next_index = n;									    \
	}														    \
} while (0)

typedef enum {
	VNETFILTER_HOOK_NEXT_DROP,
	VNETFILTER_HOOK_NEXT_STOLEN,
	VNETFILTER_HOOK_N_NEXT,
} vnetfilter_hook_next_t;

/**
 * @brief Enable or disable a feature on an interface
 **/
void vnetfilter_hook_enable_disable (vnetfilter_main_t *fmp, u32 sw_if_index, char *arc_name, char *node_name,
		int enable_disable)
{
	if (pool_is_free_index (fmp->vnet_main->interface_main.sw_interfaces,
	sw_if_index))
		vlib_cli_output(fmp->vlib_main, "invalid interface name");

	if (0 != vnet_l2_feature_enable_disable (arc_name, node_name,
		sw_if_index, enable_disable, 0, 0))
		vlib_cli_output(fmp->vlib_main, "failed to enable/disable %s on %s",
				node_name, arc_name);
}

/**
 * @brief call all hook functions and process the action.
 **/
static inline void vnetfilter_hook_process(vnetfilter_hook_process_t *process_list, vlib_buffer_t *b, u16 *next)
{
	vnetfilter_action_t result = VNF_ACCEPT;

	for (int j = 0; j < vec_len(process_list); j++) {
		result = process_list[j].function(b);
		if (result != VNF_ACCEPT)
			break;
	}
	switch (result) {
	case VNF_DROP:
		next[0] = VNETFILTER_HOOK_NEXT_DROP;
		break;

	case VNF_STOLEN:
		next[0] = VNETFILTER_HOOK_NEXT_STOLEN;
		break;

	case VNF_ACCEPT:
	case VNF_STOP:
	default:
		vnet_feature_next_u16(next, b);
		break;
	}
}

/**
 * @brief node function for all hook nodes
 **/
always_inline uword
vnetfilter_hook_process_inline (vlib_main_t * vm,
	 		 vlib_node_runtime_t * node, vlib_frame_t * frame,
			 int is_trace, vnetfilter_hook_process_t *process_list)
{
	u32 n_left_from, *from;
	vlib_buffer_t *bufs[VLIB_FRAME_SIZE], **b;
	u16 nexts[VLIB_FRAME_SIZE], *next;

	from = vlib_frame_vector_args (frame);
	n_left_from = frame->n_vectors;

	vlib_get_buffers (vm, from, bufs, n_left_from);
	b = bufs;
	next = nexts;

	while (n_left_from >= 4) {
		/* Prefetch next iteration. */
		if (PREDICT_TRUE (n_left_from >= 8)) {
			vlib_prefetch_buffer_header (b[4], STORE);
			vlib_prefetch_buffer_header (b[5], STORE);
			vlib_prefetch_buffer_header (b[6], STORE);
			vlib_prefetch_buffer_header (b[7], STORE);
			CLIB_PREFETCH (b[4]->data, CLIB_CACHE_LINE_BYTES, STORE);
			CLIB_PREFETCH (b[5]->data, CLIB_CACHE_LINE_BYTES, STORE);
			CLIB_PREFETCH (b[6]->data, CLIB_CACHE_LINE_BYTES, STORE);
			CLIB_PREFETCH (b[7]->data, CLIB_CACHE_LINE_BYTES, STORE);
		}
		/* $$$$ process 4x pkts right here */
		next[0] = next[1] = next[2] = next[3] = 0;
		if (is_trace) {
			vnetfilter_trace_buffer(b[0], next[0]);
			vnetfilter_trace_buffer(b[1], next[1]);
			vnetfilter_trace_buffer(b[2], next[2]);
			vnetfilter_trace_buffer(b[3], next[3]);
		}

		vnetfilter_hook_process(process_list, b[0], &next[0]);
		vnetfilter_hook_process(process_list, b[1], &next[1]);
		vnetfilter_hook_process(process_list, b[2], &next[2]);
		vnetfilter_hook_process(process_list, b[3], &next[3]);

		b += 4;
		next += 4;
		n_left_from -= 4;
	}

	while (n_left_from > 0) {
		/* $$$$ process 1 pkt right here */
		next[0] = 0;
		if (is_trace)
			vnetfilter_trace_buffer(b[0], next[0]);
		vnetfilter_hook_process(process_list, b[0], next);
		b += 1;
		next += 1;
		n_left_from -= 1;
	}
	vlib_buffer_enqueue_to_next (vm, node, from, nexts, frame->n_vectors);
	return frame->n_vectors;
}

/**  Register all hook nodes **/
#define _(hook, _arc_name, _runs_before)										\
	vlib_node_registration_t hook##_node;										\
	VLIB_NODE_FN (hook##_node) (vlib_main_t * vm, vlib_node_runtime_t * node,	\
								vlib_frame_t * frame) {							\
		if (PREDICT_FALSE (node->flags & VLIB_NODE_FLAG_TRACE))					\
			return vnetfilter_hook_process_inline (vm, node, frame,				\
						1 /* is_trace */ , hook##_hook_process_list);			\
		else																	\
			return vnetfilter_hook_process_inline (vm, node, frame,				\
						0 /* is_trace */ , hook##_hook_process_list);			\
	}																		 	\
	VLIB_REGISTER_NODE (hook##_node) = {									 	\
		.name = _arc_name "-hook",												\
		.vector_size = sizeof (u32),											\
		.format_trace = vnetfilter_format_hook_trace,							\
		.type = VLIB_NODE_TYPE_INTERNAL,										\
		.n_next_nodes = VNETFILTER_HOOK_N_NEXT,									\
		.next_nodes = {													 		\
			[VNETFILTER_HOOK_NEXT_STOLEN] = "vnetfilter-stolen",				\
			[VNETFILTER_HOOK_NEXT_DROP] = "error-drop",							\
		},																		\
	};																			\
	VNET_FEATURE_INIT (hook##_hook, static) = {								 	\
		.arc_name = (_arc_name),												\
		.node_name = _arc_name "-hook",											\
		.runs_before = VNET_FEATURES (_runs_before),							\
	};

foreach_vnetfilter_hook
#undef _

/** if packet has been stolen, trace it only **/
vlib_node_registration_t vnetfilter_stolen_node;
VLIB_NODE_FN(vnetfilter_stolen_node) (vlib_main_t * vm, vlib_node_runtime_t * node,
									  vlib_frame_t * frame)
{
	u32 n_left_from, *from;
	vlib_buffer_t *bufs[VLIB_FRAME_SIZE], **b;
	u16 nexts[VLIB_FRAME_SIZE], *next;

	from = vlib_frame_vector_args (frame);
	n_left_from = frame->n_vectors;

	vlib_get_buffers (vm, from, bufs, n_left_from);
	b = bufs;
	next = nexts;
	while (n_left_from >= 4) {
		/* Prefetch next iteration. */
		if (PREDICT_TRUE (n_left_from >= 8)) {
			vlib_prefetch_buffer_header (b[4], STORE);
			vlib_prefetch_buffer_header (b[5], STORE);
			vlib_prefetch_buffer_header (b[6], STORE);
			vlib_prefetch_buffer_header (b[7], STORE);
			CLIB_PREFETCH (b[4]->data, CLIB_CACHE_LINE_BYTES, STORE);
			CLIB_PREFETCH (b[5]->data, CLIB_CACHE_LINE_BYTES, STORE);
			CLIB_PREFETCH (b[6]->data, CLIB_CACHE_LINE_BYTES, STORE);
			CLIB_PREFETCH (b[7]->data, CLIB_CACHE_LINE_BYTES, STORE);
		}
		if (PREDICT_FALSE (node->flags & VLIB_NODE_FLAG_TRACE)) {
			vnetfilter_trace_buffer(b[0], next[0]);
			vnetfilter_trace_buffer(b[1], next[1]);
			vnetfilter_trace_buffer(b[2], next[2]);
			vnetfilter_trace_buffer(b[3], next[3]);
		}
		b += 4;
		next += 4;
		n_left_from -= 4;
	}

	while (n_left_from > 0) {
		/* $$$$ process 1 pkt right here */
		next[0] = 0;

		if (PREDICT_FALSE (node->flags & VLIB_NODE_FLAG_TRACE))
			vnetfilter_trace_buffer(b[0], next[0]);
		b += 1;
		next += 1;
		n_left_from -= 1;
	}
	return 0;
}

VLIB_REGISTER_NODE (vnetfilter_stolen_node) = {
	.name = "vnetfilter-stolen",
	.vector_size = sizeof (u32),
	.type = VLIB_NODE_TYPE_INTERNAL,
	.n_next_nodes = 0,
};

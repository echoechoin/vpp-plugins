#include <vlib/vlib.h>
#include <vnet/vnet.h>
#include <vnet/plugin/plugin.h>
#include <vnet/pg/pg.h>
#include <vppinfra/error.h>
#include <vnet/l2/l2_in_out_feat_arc.h>

#include "include/flow_table.h"
#include "include/node.h"

typedef struct {
    u32 sw_if_index;
    u32 next_index;
} flow_table_hook_trace_t;

/* packet trace format function */
static u8 * format_flow_table_hook_trace (u8 * s, va_list * args)
{
    CLIB_UNUSED (vlib_main_t * vm) = va_arg (*args, vlib_main_t *);
    CLIB_UNUSED (vlib_node_t * node) = va_arg (*args, vlib_node_t *);
    flow_table_hook_trace_t * t = va_arg (*args, flow_table_hook_trace_t *);
    
    s = format (s, "L2_INPUT_IP4_HOOK: sw_if_index %d, next index %d\n",
                t->sw_if_index, t->next_index);
    return s;
}

typedef enum {
    FLOW_TABLE_HOOK_NEXT_DROP,
    FLOW_TABLE_HOOK_N_NEXT,
} flow_table_hook_next_t;

/**
 * @brief Enable or disable a feature on an interface
 **/
void flow_table_hook_enable_disable (flow_table_main_t *fmp, u32 sw_if_index, char *arc_name, char *node_name,
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
 * @brief Hook node function
 **/
always_inline uword
flow_table_hook_inline (vlib_main_t * vm,
     		 vlib_node_runtime_t * node, vlib_frame_t * frame,
		 int is_ip4, int is_trace, flow_table_hook_t hook)
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
        next[0] = 0;
        next[1] = 0;
        next[2] = 0;
        next[3] = 0;

        if (is_trace) {
            if (b[0]->flags & VLIB_BUFFER_IS_TRACED) {
                flow_table_hook_trace_t *t = 
                vlib_add_trace (vm, node, b[0], sizeof (*t));
                t->next_index = next[0];
                    t->sw_if_index = vnet_buffer(b[0])->sw_if_index[VLIB_RX];
                }
            if (b[1]->flags & VLIB_BUFFER_IS_TRACED) {
                flow_table_hook_trace_t *t = 
                    vlib_add_trace (vm, node, b[1], sizeof (*t));
                t->next_index = next[1];
                    t->sw_if_index = vnet_buffer(b[1])->sw_if_index[VLIB_RX];
            }
            if (b[2]->flags & VLIB_BUFFER_IS_TRACED) {
                flow_table_hook_trace_t *t = 
                    vlib_add_trace (vm, node, b[2], sizeof (*t));
                t->next_index = next[2];
                    t->sw_if_index = vnet_buffer(b[2])->sw_if_index[VLIB_RX];
            }
	        if (b[3]->flags & VLIB_BUFFER_IS_TRACED) {
                flow_table_hook_trace_t *t = 
                    vlib_add_trace (vm, node, b[3], sizeof (*t));
                t->next_index = next[3];
                    t->sw_if_index = vnet_buffer(b[3])->sw_if_index[VLIB_RX];
	        }
	    }

        b += 4;
        next += 4;
        n_left_from -= 4;
    }   

    while (n_left_from > 0) {
        /* $$$$ process 1 pkt right here */
        next[0] = 0;

        if (is_trace) {
            if (b[0]->flags & VLIB_BUFFER_IS_TRACED) {
                flow_table_hook_trace_t *t = 
                    vlib_add_trace (vm, node, b[0], sizeof (*t));
                t->next_index = next[0];
                    t->sw_if_index = vnet_buffer(b[0])->sw_if_index[VLIB_RX];
            }
        }

        b += 1;
        next += 1;
        n_left_from -= 1;
    }

    vlib_buffer_enqueue_to_next (vm, node, from, nexts, frame->n_vectors);

    return frame->n_vectors;
}

/**  Register all hook nodes **/
#define _(hook, _arc_name, _runs_before)                                      \
    vlib_node_registration_t hook##_node;                                     \
    VLIB_NODE_FN (hook##_node) (vlib_main_t * vm, vlib_node_runtime_t * node, \
                                vlib_frame_t * frame) {                       \
        if (PREDICT_FALSE (node->flags & VLIB_NODE_FLAG_TRACE))               \
            return flow_table_hook_inline (vm, node, frame, 1 /* is_ip4 */ ,  \
                        1 /* is_trace */ , FLOW_TABLE_HOOK_##hook);           \
        else                                                                  \
            return flow_table_hook_inline (vm, node, frame, 1 /* is_ip4 */ ,  \
                        0 /* is_trace */ , FLOW_TABLE_HOOK_##hook);           \
    }                                                                         \
    VLIB_REGISTER_NODE (hook##_node) = {                                      \
        .name = _arc_name "-hook",                                            \
        .vector_size = sizeof (u32),                                          \
        .format_trace = format_flow_table_hook_trace,                         \
        .type = VLIB_NODE_TYPE_INTERNAL,                                      \
        .n_next_nodes = FLOW_TABLE_HOOK_N_NEXT,                               \
        .next_nodes = {                                                       \
            [FLOW_TABLE_HOOK_NEXT_DROP] = "error-drop",                       \
        },                                                                    \
    };                                                                        \
    VNET_FEATURE_INIT (hook##_hook, static) = {                               \
        .arc_name = (_arc_name),                                              \
        .node_name = _arc_name "-hook",                                       \
        .runs_before = VNET_FEATURES (_runs_before),                          \
    };

foreach_flow_table_hook
#undef _

#ifndef __included_node_h__
#define __included_node_h__

#include "flow_table.h"

/** flow table hook list: _(hook_node, arc_name, runs_after) **/
#define foreach_flow_table_hook                                 \
    _(l2_input_ip4, "l2-input-ip4", "l2-input-feat-arc-end")    \
    _(l2_input_ip6, "l2-input-ip6", "l2-input-feat-arc-end")    \
    _(l2_output_ip4, "l2-output-ip4", "l2-output-feat-arc-end") \
    _(l2_output_ip6, "l2-output-ip6", "l2-output-feat-arc-end") \

typedef enum {
#define _(a, b, c) FLOW_TABLE_HOOK_##a,
    foreach_flow_table_hook
#undef _
    FLOW_TABLE_N_HOOK,
} flow_table_hook_t;

void flow_table_hook_enable_disable (flow_table_main_t *fmp, u32 sw_if_index, char *arc_name, char *node_name,
        int enable_disable);

#endif /* __included_node_h__ */

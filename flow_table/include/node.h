#ifndef __included_node_h__
#define __included_node_h__

#define foreach_flow_table_hook \
    _(l2_input_ip4, "l2-input-ip4", "l2-input-feat-arc-end") \
    _(l2_input_ip6, "l2-input-ip6", "l2-input-feat-arc-end") \
    _(l2_output_ip4, "l2-output-ip4", "l2-output-feat-arc-end") \
    _(l2_output_ip6, "l2-output-ip6", "l2-output-feat-arc-end") \

typedef enum {
#define _(a, b, c) FLOW_TABLE_HOOK_##a,
    foreach_flow_table_hook
#undef _
    FLOW_TABLE_N_HOOK,
} flow_table_hook_t;

#endif /* __included_node_h__ */

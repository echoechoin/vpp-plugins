#ifndef __included_hook_h__
#define __included_hook_h__

#include <vnet/vnet.h>
#include "vnetfilter.h"

typedef vnetfilter_action_t (*vnetfilter_hook_function)(vlib_buffer_t *);

typedef struct {
	u32 priority;
	vnetfilter_hook_function function;
} vnetfilter_hook_process_t;

/** flow table hook list: _(hook_node, arc_name, runs_after) **/
#define foreach_vnetfilter_hook								 	\
	_(l2_input_ip4, "l2-input-ip4", "l2-input-feat-arc-end")	\
	_(l2_input_ip6, "l2-input-ip6", "l2-input-feat-arc-end")	\
	_(l2_output_ip4, "l2-output-ip4", "l2-output-feat-arc-end") \
	_(l2_output_ip6, "l2-output-ip6", "l2-output-feat-arc-end") \

#define _(hook, _arc_name, _runs_after) \
	extern vnetfilter_hook_process_t *hook##_hook_process_list; \
	int hook##_hook_register(vnetfilter_hook_process_t *h);	    \

foreach_vnetfilter_hook
#undef _

void vnetfilter_hook_enable_disable (vnetfilter_main_t *fmp, u32 sw_if_index, char *arc_name, char *node_name,
		int enable_disable);

#endif /* __included_hook_h__ */

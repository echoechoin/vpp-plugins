#ifndef __included_process_h__
#define __included_process_h__

#include "flow_table/vnetfilter/include/vnetfilter.h"

#define for_each_flow_table_hook			\
	_(0, ip4_pre_process, l2_input_ip4)		\
	_(0, ip4_input_process, l2_input_ip4)	\
	_(0, ip4_output_process, l2_output_ip4)	\
	_(0, ip6_pre_process, l2_input_ip6)		\
	_(0, ip6_input_process, l2_input_ip6)	\
	_(0, ip6_output_process, l2_output_ip6)	\

#define _(a, b, c) \
	vnetfilter_action_t b(vlib_buffer_t *b);
for_each_flow_table_hook
#undef _

#endif

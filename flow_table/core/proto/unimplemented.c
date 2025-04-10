#include <stdarg.h>
#include "vlib/vlib.h"
#include "plugins/flow_table/core/include/protocol.h"

static vnetfilter_action_t umimplemented_parse_flow_key(vlib_buffer_t *b, flow_key_t *key)
{
	return VNF_ACCEPT;
}

static vnetfilter_action_t umimplemented_init_state(vlib_buffer_t *b, flow_dir_t direction)
{
	return VNF_ACCEPT;
}

static vnetfilter_action_t umimplemented_update_state(vlib_buffer_t *b, flow_dir_t direction)
{
	return VNF_ACCEPT;
}

static u8 *umimplemented_flow_format(u8 *s, va_list *args)
{
	flow_t *flow = va_arg (*args, flow_t *);
	if (flow->ip_version == 4)
		s = format (s, "sw_if_index %d %d, src %U, dst %U, protocol %U\n",
				flow->flow_entry[0].sw_if_index, flow->flow_entry[1].sw_if_index,
				format_ip4_address, &flow->flow_entry[0].src.ip.ip4.as_u8,
				format_ip4_address, &flow->flow_entry[0].dst.ip.ip4.as_u8,
				format_ip_protocol, flow->protocol);
	else
		s = format (s, "sw_if_index %d %d, src %U, dst %U, protocol %U\n",
				flow->flow_entry[0].sw_if_index, flow->flow_entry[1].sw_if_index,
				format_ip6_address, &flow->flow_entry[0].src.ip.ip6,
				format_ip6_address, &flow->flow_entry[0].dst.ip.ip6,
				format_ip_protocol, flow->protocol);
	return s;
}

static protocol_handler_t unimplemented_protocol = {
	.parse_key = umimplemented_parse_flow_key,
	.init_state = umimplemented_init_state,
	.update_state = umimplemented_update_state,
	.format_flow = umimplemented_flow_format,
};

static clib_error_t *unimplemented_protocol_handler_register(vlib_main_t * vm)
{
	/* Register unimplemented protocol handler for all protocols firstly */
	for (int i = 0; i < IP_PROTOCOL_RESERVED; i++) {
		protocol_handler_register(i, &unimplemented_protocol);
	}
	return 0;
}

VLIB_INIT_FUNCTION(unimplemented_protocol_handler_register);

#include <stdarg.h>
#include "vlib/vlib.h"
#include "vlib/init.h"
#include "vnet/ip/ip_packet.h"
#include "vnet/tcp/tcp_packet.h"

#include "plugins/flow_table/core/include/protocol.h"

static vnetfilter_action_t udp_parse_flow_key(vlib_buffer_t *b, flow_key_t *key)
{
	return VNF_ACCEPT;
}

static vnetfilter_action_t udp_init_state(vlib_buffer_t *b, flow_dir_t direction)
{
	return VNF_ACCEPT;
}

static vnetfilter_action_t udp_update_state(vlib_buffer_t *b, flow_dir_t direction)
{
	return VNF_ACCEPT;
}

static u8 *udp_flow_format(u8 *s, va_list *args)
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

static protocol_handler_t udp_protocol = {
	.parse_key = udp_parse_flow_key,
	.init_state = udp_init_state,
	.update_state = udp_update_state,
	.format_flow = udp_flow_format,
};

static clib_error_t *udp_protocol_handler_register(vlib_main_t * vm)
{
	clib_error_t *error = 0;
	protocol_handler_register(IP_PROTOCOL_UDP, &udp_protocol);
	return error;
}

VLIB_MAIN_LOOP_ENTER_FUNCTION(udp_protocol_handler_register);

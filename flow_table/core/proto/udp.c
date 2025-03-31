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

static const char *udp_format_state(int state)
{
	return NULL;
}

static protocol_handler_t udp_protocol = {
	.parse_key = udp_parse_flow_key,
	.init_state = udp_init_state,
	.update_state = udp_update_state,
	.format_state = udp_format_state,
};

static clib_error_t *udp_protocol_handler_register(vlib_main_t * vm)
{
	clib_error_t *error = 0;
	protocol_handler_register(IP_PROTOCOL_UDP, &udp_protocol);
	return error;
}

VLIB_MAIN_LOOP_ENTER_FUNCTION(udp_protocol_handler_register);

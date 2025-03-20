#include "../include/protocol.h"
#include "vlib/init.h"
#include "vnet/ip/ip_packet.h"

static vnetfilter_action_t tcp_parse_flow_key(vlib_buffer_t *b, flow_key_t *key, bool reverse)
{
	return VNF_ACCEPT;
}

static vnetfilter_action_t tcp_init_state(vlib_buffer_t *b, flow_key_t *key)
{
	return VNF_ACCEPT;
}

static vnetfilter_action_t tcp_update_state(vlib_buffer_t *b, flow_key_t *key)
{
	return VNF_ACCEPT;
}

static protocol_t tcp_protocol = {
	.parse_key = tcp_parse_flow_key,
	.init_state = tcp_init_state,
	.update_state = tcp_update_state,
};

static clib_error_t *tcp_protocol_register(vlib_main_t * vm)
{
	clib_error_t *error = 0;
	protocol_register(IP_PROTOCOL_TCP, &tcp_protocol);
	return error;
}

VLIB_MAIN_LOOP_ENTER_FUNCTION(tcp_protocol_register);
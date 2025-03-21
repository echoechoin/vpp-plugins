#include "../include/protocol.h"
#include "vlib/init.h"

static vnetfilter_action_t icmp_parse_flow_key(vlib_buffer_t *b, flow_key_t *key)
{
	return VNF_ACCEPT;
}

static vnetfilter_action_t icmp_init_state(vlib_buffer_t *b, flow_dir_t direction)
{
	return VNF_ACCEPT;
}

static vnetfilter_action_t icmp_update_state(vlib_buffer_t *b, flow_dir_t direction)
{
	return VNF_ACCEPT;
}

static protocol_handler_t icmp_protocol = {
	.parse_key = icmp_parse_flow_key,
	.init_state = icmp_init_state,
	.update_state = icmp_update_state,
};

static clib_error_t *icmp_protocol_handler_register(vlib_main_t * vm)
{
	clib_error_t *error = 0;
	protocol_handler_register(IP_PROTOCOL_ICMP, &icmp_protocol);
	return error;
}

VLIB_MAIN_LOOP_ENTER_FUNCTION(icmp_protocol_handler_register);

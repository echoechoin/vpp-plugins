#include "../include/protocol.h"

static vnetfilter_action_t umimplemented_parse_flow_key(vlib_buffer_t *b, flow_key_t *key)
{
	return VNF_ACCEPT;
}

static vnetfilter_action_t umimplemented_init_state(vlib_buffer_t *b)
{
	return VNF_ACCEPT;
}

static vnetfilter_action_t umimplemented_update_state(vlib_buffer_t *b)
{
	return VNF_ACCEPT;
}

static protocol_handler_t unimplemented_protocol = {
	.parse_key = umimplemented_parse_flow_key,
	.init_state = umimplemented_init_state,
	.update_state = umimplemented_update_state,
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

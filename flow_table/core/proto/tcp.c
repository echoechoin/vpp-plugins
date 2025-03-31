#include <stdbool.h>
#include <arpa/inet.h>

#include "vlib/init.h"
#include "vnet/ip/ip_packet.h"
#include "vnet/tcp/tcp_packet.h"

#include "plugins/flow_table/core/include/flow.h"
#include "plugins/flow_table/core/include/protocol.h"
#include "tcp_state_transfer.h"

static vnetfilter_action_t tcp_parse_flow_key(vlib_buffer_t *b, flow_key_t *key)
{
	tcp_header_t *th = (tcp_header_t *)(b->data + vnet_buffer(b)->l4_hdr_offset);
	key->src_port = htons(th->src_port);
	key->dst_port = htons(th->dst_port);
	return VNF_ACCEPT;
}

static vnetfilter_action_t tcp_init_state(vlib_buffer_t *b, flow_dir_t direction)
{
	u8 flags;
	tcp_state_t init_state; 
	tcp_event_t event;
	tcp_header_t *th;
	flow_t *flow;

	th = (tcp_header_t *)(b->data + vnet_buffer(b)->l4_hdr_offset);
	flow = vlib_buffer_get_flow(b);

	flags = th->flags & TCP_FLAG_CONCERNED;
	if (flags == TCP_FLAG_SYN)
		flow->three_way_handshake = true;

	init_state = TCP_STATE_NONE;
	event = flags_to_events(flags);
	
	/* Set initial state */
	flow->state = tcp_state_transfer(init_state, event, direction);

	return VNF_ACCEPT;
}

static vnetfilter_action_t tcp_update_state(vlib_buffer_t *b, flow_dir_t direction)
{
	u8 flags;
	tcp_event_t event;
	tcp_header_t *th;
	flow_t *flow;

	th = (tcp_header_t *)(b->data + vnet_buffer(b)->l4_hdr_offset);
	flow = vlib_buffer_get_flow(b);

	flags = th->flags & TCP_FLAG_CONCERNED;
	event = flags_to_events(flags);

	/* Update flow state */
	flow->state = tcp_state_transfer(flow->state, event, direction);

	return VNF_ACCEPT;
}

static const char *tcp_format_state(int state)
{
	return tcp_state_name[state];
}

static protocol_handler_t tcp_protocol = {
	.parse_key = tcp_parse_flow_key,
	.init_state = tcp_init_state,
	.update_state = tcp_update_state,
	.format_state = tcp_format_state,
};

static clib_error_t *tcp_protocol_handler_register(vlib_main_t * vm)
{
	clib_error_t *error = 0;
	protocol_handler_register(IP_PROTOCOL_TCP, &tcp_protocol);
	return error;
}

VLIB_MAIN_LOOP_ENTER_FUNCTION(tcp_protocol_handler_register);

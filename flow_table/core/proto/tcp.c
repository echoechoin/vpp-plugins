#include <stdarg.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <string.h>

#include "vlib/vlib.h"
#include "vlib/init.h"
#include "vnet/ethernet/packet.h"
#include "vnet/interface.h"
#include "vnet/ip/ip_packet.h"
#include "vnet/tcp/tcp_packet.h"
#include "vnet/buffer.h"
#include "vnet/vnet.h"
#include "vppinfra/pool.h"

#include "plugins/flow_table/core/include/flow.h"
#include "plugins/flow_table/core/include/protocol.h"
#include "plugins/flow_table/core/include/timer.h"
#include "plugins/flow_table/core/proto/tcp_state_transfer.h"
#include "plugins/flow_table/core/proto/tcp_control.h"


#define TCP_DEBUG 1

static vnetfilter_action_t tcp_parse_flow_key(vlib_buffer_t *b, flow_key_t *key)
{
	tcp_header_t *th = (tcp_header_t *)(b->data + vnet_buffer(b)->l4_hdr_offset);
	key->src_port = ntohs(th->src_port);
	key->dst_port = ntohs(th->dst_port);
	return VNF_ACCEPT;
}

static vnetfilter_action_t tcp_track_sequence(vlib_buffer_t *b, flow_dir_t direction) {
	flow_t *flow;
	tcp_header_t *th;

	flow = vlib_buffer_get_flow(b);
	th = (tcp_header_t *)(b->data + vnet_buffer(b)->l4_hdr_offset);

	flow->flow_entry[direction].tcp_attr.seq = ntohs(th->seq_number);
	flow->flow_entry[direction].tcp_attr.ack = ntohs(th->ack_number);
	return VNF_ACCEPT;
}

static vnetfilter_action_t tcp_init_state(vlib_buffer_t *b, flow_dir_t direction)
{
	u8 flags;
	tcp_state_t init_state; 
	tcp_event_t event;
	tcp_header_t *th;
	flow_t *flow;
	flow_table_wrk_ctx_t *wrk = flow_table_get_worker_ctx();

	th = (tcp_header_t *)(b->data + vnet_buffer(b)->l4_hdr_offset);
	flow = vlib_buffer_get_flow(b);
	flags = th->flags & TCP_FLAG_CONCERNED;
	if (flags == TCP_FLAG_SYN)
		flow->three_way_handshake = true;

	init_state = TCP_STATE_START;
	event = flags_to_events(flags);
	
#if TCP_DEBUG
	printf("last state: %s\n", tcp_state_name[flow->state]);
#endif
	/* Set initial state */
	flow->state = tcp_state_transfer(init_state, event, direction);
	flow_expiration_timer_update(&wrk->tw, flow, tcp_state_timeout[flow->state]);

	tcp_track_sequence(b, direction);

#if TCP_DEBUG
	printf("events: %d\n", event);
	printf("init flow %p %u state %s\n", flow, flow->elt_index, tcp_state_name[flow->state]);
#endif

	return VNF_ACCEPT;
}

static vnetfilter_action_t tcp_update_state(vlib_buffer_t *b, flow_dir_t direction)
{
	u8 flags;
	tcp_event_t event;
	tcp_header_t *th;
	flow_t *flow;
	flow_table_wrk_ctx_t *wrk = flow_table_get_worker_ctx();

	th = (tcp_header_t *)(b->data + vnet_buffer(b)->l4_hdr_offset);
	flow = vlib_buffer_get_flow(b);
	flags = th->flags & TCP_FLAG_CONCERNED;
	event = flags_to_events(flags);

#if TCP_DEBUG
	printf("last state: %s\n", tcp_state_name[flow->state]);
#endif
	/* Update flow state */
	flow->state = tcp_state_transfer(flow->state, event, direction);
	flow_expiration_timer_update(&wrk->tw, flow, tcp_state_timeout[flow->state]);

	tcp_track_sequence(b, direction);

#if TCP_DEBUG
	printf("events: %d\n", event);
	printf("update flow %p %u state %s\n", flow, flow->elt_index, tcp_state_name[flow->state]);
#endif

	tcp_send_reset(flow);
	return VNF_ACCEPT;
}

static u8 *tcp_flow_format(u8 *s, va_list *args)
{
	flow_t *flow = va_arg (*args, flow_t *);
	if (flow->ip_version == 4)
		s = format (s, "sw_if_index %d %d, src %U, dst %U, protocol %U, state %s\n"
					"  seq[0] %u, ack[0] %u, seq[1] %u, ack[1] %u\n",
					flow->flow_entry[0].sw_if_index, flow->flow_entry[1].sw_if_index,
					format_ip4_address, &flow->flow_entry[0].src.ip.ip4.as_u8,
					format_ip4_address, &flow->flow_entry[0].dst.ip.ip4.as_u8,
					format_ip_protocol, flow->protocol, tcp_state_name[flow->state],
					flow->flow_entry[0].tcp_attr.seq, flow->flow_entry[0].tcp_attr.ack,
					flow->flow_entry[1].tcp_attr.seq, flow->flow_entry[1].tcp_attr.ack
				);
	else
		s = format (s, "sw_if_index %d %d, src %U, dst %U, protocol %U, state %s\n",
					"  seq[0] %u, ack[0] %u, seq[1] %u, ack[1] %u\n",
					flow->flow_entry[0].sw_if_index, flow->flow_entry[1].sw_if_index,
					format_ip6_address, &flow->flow_entry[0].src.ip.ip6,
					format_ip6_address, &flow->flow_entry[0].dst.ip.ip6,
					format_ip_protocol, flow->protocol, tcp_state_name[flow->state],
					flow->flow_entry[0].tcp_attr.seq, flow->flow_entry[0].tcp_attr.ack,
					flow->flow_entry[1].tcp_attr.seq, flow->flow_entry[1].tcp_attr.ack
				);
	return s;
}

static protocol_handler_t tcp_protocol = {
	.parse_key = tcp_parse_flow_key,
	.init_state = tcp_init_state,
	.update_state = tcp_update_state,
	.format_flow = tcp_flow_format,
};

static clib_error_t *tcp_protocol_handler_register(vlib_main_t * vm)
{
	clib_error_t *error = 0;
	protocol_handler_register(IP_PROTOCOL_TCP, &tcp_protocol);
	return error;
}

VLIB_MAIN_LOOP_ENTER_FUNCTION(tcp_protocol_handler_register);

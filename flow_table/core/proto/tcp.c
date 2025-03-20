#include "../include/protocol.h"
#include "plugins/flow_table/core/include/flow.h"
#include "vlib/init.h"
#include "vnet/ip/ip_packet.h"
#include "vnet/tcp/tcp_packet.h"

#define for_each_tcp_state					\
	_(NONE,			"none",			10) 	\
	_(CLOSE,		"close",		10)		\
	_(SYN_SENT,		"syn-sent",		10)		\
	_(SYN_RECV,		"syn-recv",		10)		\
	_(ESTABLISHED,	"established",	120)	\
	_(FIN_WAIT,		"fin-wait",		20)		\
	_(CLOSE_WAIT,	"close-wait",	20)		\
	_(TIME_WAIT,	"time-wait",	5)		\
	_(UNKNOWN,		"unknown",		100)    \

typedef enum {
#define _(a, b, c) TPC_STATE_##a,
	for_each_tcp_state
#undef _
} tcp_state_t;

__attribute__((unused))
static const u32 tcp_state_timeout[] = {
#define _(a, b, c) c,
	for_each_tcp_state
#undef _
};

__attribute__((unused))
static const char *tcp_state_name[] = {
#define _(a, b, c) "tcp-state-" b,
	for_each_tcp_state
#undef _
};

static vnetfilter_action_t tcp_parse_flow_key(vlib_buffer_t *b, flow_key_t *key)
{
	tcp_header_t *th = (tcp_header_t *)b->data + vnet_buffer(b)->l4_hdr_offset;
	key->src_port = th->src_port;
	key->dst_port = th->dst_port;
	return VNF_ACCEPT;
}

static vnetfilter_action_t tcp_init_state(vlib_buffer_t *b)
{
	tcp_header_t *th = (tcp_header_t *)b->data + vnet_buffer(b)->l4_hdr_offset;
	flow_t *flow = vlib_buffer_get_flow(b);
	u8 flags = th->flags;
	u8 state = TPC_STATE_NONE;

	if (flags == TCP_FLAG_SYN)
		state = TPC_STATE_SYN_SENT;
	else if (flags == (TCP_FLAG_SYN | TCP_FLAG_ACK))
		state = TPC_STATE_SYN_RECV;
	else if (flags == TCP_FLAG_ACK)
		state = TPC_STATE_ESTABLISHED;
	else if (flags == TCP_FLAG_FIN)
		state = TPC_STATE_FIN_WAIT;
	else if (flags == (TCP_FLAG_FIN | TCP_FLAG_ACK))
		state = TPC_STATE_CLOSE_WAIT;
	else if (flags == TCP_FLAG_RST)
		state = TPC_STATE_CLOSE;
	else
		state = TPC_STATE_UNKNOWN; /* for example: no any flags */

	flow->state = state;
	return VNF_ACCEPT;
}

static vnetfilter_action_t tcp_update_state(vlib_buffer_t *b)
{
	return VNF_ACCEPT;
}

static protocol_handler_t tcp_protocol = {
	.parse_key = tcp_parse_flow_key,
	.init_state = tcp_init_state,
	.update_state = tcp_update_state,
};

static clib_error_t *tcp_protocol_handler_register(vlib_main_t * vm)
{
	clib_error_t *error = 0;
	protocol_handler_register(IP_PROTOCOL_TCP, &tcp_protocol);
	return error;
}

VLIB_MAIN_LOOP_ENTER_FUNCTION(tcp_protocol_handler_register);
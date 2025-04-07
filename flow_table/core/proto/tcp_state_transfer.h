#ifndef __included_tcp_state_transfer_h__
#define __included_tcp_state_transfer_h__

#include <stdbool.h>

#include "plugins/flow_table/core/include/flow.h"
#include "plugins/flow_table/core/include/protocol.h"

/*	ori_state		event		dir		new_state      */
#define for_each_tcp_state_transfer_entry 				\
	_(START,		UNKNOWN,	IN,		UNKNOWN) 		\
	_(START,		SYN,		IN,		SYN_SENT) 		\
	_(START,		ACK,		IN,		ESTABLISHED)	\
	_(START,		FIN,		IN,		CLOSE) 			\
	_(START,		RST,		IN,		CLOSE) 			\
	_(START,		UNKNOWN,	OUT,	UNKNOWN) 		\
	_(START,		SYN,		OUT,	SYN_SENT) 		\
	_(START,		ACK,		OUT,	ESTABLISHED)	\
	_(START,		FIN,		OUT,	CLOSE) 			\
	_(START,		RST,		OUT,	CLOSE) 			\
														\
	_(CLOSE,		UNKNOWN,	IN,		UNKNOWN)	 	\
	_(CLOSE,		SYN,		IN,		SYN_SENT)		\
	_(CLOSE,		ACK,		IN,		ESTABLISHED)	\
	_(CLOSE,		FIN,		IN,		CLOSE)	 		\
	_(CLOSE,		RST,		IN,		CLOSE)	 		\
	_(CLOSE,		UNKNOWN,	OUT,	UNKNOWN)	 	\
	_(CLOSE,		SYN,		OUT,	SYN_SENT)		\
	_(CLOSE,		ACK,		OUT,	ESTABLISHED)	\
	_(CLOSE,		FIN,		OUT,	CLOSE)	 		\
	_(CLOSE,		RST,		OUT,	CLOSE)	 		\
														\
	_(SYN_SENT,		UNKNOWN,	IN,		UNKNOWN) 		\
	_(SYN_SENT,		SYN,		IN,		SYN_RCVD) 		\
	_(SYN_SENT,		ACK,		IN,		ESTABLISHED)	\
	_(SYN_SENT,		FIN,		IN,		CLOSE) 			\
	_(SYN_SENT,		RST,		IN,		CLOSE) 			\
	_(SYN_SENT,		UNKNOWN,	OUT,	UNKNOWN) 		\
	_(SYN_SENT,		SYN,		OUT,	SYN_RCVD) 		\
	_(SYN_SENT,		ACK,		OUT,	ESTABLISHED)	\
	_(SYN_SENT,		FIN,		OUT,	CLOSE) 			\
	_(SYN_SENT,		RST,		OUT,	CLOSE) 			\
														\
	_(SYN_RCVD,		UNKNOWN,	IN,		UNKNOWN) 		\
	_(SYN_RCVD,		SYN,		IN,		UNKNOWN)		\
	_(SYN_RCVD,		ACK,		IN,		ESTABLISHED)	\
	_(SYN_RCVD,		FIN,		IN,		CLOSE) 			\
	_(SYN_RCVD,		RST,		IN,		CLOSE) 			\
	_(SYN_RCVD,		UNKNOWN,	OUT,	UNKNOWN) 		\
	_(SYN_RCVD,		SYN,		OUT,	UNKNOWN)		\
	_(SYN_RCVD,		ACK,		OUT,	ESTABLISHED)	\
	_(SYN_RCVD,		FIN,		OUT,	CLOSE) 			\
	_(SYN_RCVD,		RST,		OUT,	CLOSE) 			\
														\
	_(ESTABLISHED,	UNKNOWN,	IN,		UNKNOWN) 		\
	_(ESTABLISHED,	SYN,		IN,		UNKNOWN)		\
	_(ESTABLISHED,	ACK,		IN,		ESTABLISHED)	\
	_(ESTABLISHED,	FIN,		IN,		FIN_WAIT) 		\
	_(ESTABLISHED,	RST,		IN,		CLOSE) 			\
	_(ESTABLISHED,	UNKNOWN,	OUT,	UNKNOWN) 		\
	_(ESTABLISHED,	SYN,		OUT,	UNKNOWN)		\
	_(ESTABLISHED,	ACK,		OUT,	ESTABLISHED)	\
	_(ESTABLISHED,	FIN,		OUT,	CLOSE_WAIT) 	\
	_(ESTABLISHED,	RST,		OUT,	CLOSE) 			\
														\
	_(CLOSE_WAIT,	UNKNOWN,	IN,		UNKNOWN) 		\
	_(CLOSE_WAIT,	SYN,		IN,		UNKNOWN)		\
	_(CLOSE_WAIT,	ACK,		IN,		CLOSE_WAIT)		\
	_(CLOSE_WAIT,	FIN,		IN,		CLOSE) 			\
	_(CLOSE_WAIT,	RST,		IN,		CLOSE) 			\
	_(CLOSE_WAIT,	UNKNOWN,	OUT,	UNKNOWN) 		\
	_(CLOSE_WAIT,	SYN,		OUT,	UNKNOWN)		\
	_(CLOSE_WAIT,	ACK,		OUT,	CLOSE_WAIT)		\
	_(CLOSE_WAIT,	FIN,		OUT,	CLOSE) 			\
	_(CLOSE_WAIT,	RST,		OUT,	CLOSE) 			\
														\
	_(FIN_WAIT,		UNKNOWN,	IN,		UNKNOWN) 		\
	_(FIN_WAIT,		SYN,		IN,		UNKNOWN)		\
	_(FIN_WAIT,		ACK,		IN,		FIN_WAIT)		\
	_(FIN_WAIT,		FIN,		IN,		TIME_WAIT) 		\
	_(FIN_WAIT,		RST,		IN,		CLOSE) 			\
	_(FIN_WAIT,		UNKNOWN,	OUT,	UNKNOWN) 		\
	_(FIN_WAIT,		SYN,		OUT,	UNKNOWN)		\
	_(FIN_WAIT,		ACK,		OUT,	FIN_WAIT)		\
	_(FIN_WAIT,		FIN,		OUT,	TIME_WAIT) 		\
	_(FIN_WAIT,		RST,		OUT,	CLOSE) 			\
														\
	_(TIME_WAIT,	UNKNOWN,	IN,		UNKNOWN) 		\
	_(TIME_WAIT,	SYN,		IN,		SYN_SENT)		\
	_(TIME_WAIT,	ACK,		IN,		UNKNOWN)		\
	_(TIME_WAIT,	FIN,		IN,		UNKNOWN) 		\
	_(TIME_WAIT,	RST,		IN,		CLOSE) 			\
	_(TIME_WAIT,	UNKNOWN,	OUT,	UNKNOWN) 		\
	_(TIME_WAIT,	SYN,		OUT,	SYN_SENT)		\
	_(TIME_WAIT,	ACK,		OUT,	UNKNOWN)		\
	_(TIME_WAIT,	FIN,		OUT,	UNKNOWN) 		\
	_(TIME_WAIT,	RST,		OUT,	CLOSE) 			\
														\
	_(UNKNOWN,		UNKNOWN,	IN,		UNKNOWN) 		\
	_(UNKNOWN,		SYN,		IN,		UNKNOWN)		\
	_(UNKNOWN,		ACK,		IN,		UNKNOWN)		\
	_(UNKNOWN,		FIN,		IN,		UNKNOWN) 		\
	_(UNKNOWN,		RST,		IN,		UNKNOWN) 		\
	_(UNKNOWN,		UNKNOWN,	OUT,	UNKNOWN) 		\
	_(UNKNOWN,		SYN,		OUT,	UNKNOWN)		\
	_(UNKNOWN,		ACK,		OUT,	UNKNOWN)		\
	_(UNKNOWN,		FIN,		OUT,	UNKNOWN) 		\
	_(UNKNOWN,		RST,		OUT,	UNKNOWN)		\

#define for_each_tcp_state					\
	_(START,		0,	"start",		5)	\
	_(CLOSE,		1,	"close",		5)	\
	_(SYN_SENT,		2,	"syn-sent",		5)	\
	_(SYN_RCVD,		3,	"syn-recv",		5)	\
	_(ESTABLISHED,	4,	"established",	5)	\
	_(FIN_WAIT,		5,	"fin-wait",		5)	\
	_(CLOSE_WAIT,	6,	"close-wait",	5)	\
	_(TIME_WAIT,	7,	"time-wait",	5)	\
	_(UNKNOWN,		8,	"unknown",		5)	\


typedef enum {
#define _(a, b, c, d) TCP_STATE_##a = (b),
	for_each_tcp_state
#undef _
	TCP_STATE_MAX
} tcp_state_t;

__attribute__((unused))
static const u64 tcp_state_timeout[] = {
#define _(a, b, c, d) d,
	for_each_tcp_state
#undef _
};

static const char *tcp_state_name[] = {
#define _(a, b, c, d) c,
	for_each_tcp_state
#undef _
};

#define TCP_FLAG_CONCERNED (TCP_FLAG_SYN | TCP_FLAG_ACK | TCP_FLAG_FIN | TCP_FLAG_RST)

/* Simplify tcp flags to events */
#define for_each_tcp_event			 	  \
	_((TCP_FLAG_SYN),				 SYN) \
	_((TCP_FLAG_SYN | TCP_FLAG_ACK), SYN) \
	_((TCP_FLAG_ACK),				 ACK) \
	_((TCP_FLAG_FIN),				 FIN) \
	_((TCP_FLAG_FIN | TCP_FLAG_ACK), FIN) \
	_((TCP_FLAG_RST),				 RST) \

typedef enum {
	TCP_EVENT_UNKNOWN = 0,
	TCP_EVENT_SYN,
	TCP_EVENT_ACK,
	TCP_EVENT_FIN,
	TCP_EVENT_RST,
	TCP_EVENT_MAX,
} tcp_event_t;

static tcp_event_t flags_events_map[] = {
#define _(a, b) [a] = (TCP_EVENT_##b),
	for_each_tcp_event
#undef _
	[0xff] = (TCP_EVENT_UNKNOWN)
};

static tcp_state_t tcp_state_transfer_map[FLOW_DIR_MAX][TCP_STATE_MAX][TCP_EVENT_MAX] = {
#define _(a, b, c, d) [FLOW_DIR_##c][TCP_STATE_##a][TCP_EVENT_##b] = (TCP_STATE_##d),
	for_each_tcp_state_transfer_entry
#undef _
};

/**
 * @brief transfer tcp state
 **/
static inline
tcp_state_t tcp_state_transfer(tcp_state_t old, tcp_event_t event, flow_dir_t dir)
{
	return tcp_state_transfer_map[dir][old][event];	
}

/**
 * @brief Convert tcp flags to tcp events
 **/
static inline
u8 flags_to_events(u8 flags)
{
	return flags_events_map[flags];
}

#endif

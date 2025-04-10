#ifndef __included_protocol_h__
#define __included_protocol_h__

#include "vlib/vlib.h"
#include "vlib/init.h"
#include "vnet/ip/ip_packet.h"
#include "vnet/ip/format.h"

#include "flow.h"
#include "plugins/flow_table/vnetfilter/include/vnetfilter.h"

typedef vnetfilter_action_t (*parse_flow_key_function) (vlib_buffer_t *b, flow_key_t *key);
typedef vnetfilter_action_t (*init_state_function) (vlib_buffer_t *b, flow_dir_t direction);
typedef vnetfilter_action_t (*update_state_function) (vlib_buffer_t *b, flow_dir_t direction);
typedef u8 *(*format_flow_function) (u8 * s, va_list * args);

typedef struct {
	parse_flow_key_function parse_key;
	init_state_function init_state;
	update_state_function update_state;
	format_flow_function format_flow;
} protocol_handler_t;

/**
 * @brief Register callback for specific protocol
 * 
 * @param protocol Protocol number
 * @return 0 on success, -1 on failure
 */
int protocol_handler_register(u8 protocol, protocol_handler_t *proto);

/**
 * @brief Get protocol callback functions by protocol number
 * 
 * @param protocol Protocol number
 * @return Protocol callback functions
 */
protocol_handler_t *protocol_handler_get(u8 protocol);

#endif

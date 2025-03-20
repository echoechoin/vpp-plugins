#include "include/protocol.h"

static protocol_handler_t protocol_handlers[IP_PROTOCOL_RESERVED + 1];

int protocol_handler_register(u8 protocol, protocol_handler_t *proto)
{
	protocol_handlers[protocol] = *proto;
	return 0;
}

protocol_handler_t *protocol_handler_get(u8 protocol)
{
	return &protocol_handlers[protocol];
}

#include "include/protocol.h"

static protocol_t protocols[IP_PROTOCOL_RESERVED + 1];

int protocol_register(u8 protocol, protocol_t *proto)
{
	protocols[protocol] = *proto;
	return 0;
}

protocol_t *protocol_get(u8 protocol)
{
	return &protocols[protocol];
}

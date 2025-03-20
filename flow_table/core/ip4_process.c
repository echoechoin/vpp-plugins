#include <stdbool.h>
#include <vnet/ethernet/packet.h>
#include <vnet/ip/ip4_packet.h>
#include <vlib/buffer_funcs.h>
#include <vnet/buffer.h>

#include "../vnetfilter/include/vnetfilter.h"
#include "flow_table/core/include/flow.h"
#include "flow_table/core/include/protocol.h"
#include "vlib/global_funcs.h"
#include "vppinfra/hash.h"
#include "vppinfra/types.h"


vnetfilter_action_t ip4_pre_process(vlib_buffer_t *b)
{
	flow_table_init_buffer(b);
    return VNF_ACCEPT;
}

vnetfilter_action_t ip4_input_process(vlib_buffer_t *b)
{
	u32 sw_if_index;
	uword *hash_table;
	flow_t *flow;
	ip4_header_t *ih4;
	flow_key_t key;
	u8 protocol;
	ip4_address_t src;
	ip4_address_t dst;

	/* Parse L3 header */
    ih4 = (ip4_header_t *)b->data + vnet_buffer(b)->l3_hdr_offset;
	protocol = ih4->protocol;
	src = ih4->src_address;
	dst = ih4->dst_address;

	/* Prepare flow key */
	key.dst.ip.ip4 = dst;
	key.src.ip.ip4 = src;
	key.protocol = protocol;
	key.src_port = 0;
	key.dst_port = 0;

	/* Parse L4 header and fill in flow key */
	protocol_get(protocol)->parse_key(b, &key, false);
	uword hash_key = flow_table_hash(&key);
	
	/* Get flow from flow table */
	hash_table = flow_table_get_worker_ctx()->flows_hash_table;
	flow = (flow_t *)hash_get(hash_table, hash_key);
	if (flow == NULL) {
		sw_if_index = vnet_buffer(b)->sw_if_index[VLIB_RX];
		flow = flow_table_alloc_flow();
		if (flow == NULL)
			return VNF_ACCEPT;

		/* Init flow entry and store it in flow hash table */
		flow_table_flow_init(flow, &key, sw_if_index);
		hash_set(hash_table, hash_key, (uword)&flow->flow[FLOW_DIR_IN]);

		/* Init flow stats */
		protocol_get(protocol)->init_state(b, &key);
	}

	/* Update flow stats */
	protocol_get(protocol)->update_state(b, &key);

    return VNF_ACCEPT;
}

vnetfilter_action_t ip4_output_process(vlib_buffer_t *b)
{
	flow_t *flow;

	flow = flow_table_get_flow(b);
	if (flow == NULL)
		return VNF_ACCEPT;

	
    return VNF_ACCEPT;
}

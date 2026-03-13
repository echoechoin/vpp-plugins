#include <stdbool.h>
#include "vnet/ethernet/packet.h"
#include "vnet/ip/ip4_packet.h"
#include "vlib/buffer_funcs.h"
#include "vnet/buffer.h"
#include "vlib/global_funcs.h"
#include "vppinfra/hash.h"
#include "vppinfra/types.h"


#include "plugins/flow_table/core/include/flow.h"
#include "plugins/flow_table/core/include/protocol.h"
#include "plugins/flow_table/vnetfilter/include/vnetfilter.h"

#define IP4_DEBUG 1
#if IP4_DEBUG
#include <stdio.h>
#include <arpa/inet.h>
#endif

/**
 * @brief Parse VLAN tags and return VLAN information
 *
 * @param b Buffer
 * @param outer_vlan_id Output: Outer VLAN ID
 * @param inner_vlan_id Output: Inner VLAN ID
 * @param vlan_present Output: 0=no VLAN, 1=single VLAN, 2=QinQ
 * @return L3 header offset
 */
static inline u16 parse_vlan_tags(vlib_buffer_t *b, u16 *outer_vlan_id, u16 *inner_vlan_id, u8 *vlan_present)
{
	ethernet_header_t *eth;
	u16 ethertype;
	u16 l3_offset = sizeof(ethernet_header_t);

	*outer_vlan_id = 0;
	*inner_vlan_id = 0;
	*vlan_present = 0;

	eth = (ethernet_header_t *)(b->data + vnet_buffer(b)->l2_hdr_offset);
	ethertype = clib_net_to_host_u16(eth->type);

	/* Check for outer VLAN tag (802.1Q: 0x8100 or 802.1ad: 0x88a8) */
	if (ethertype == ETHERNET_TYPE_VLAN || ethertype == ETHERNET_TYPE_DOT1AD) {
		ethernet_vlan_header_t *vlan = (ethernet_vlan_header_t *)((u8 *)eth + sizeof(ethernet_header_t));
		*outer_vlan_id = clib_net_to_host_u16(vlan->priority_cfi_and_id) & 0x0FFF;
		ethertype = clib_net_to_host_u16(vlan->type);
		l3_offset += 4;  /* VLAN tag is 4 bytes */
		*vlan_present = 1;

		/* Check for inner VLAN tag (QinQ) */
		if (ethertype == ETHERNET_TYPE_VLAN) {
			ethernet_vlan_header_t *inner_vlan = (ethernet_vlan_header_t *)((u8 *)vlan + 4);
			*inner_vlan_id = clib_net_to_host_u16(inner_vlan->priority_cfi_and_id) & 0x0FFF;
			ethertype = clib_net_to_host_u16(inner_vlan->type);
			l3_offset += 4;  /* Inner VLAN tag is also 4 bytes */
			*vlan_present = 2;
		}
	}

	return l3_offset;
}

vnetfilter_action_t ip4_pre_process(vlib_buffer_t *b)
{
	u16 outer_vlan_id, inner_vlan_id;
	u8 vlan_present;
	u16 l3_offset;
	ip4_header_t *ih4;
	i16 ihl;

	/* Parse VLAN tags and get L3 offset */
	l3_offset = parse_vlan_tags(b, &outer_vlan_id, &inner_vlan_id, &vlan_present);
	vnet_buffer(b)->l3_hdr_offset = l3_offset;

	/* Parse IP header */
	ih4 = (ip4_header_t *)(b->data + vnet_buffer(b)->l3_hdr_offset);
	ihl = (i16)((ih4->ip_version_and_header_length & 0x0F) << 2);
	vnet_buffer(b)->l4_hdr_offset = (i16)(ihl + vnet_buffer(b)->l3_hdr_offset);

	/* Store VLAN info in buffer opaque2 (unused space) */
	vnet_buffer2(b)->unused[0] = (outer_vlan_id << 16) | inner_vlan_id;
	vnet_buffer2(b)->unused[1] = vlan_present;

	vlib_buffer_init_flow(b);

#if IP4_DEBUG
	if (vlan_present) {
		printf("--- VLAN detected ---\n");
		printf("Outer VLAN ID: %u\n", outer_vlan_id);
		if (vlan_present == 2)
			printf("Inner VLAN ID: %u (QinQ)\n", inner_vlan_id);
	}
#endif

    return VNF_ACCEPT;
}

vnetfilter_action_t ip4_input_process(vlib_buffer_t *b)
{
	u32 sw_if_index;
	uword *hash_table, hash_key;
	flow_entry_t *flow_entry, **flow_entries;
	flow_t *flow;

#if IP4_DEBUG
	flow = NULL;
#endif

	flow_dir_t direction;
	ip4_header_t *ih4;
	ethernet_header_t *eth;
	flow_key_t key = {0};
	u8 protocol;
	ip4_address_t src;
	ip4_address_t dst;

	/* Parse L2 header for MAC addresses */
	eth = (ethernet_header_t *)(b->data + vnet_buffer(b)->l2_hdr_offset);

	/* Parse L3 header */
    ih4 = (ip4_header_t *)(b->data + vnet_buffer(b)->l3_hdr_offset);
	protocol = ih4->protocol;
	src = ih4->src_address;
	dst = ih4->dst_address;

	/* Prepare flow key */
	key.dst.ip.ip4 = dst;
	key.src.ip.ip4 = src;
	key.protocol = protocol;
	key.src_port = 0;
	key.dst_port = 0;

	/* Extract VLAN info from buffer opaque2 */
	u32 vlan_data = vnet_buffer2(b)->unused[0];
	key.outer_vlan_id = (vlan_data >> 16) & 0xFFFF;
	key.inner_vlan_id = vlan_data & 0xFFFF;
	key.vlan_present = vnet_buffer2(b)->unused[1];

#if IP4_DEBUG
	printf("\n\n--- ip4_input_process ---\n");
#endif

	/* Parse L4 header and fill in flow key */
	protocol_handler_get(protocol)->parse_key(b, &key);
	hash_key = flow_table_hash(&key);
	
	/* Get flow from hash_table */
	hash_table = flow_table_get_worker_ctx()->flows_hash_table;
	flow_entries = (flow_entry_t **)hash_get(hash_table, hash_key);
	if (flow_entries == NULL) {
		direction = FLOW_DIR_IN;
		sw_if_index = vnet_buffer(b)->sw_if_index[VLIB_RX];
		flow = flow_table_alloc_flow();
		flow_entry = &flow->flow_entry[direction];
		if (flow == NULL)
			return VNF_ACCEPT;

		flow->protocol = protocol;
		flow->ip_version = 4;
		vlib_buffer_set_flow(b, flow);

		/* Init flow_entry and store it in hash_table */
		flow_table_init_flow(flow, &key, sw_if_index, direction);

		/* Learn MAC addresses */
		memcpy(flow_entry->smac, eth->src_address, 6);
		memcpy(flow_entry->dmac, eth->dst_address, 6);

		/* Save VLAN information */
		flow_entry->outer_vlan_id = key.outer_vlan_id;
		flow_entry->inner_vlan_id = key.inner_vlan_id;
		flow_entry->vlan_present = key.vlan_present;

		hash_set(hash_table, hash_key, (uword)flow_entry);

		/* Init flow state */
		protocol_handler_get(protocol)->init_state(b, direction);
	} else {
		flow_entry = flow_entries[0]; // TODO: process hash collision
		flow = flow_entry->flow;
		vlib_buffer_set_flow(b, flow);

		/* Update flow state */
		direction = flow_table_get_direction(flow_entry);		
		protocol_handler_get(protocol)->update_state(b, direction);
	}

#if IP4_DEBUG
	flow = flow_entry->flow;
	if (flow) {
		printf("flow: %p\n", flow);
		printf("flow key: src: %d.%d.%d.%d, dst: %d.%d.%d.%d, protocol: %d, src_port: %d, dst_port: %d\n",
				key.src.ip.ip4.as_u8[0], key.src.ip.ip4.as_u8[1], key.src.ip.ip4.as_u8[2], key.src.ip.ip4.as_u8[3],
				key.dst.ip.ip4.as_u8[0], key.dst.ip.ip4.as_u8[1], key.dst.ip.ip4.as_u8[2], key.dst.ip.ip4.as_u8[3],
				key.protocol, key.src_port, key.dst_port);
		printf("hash_table: %p, flow_entry[0]: %p, flow_entry[1]: %p\n", hash_table, &flow->flow_entry[0], &flow->flow_entry[1]);
	}
#endif

    return VNF_ACCEPT;
}

vnetfilter_action_t ip4_output_process(vlib_buffer_t *b)
{
	flow_t *flow;
	flow_key_t key;
	uword *hash_table, hash_key;

	flow = vlib_buffer_get_flow(b);
	if (flow == NULL)
		return VNF_STOP;

#if IP4_DEBUG
	printf("\n--- ip4_output_process ---\n");
#endif

	if (!flow->flow_entry[FLOW_DIR_OUT].flow) {
		/* Create reverse flow entry */
		key = flow->key;
		flow_key_reverse(&key);
		flow_table_update_flow(flow, &key, FLOW_DIR_OUT);

		/* Save reverse flow_entry to hash table */
		hash_table = flow_table_get_worker_ctx()->flows_hash_table;
		hash_key = flow_table_hash(&key);
		hash_set(hash_table, hash_key, (uword)&flow->flow_entry[FLOW_DIR_OUT]);
#if IP4_DEBUG
		printf("flow: %p\n", flow);
		char src_str[INET6_ADDRSTRLEN], dst_str[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET, &key.src.ip.ip4, src_str, sizeof(src_str));
		inet_ntop(AF_INET, &key.dst.ip.ip4, dst_str, sizeof(src_str));
		printf("src: %s, dst: %s, protocol: %d, src_port: %d, dst_port: %d\n", src_str, dst_str, key.protocol, key.src_port, key.dst_port);
		printf("hash_key: %lu, hash_table: %p, flow_entry[0]: %p, flow_entry[1]: %p\n", hash_key, hash_table, &flow->flow_entry[0], &flow->flow_entry[1]);
#endif
	}
	
    return VNF_ACCEPT;
}

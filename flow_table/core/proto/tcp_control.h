#ifndef __included_tcp_control_h__
#define __included_tcp_control_h__

#include <arpa/inet.h>

#include "vlib/vlib.h"
#include "vnet/vnet.h"
#include "vnet/ip/ip.h"
#include "vnet/ethernet/ethernet.h"
#include "vnet/ip/ip4_packet.h"
#include "vnet/tcp/tcp_packet.h"

#include "plugins/flow_table/core/include/flow.h"
#include "plugins/flow_table/core/include/protocol.h"

/* Calculate IP checksum */
static inline u16 flow_table_ip4_checksum(ip4_header_t *ip4)
{
	u32 sum = 0;
	u16 *p = (u16 *)ip4;
	int i;

	/* IP header is 20 bytes (10 u16 words) */
	for (i = 0; i < 10; i++)
		sum += p[i];

	/* Fold 32-bit sum to 16 bits */
	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);

	return ~sum;
}

/* Calculate TCP checksum with pseudo header */
static inline u16 flow_table_tcp_checksum(ip4_header_t *ip4, tcp_header_t *tcp, u16 tcp_len)
{
	u32 sum = 0;
	u16 *p;
	int i;

	/* Pseudo header: src_ip, dst_ip, zero, protocol, tcp_length */
	p = (u16 *)&ip4->src_address;
	sum += p[0] + p[1] + p[2] + p[3]; /* src and dst IP */
	sum += htons(ip4->protocol);
	sum += htons(tcp_len);

	/* TCP header and data */
	p = (u16 *)tcp;
	for (i = 0; i < tcp_len / 2; i++)
		sum += p[i];

	/* Handle odd byte */
	if (tcp_len & 1)
		sum += ((u8 *)tcp)[tcp_len - 1];

	/* Fold to 16 bits */
	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);

	return ~sum;
}

static inline int tcp_send_reset(flow_t *flow)
{
	u32 bi;
	vlib_buffer_t *rst_buffer;
    vlib_main_t *vm = vlib_get_main();
	vnet_main_t *vnm = vnet_get_main();

	ethernet_header_t *eth;
	ip4_header_t *ip4;
	tcp_header_t *tcp;
	u16 tcp_len;

	vnet_sw_interface_t *sw_if;
	vnet_hw_interface_t *hw_if;
	u32 next_node_idx;
	vlib_frame_t *frame;
    u32 *to_next;

	for (int i = 0; i < FLOW_DIR_MAX; i++) {
		/* check if interface is not found */
		if (pool_is_free_index(vnm->interface_main.sw_interfaces, flow->flow_entry[i].sw_if_index))
			return -1;

		/* Check if interface is valid */
		sw_if = vnet_get_sup_sw_interface(vnm, flow->flow_entry[i].sw_if_index);
		if (sw_if->type != VNET_SW_INTERFACE_TYPE_HARDWARE)
			continue;

		hw_if = vnet_get_sup_hw_interface(vnm, flow->flow_entry[i].sw_if_index);

		/* Check if MAC addresses are learned */
		u8 zero_mac[6] = {0};
		if (memcmp(flow->flow_entry[i].smac, zero_mac, 6) == 0 ||
		    memcmp(flow->flow_entry[i].dmac, zero_mac, 6) == 0) {
			/* MAC addresses not learned yet, skip */
			continue;
		}

		/* Allocate buffer for RST packet */
		if (vlib_buffer_alloc(vm, &bi, 1) != 1)
			continue;

		rst_buffer = vlib_get_buffer(vm, bi);

		/* Initialize buffer metadata */
		clib_memset(vnet_buffer(rst_buffer), 0, sizeof(vnet_buffer_opaque_t));
		rst_buffer->flags = 0;
		rst_buffer->current_data = 0;

		u16 l3_offset = sizeof(ethernet_header_t);

		/* Construct Ethernet header */
		eth = (ethernet_header_t *)rst_buffer->data;
		memcpy(eth->src_address, flow->flow_entry[i].dmac, 6);
		memcpy(eth->dst_address, flow->flow_entry[i].smac, 6);

		/* Add VLAN tags if present */
		if (flow->flow_entry[i].vlan_present >= 1) {
			/* Outer VLAN tag */
			eth->type = clib_host_to_net_u16(ETHERNET_TYPE_VLAN);
			ethernet_vlan_header_t *vlan = (ethernet_vlan_header_t *)(eth + 1);
			u16 tci = flow->flow_entry[i].outer_vlan_id & 0x0FFF;
			vlan->priority_cfi_and_id = clib_host_to_net_u16(tci);

			if (flow->flow_entry[i].vlan_present == 2) {
				/* Inner VLAN tag (QinQ) */
				vlan->type = clib_host_to_net_u16(ETHERNET_TYPE_VLAN);
				ethernet_vlan_header_t *inner_vlan = (ethernet_vlan_header_t *)((u8 *)vlan + 4);
				u16 inner_tci = flow->flow_entry[i].inner_vlan_id & 0x0FFF;
				inner_vlan->priority_cfi_and_id = clib_host_to_net_u16(inner_tci);
				inner_vlan->type = clib_host_to_net_u16(ETHERNET_TYPE_IP4);
				l3_offset += 8;  /* Two VLAN tags = 8 bytes */
			} else {
				/* Single VLAN */
				vlan->type = clib_host_to_net_u16(ETHERNET_TYPE_IP4);
				l3_offset += 4;  /* One VLAN tag = 4 bytes */
			}
		} else {
			/* No VLAN */
			if (flow->ip_version == 4)
				eth->type = htons(ETHERNET_TYPE_IP4);
			else
				eth->type = htons(ETHERNET_TYPE_IP6);
		}

		/* Construct IP header */
		if (flow->ip_version == 4) {
			ip4 = (ip4_header_t *)(rst_buffer->data + l3_offset);
			clib_memset(ip4, 0, sizeof(ip4_header_t));

			ip4->ip_version_and_header_length = 0x45;
			ip4->tos = 0;
			ip4->length = htons(sizeof(ip4_header_t) + sizeof(tcp_header_t));
			ip4->fragment_id = 0;
			ip4->flags_and_fragment_offset = 0;
			ip4->ttl = 64;
			ip4->protocol = IP_PROTOCOL_TCP;
			ip4->src_address.as_u32 = flow->flow_entry[i].dst.ip.ip4.as_u32;
			ip4->dst_address.as_u32 = flow->flow_entry[i].src.ip.ip4.as_u32;
			ip4->checksum = 0;

			/* Calculate IP checksum */
			ip4->checksum = flow_table_ip4_checksum(ip4);

			tcp_len = sizeof(tcp_header_t);
		} else {
			// TODO: IPv6 support
			vlib_buffer_free_one(vm, bi);
			continue;
		}

		/* Construct TCP header */
		if (flow->ip_version == 4)
			tcp = (tcp_header_t *)(rst_buffer->data + l3_offset + sizeof(ip4_header_t));
		else
			tcp = (tcp_header_t *)(rst_buffer->data + l3_offset + sizeof(ip6_header_t));

		clib_memset(tcp, 0, sizeof(tcp_header_t));
		tcp->src_port = htons(flow->flow_entry[i].dst_port);
		tcp->dst_port = htons(flow->flow_entry[i].src_port);
		tcp->seq_number = htonl(flow->flow_entry[i].tcp_attr.ack);
		tcp->ack_number = htonl(flow->flow_entry[i].tcp_attr.seq);
		tcp->data_offset_and_reserved = 0x50; /* 5 words (20 bytes), no options */
		tcp->flags = TCP_FLAG_RST | TCP_FLAG_ACK;
		tcp->window = 0;
		tcp->urgent_pointer = 0;
		tcp->checksum = 0;

		/* Calculate TCP checksum */
		if (flow->ip_version == 4) {
			tcp->checksum = flow_table_tcp_checksum(ip4, tcp, tcp_len);
		}

		/* Set buffer metadata */
		vnet_buffer(rst_buffer)->l2_hdr_offset = 0;
		rst_buffer->total_length_not_including_first_buffer = 0;

		if (flow->ip_version == 4) {
			vnet_buffer(rst_buffer)->l3_hdr_offset = l3_offset;
			vnet_buffer(rst_buffer)->l4_hdr_offset = l3_offset + sizeof(ip4_header_t);
			rst_buffer->current_length = l3_offset + sizeof(ip4_header_t) + sizeof(tcp_header_t);
			rst_buffer->flags |= VNET_BUFFER_F_IS_IP4;
		} else {
			vnet_buffer(rst_buffer)->l3_hdr_offset = l3_offset;
			vnet_buffer(rst_buffer)->l4_hdr_offset = l3_offset + sizeof(ip6_header_t);
			rst_buffer->current_length = l3_offset + sizeof(ip6_header_t) + sizeof(tcp_header_t);
			rst_buffer->flags |= VNET_BUFFER_F_IS_IP6;
		}

		/* Set buffer flags to indicate packet structure and origin */
		rst_buffer->flags |= VLIB_BUFFER_TOTAL_LENGTH_VALID          /* Total packet length is valid (no chained buffers) */
						  |  VNET_BUFFER_F_LOCALLY_ORIGINATED        /* Packet originated from VPP (not received from interface) */
						  |  VNET_BUFFER_F_L2_HDR_OFFSET_VALID       /* L2 (Ethernet) header offset is set and valid */
						  |  VNET_BUFFER_F_L3_HDR_OFFSET_VALID       /* L3 (IP) header offset is set and valid */
						  |  VNET_BUFFER_F_L4_HDR_OFFSET_VALID;      /* L4 (TCP) header offset is set and valid */

		vnet_buffer(rst_buffer)->sw_if_index[VLIB_RX] = 0;
		vnet_buffer(rst_buffer)->sw_if_index[VLIB_TX] = flow->flow_entry[i].sw_if_index;

		/* Send the RST packet */
		next_node_idx = hw_if->output_node_index;
		frame = vlib_get_frame_to_node(vm, next_node_idx);
		to_next = vlib_frame_vector_args(frame);
		to_next[0] = bi;
		frame->n_vectors = 1;
		vlib_put_frame_to_node(vm, next_node_idx, frame);

#if TCP_DEBUG
		printf("Sent TCP RST packet on sw_if_index %u\n", flow->flow_entry[i].sw_if_index);
#endif
	}
	return 0;
}

#endif

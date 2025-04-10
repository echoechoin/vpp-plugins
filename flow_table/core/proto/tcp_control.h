#ifndef __included_tcp_control_h__
#define __included_tcp_control_h__

#include <arpa/inet.h>

#include "vlib/vlib.h"
#include "vnet/vnet.h"
#include "vnet/ip/ip.h"
#include "vnet/ethernet/ethernet.h"

#include "plugins/flow_table/core/include/flow.h"
#include "plugins/flow_table/core/include/protocol.h"

static inline int tcp_send_reset(flow_t *flow)
{
	u32 bi;
	vlib_buffer_t *rst_buffer;
    vlib_main_t *vm = vlib_get_main();
	vnet_main_t *vnm = vnet_get_main();

	ethernet_header_t *eth;
	ip4_header_t *ip4;
	tcp_header_t *tcp;
	
	vnet_sw_interface_t *sw_if;
	vnet_hw_interface_t *hw_if;
	u32 next_node_idx;
	vlib_frame_t *frame;
    u32 *to_next;

	for (int i = 0; i < FLOW_DIR_MAX; i++) {
		/* check if interface is not found */
		if (pool_is_free_index(vnm->interface_main.sw_interfaces, flow->flow_entry[i].sw_if_index))
			return -1;

		/* check if interface is not a hardware interface */
		sw_if = vnet_get_sup_sw_interface(vnm, flow->flow_entry[i].sw_if_index);
		if (sw_if->type != VNET_SW_INTERFACE_TYPE_HARDWARE)
			return -1;

		hw_if = vnet_get_sup_hw_interface(vnm, flow->flow_entry[i].sw_if_index);
		if (vlib_buffer_alloc(vm, &bi, 1) != 1) 
			return -1;

		rst_buffer = vlib_get_buffer(vm, bi);

		/* ethernet header */
		eth = (ethernet_header_t *)rst_buffer->data;
		memcpy(eth->src_address, flow->flow_entry[i].dmac, 6);
		memcpy(eth->dst_address, flow->flow_entry[i].smac, 6);
		if (flow->ip_version == 4)
			eth->type = htons(0x800);
		else
			eth->type = htons(0x86dd);

		/* ip header */
		if (flow->ip_version == 4) {
			ip4 = (ip4_header_t *)(rst_buffer->data + sizeof(ethernet_header_t));
			ip4->ip_version_and_header_length = 0x45;
			ip4->ttl = 64;
			ip4->protocol = IP_PROTOCOL_TCP;
			ip4->src_address.as_u32 = flow->flow_entry[i].dst.ip.ip4.as_u32;
			ip4->dst_address.as_u32 = flow->flow_entry[i].src.ip.ip4.as_u32;
			ip4->length = sizeof(ethernet_header_t) + sizeof(ip4_header_t) + sizeof(tcp_header_t);
			ip4->checksum = 0; // fixup after-the-fact
		} else {
			// TODO: ip6
		}

		/* tcp header */
		if (flow->ip_version == 4)
			tcp = (tcp_header_t *)(rst_buffer->data + sizeof(ethernet_header_t) + sizeof(ip4_header_t));
		else
			tcp = (tcp_header_t *)(rst_buffer->data + sizeof(ethernet_header_t) + sizeof(ip6_header_t));
		tcp->src_port = flow->flow_entry[i].dst_port;
		tcp->dst_port = flow->flow_entry[i].src_port;
		tcp->seq_number = flow->flow_entry[i].tcp_attr.ack;
		tcp->ack_number = flow->flow_entry[i].tcp_attr.seq;
		tcp->flags = TCP_FLAG_RST;
		tcp->window = 0;
		tcp->checksum = 0; // fixup after-the-fact
		tcp->urgent_pointer = 0;

		// /* checksum fixup */
		// if (flow->ip_version == 4) {
			
		// } else {
		// 	// TODO: tcp6
		// }
		
		/* send packet to selected interface */
		vnet_buffer(rst_buffer)->l2_hdr_offset = sizeof(ethernet_header_t);
		rst_buffer->total_length_not_including_first_buffer = 0;
		if (flow->ip_version == 4) {
			vnet_buffer(rst_buffer)->l3_hdr_offset = sizeof(ethernet_header_t) + sizeof(ip4_header_t);
			vnet_buffer(rst_buffer)->l4_hdr_offset = sizeof(ethernet_header_t) + sizeof(ip4_header_t) + sizeof(tcp_header_t);
			rst_buffer->current_length = sizeof(ethernet_header_t) + sizeof(ip4_header_t) + sizeof(tcp_header_t);
			rst_buffer->flags = VNET_BUFFER_F_IS_IP4;
		} else {
			vnet_buffer(rst_buffer)->l3_hdr_offset = sizeof(ethernet_header_t) + sizeof(ip6_header_t);
			vnet_buffer(rst_buffer)->l4_hdr_offset = sizeof(ethernet_header_t) + sizeof(ip6_header_t) + sizeof(tcp_header_t);
			rst_buffer->current_length = sizeof(ethernet_header_t) + sizeof(ip6_header_t) + sizeof(tcp_header_t);
			rst_buffer->flags = VNET_BUFFER_F_IS_IP6;
		}
		
		rst_buffer->flags |= VNET_BUFFER_OFFLOAD_F_TCP_CKSUM
						  |  VLIB_BUFFER_TOTAL_LENGTH_VALID
						  |  VNET_BUFFER_F_LOCALLY_ORIGINATED
						  |  VNET_BUFFER_F_L2_HDR_OFFSET_VALID;
		vnet_buffer(rst_buffer)->sw_if_index[VLIB_RX] = 0;
		vnet_buffer(rst_buffer)->sw_if_index[VLIB_TX] = flow->flow_entry[i].sw_if_index;
		next_node_idx = hw_if->output_node_index;
		frame = vlib_get_frame_to_node(vm, next_node_idx);
		to_next = vlib_frame_vector_args(frame);
		to_next[0] = bi;
		frame->n_vectors = 1;
		vlib_put_frame_to_node(vm, next_node_idx, frame);
	}
	return 0;
}

#endif

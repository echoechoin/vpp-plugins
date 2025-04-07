#ifndef __included_flow_h__
#define __included_flow_h__

#include "vnet/ip/ip4_packet.h"
#include "vnet/ip/ip6_packet.h"
#include "vnet/ip/ip_types.h"
#include "vppinfra/pool.h"
#include "vppinfra/types.h"
#include "vppinfra/tw_timer_2t_1w_2048sl.h"
#include "vppinfra/xxhash.h"

typedef enum {
	FLOW_DIR_IN,
	FLOW_DIR_OUT,
	FLOW_DIR_MAX,
} flow_dir_t;

/** For caculating hash value **/
typedef struct {
	ip_address_t src;
	ip_address_t dst;
	u16 src_port;
	u16 dst_port;
	u8 protocol;
} __attribute__((aligned(sizeof(u64)))) flow_key_t;

#define SWAP(a, b) do { typeof(a) tmp = (a); (a) = (b); (b) = tmp; } while (0)
#define FLOW_UNSET 0

static inline
void flow_key_reverse(flow_key_t *key)
{
	SWAP(key->src, key->dst);
	SWAP(key->src_port, key->dst_port);
}

/** Flow entry **/
typedef struct {
	/* where the flow entry is stored */
	void *flow; /* pointer to flow_t */
	u8 smac[6];
	u8 dmac[6];
	ip_address_t src;
	ip_address_t dst;

	/* ip protocol: e.g. IP_PROTO_TCP == 6, IP_PROTO_UDP == 17 */
	u8 protocol;
	u16 src_port;
	u16 dst_port;
} flow_entry_t;

/** Flow **/
typedef struct {
	/* refcount */
	u32 refcount;

	/* input sw_if_index */
	u32 sw_if_index;

	/* flow key */
	flow_key_t key;

	/* flow entry for each direction */
	flow_entry_t flow_entry[FLOW_DIR_MAX];

	/* ip version */
	u8 ip_version;

	/* ip protocol: e.g. IP_PROTO_TCP == 6, IP_PROTO_UDP == 17 */
	u8 protocol;

	/* flow state: e.g. tcp state SYN_SENT, SYN_RECV, etc */
	u8 state;

	/* is tracking the complete three-way handshake? */
	/* We can judge which direction is the client or server based on this. */
	bool three_way_handshake;

	/* timer ctx */
	u32 elt_index;
	u32 stop_timer_handle;
} flow_t;

typedef struct {
	/* flows for each worker */
	flow_t *flows;

	/* hash table mapping flow_id and flow. */
	uword *flows_hash_table;

	/* vlib main for each worker */
	vlib_main_t *vm;

	/* timer wheel for flow expiration */
	tw_timer_wheel_2t_1w_2048sl_t tw;
} flow_table_wrk_ctx_t;

typedef struct {
	/* Context for each worker */
	flow_table_wrk_ctx_t *wrk_ctx;
} flow_table_main_t;

extern flow_table_main_t flow_table_main;

/**
 * @brief Generate hash key from flow key
 **/
static inline
uword flow_table_hash(flow_key_t *key)
{
	uword hash = 0;
	for (int i = 0; i < sizeof (flow_key_t) / sizeof (u64); i++) {
		hash ^= clib_xxhash(((uword *)key)[i]);
	}
	return hash;
}

/**
 * @brief Get worker context for given thread index
 **/
static inline
flow_table_wrk_ctx_t *flow_table_get_worker_ctx()
{
	uword thread_index = vlib_get_thread_index();
	ASSERT (thread_index < vec_len (flow_table_main.wrk_ctx));
	return &flow_table_main.wrk_ctx[thread_index];
}

/**
 * @brief Get flow entry from flow index
 **/
static inline
flow_t *flow_table_get_flow_by_index (u32 flow_index)
{
	flow_table_wrk_ctx_t *wrk = flow_table_get_worker_ctx();
	if (PREDICT_FALSE (pool_is_free_index (wrk->flows, flow_index)))
		return 0;
	return pool_elt_at_index (wrk->flows, flow_index);
}

/**
 * @brief Get flow index from flow entry
 **/
static inline
u32 flow_table_get_index_by_flow (flow_t *flow)
{
	flow_table_wrk_ctx_t *wrk = flow_table_get_worker_ctx();
	return (flow - wrk->flows);
}

/**
 * @brief allocate flow entry for given thread index
 **/
static inline
flow_t *flow_table_alloc_flow()
{
	flow_t *flow = NULL;
	flow_table_wrk_ctx_t *wrk = flow_table_get_worker_ctx();
	pool_get(wrk->flows, flow);
	if (flow) {
		memset(flow, 0, sizeof(flow_t));
		flow->elt_index = flow - wrk->flows;
		flow->refcount = 1;
		flow->stop_timer_handle = ~0;
	}
	return flow;
}

/**
 * @brief Update flow entry
 **/
static inline
void flow_table_update_flow(flow_t *flow, flow_key_t *key, flow_dir_t direction)
{
	flow->flow_entry[direction].src = key->src;
	flow->flow_entry[direction].dst = key->dst;
	flow->flow_entry[direction].protocol = key->protocol;
	flow->flow_entry[direction].src_port = key->src_port;
	flow->flow_entry[direction].dst_port = key->dst_port;
	flow->flow_entry[direction].flow = flow;
}

/**
 * @brief Init flow entry
 **/
static inline
void flow_table_init_flow(flow_t *flow, flow_key_t *key, u32 sw_if_index, flow_dir_t direction)
{
	flow_table_update_flow(flow, key, direction);
	flow->key = *key;
	if (sw_if_index != ~0)
		flow->sw_if_index = sw_if_index;
}

static inline
flow_dir_t flow_table_get_direction(flow_entry_t *flow_entry)
{
	flow_t *flow = flow_entry->flow;
	return &(flow->flow_entry[FLOW_DIR_IN]) == flow_entry ? FLOW_DIR_IN : FLOW_DIR_OUT;
}

/**
 * @brief set FLOW_UNSET means buffer is not associated with any flow.
 **/
static inline
void vlib_buffer_init_flow(vlib_buffer_t *b)
{
	((u32 *)(b->data - VLIB_BUFFER_PRE_DATA_SIZE))[0] = FLOW_UNSET;
}

/**
 * @brief Store flow to vlib buffer
 **/
static inline
void vlib_buffer_set_flow(vlib_buffer_t *b, flow_t *flow)
{
	u32 flow_index = flow_table_get_index_by_flow(flow);
	((u32 *)(b->data - VLIB_BUFFER_PRE_DATA_SIZE))[0] = flow_index;
}

/**
 * @brief Get flow from vlib buffer
 **/
static inline
flow_t *vlib_buffer_get_flow(vlib_buffer_t *b)
{
	u32 flow_index = ((u32 *)(b->data - VLIB_BUFFER_PRE_DATA_SIZE))[0];
	if (flow_index == ~0)
		return NULL;
	return flow_table_get_flow_by_index(flow_index);
}


#endif

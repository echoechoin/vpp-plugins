#ifndef __included_cpu_loading_h__
#define __included_cpu_loading_h__

#include "vnet/vnet.h"


#define DEFAULT_SAMPLE_TIME 1.5

#define for_each_poll_type_node \
    _(dpdk) \
    _(virtio) \

enum {
#define _(n) POLLING_NODE_##n,
    for_each_poll_type_node
#undef _
    POLLING_NODE_MAX,
};

static const char *poll_type_str[] = {
#define _(n) #n "-input",
    for_each_poll_type_node
#undef _
};

typedef struct {
    vlib_main_t *vlib_main;

    /* 用于计算 cpu loading 的参数 */
    u64 *idle_time[POLLING_NODE_MAX];
    u64 *total_time;

    /* 采样时间 */
    f64 sampling_time;

    /* 每隔 <sampling_time> seconds 计算一次 cpu loading */
    float *last_cpu_loading;

    /* 每个线程 polling 节点的 ID */
    u32 *polling_node_index[POLLING_NODE_MAX];
} cpu_loading_main_t;

extern cpu_loading_main_t cpu_loading_main;

#endif /* __included_cpu_loading_h__ */

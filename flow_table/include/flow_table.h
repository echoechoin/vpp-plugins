#ifndef __included_flow_table_h__
#define __included_flow_table_h__

#include <vnet/vnet.h>
#include <vnet/ip/ip.h>
#include <vnet/ethernet/ethernet.h>

#include <vppinfra/hash.h>
#include <vppinfra/error.h>

typedef struct {
    /* convenience */
    vlib_main_t *vlib_main;
    vnet_main_t *vnet_main;
    ethernet_main_t *ethernet_main;
} flow_table_main_t;

extern flow_table_main_t flow_table_main;

#endif /* __included_flow_table_h__ */

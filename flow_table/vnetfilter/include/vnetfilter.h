#ifndef __included_vnetfilter_h__
#define __included_vnetfilter_h__

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
} vnetfilter_main_t;

extern vnetfilter_main_t vnetfilter_main;

typedef enum {
	VNF_ACCEPT,
	VNF_DROP,
	VNF_STOLEN,
	VNF_STOP,
} vnetfilter_action_t;


#endif /* __included_vnetfilter_h__ */

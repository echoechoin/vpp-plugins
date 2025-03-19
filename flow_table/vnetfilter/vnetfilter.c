#include <vnet/vnet.h>
#include <vnet/plugin/plugin.h>
#include <vpp/app/version.h>

#include "include/vnetfilter.h"

vnetfilter_main_t vnetfilter_main;

/**
 * @brief vnetfilter init function
 **/
static clib_error_t *vnetfilter_init(vlib_main_t * vm)
{
	vnetfilter_main_t *fmp = &vnetfilter_main;
	clib_error_t *error = 0;

	fmp->vlib_main = vm;
	fmp->vnet_main = vnet_get_main();

	return error;
}

VLIB_INIT_FUNCTION(vnetfilter_init);

VLIB_PLUGIN_REGISTER() = {
	.version = VPP_BUILD_VER,
	.description = "Tracing and action on flows",
};

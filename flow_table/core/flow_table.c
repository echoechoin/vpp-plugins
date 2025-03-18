#include <vnet/vnet.h>
#include <vnet/plugin/plugin.h>
#include <vpp/app/version.h>

#include "include/flow_table.h"

flow_table_main_t flow_table_main;

/**
 * @brief Enable or disable a feature on an interface
 **/
static clib_error_t *flow_table_init(vlib_main_t * vm)
{
    flow_table_main_t *fmp = &flow_table_main;
    clib_error_t *error = 0;

    fmp->vlib_main = vm;
    fmp->vnet_main = vnet_get_main();

    return error;
}

VLIB_INIT_FUNCTION(flow_table_init);

VLIB_PLUGIN_REGISTER() = {
  .version = VPP_BUILD_VER,
  .description = "Tracing and action on flows",
};

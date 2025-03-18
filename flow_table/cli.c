#include <vnet/vnet.h>
#include <vnet/plugin/plugin.h>
#include <vpp/app/version.h>
#include <vlib/cli.h>
#include <stdbool.h>

#include "include/flow_table.h"
#include "include/node.h"


void flow_table_hook_enable_disable (flow_table_main_t * lmp, u32 sw_if_index, char *arc_name, char *node_name,
        int enable_disable)
{
    if (pool_is_free_index (lmp->vnet_main->interface_main.sw_interfaces,
    sw_if_index))
        vlib_cli_output(lmp->vlib_main, "invalid interface name");

    vnet_feature_enable_disable (arc_name, node_name,
        sw_if_index, enable_disable, 0, 0);
}

static clib_error_t *
flow_table_hook_enable_disable_command_fn (vlib_main_t * vm __attribute__((unused)),
                                   unformat_input_t * input,
                                   vlib_cli_command_t * cmd __attribute__((unused)))
{
    flow_table_main_t * lmp = &flow_table_main;
    u32 sw_if_index = ~0;
    int enable_disable = 1;

    while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT) {
        if (unformat (input, "disable"))
            enable_disable = 0;
        else if (unformat (input, "%U", unformat_vnet_sw_interface,
                            lmp->vnet_main, &sw_if_index))
            ;
        else
            break;
    }

    if (sw_if_index == ~0)
        return clib_error_return (0, "Please specify an interface...");

#define _(a, b, c) flow_table_hook_enable_disable (lmp, sw_if_index, b, c, enable_disable);
    foreach_flow_table_hook    
#undef _
    return 0;
}

VLIB_CLI_COMMAND (flow_table_hook_enable_disable_command, static) =
{
    .path = "flow_table enable-disable",
    .short_help = "flow_table enable-disable <interface-name> [disable]",
    .function = flow_table_hook_enable_disable_command_fn,
};

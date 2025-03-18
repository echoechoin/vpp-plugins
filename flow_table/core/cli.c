#include <vnet/vnet.h>
#include <vnet/plugin/plugin.h>
#include <vpp/app/version.h>
#include <vlib/cli.h>
#include <stdbool.h>

#include "include/flow_table.h"
#include "include/node.h"

static clib_error_t *
flow_table_hook_enable_disable_command_fn (vlib_main_t *vm __attribute__((unused)),
                                   unformat_input_t *input,
                                   vlib_cli_command_t *cmd __attribute__((unused)))
{
    flow_table_main_t *fmp = &flow_table_main;
    u32 sw_if_index = ~0;
    int enable_disable = 1;

    while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT) {
        if (unformat (input, "disable"))
            enable_disable = 0;
        else if (unformat (input, "%U", unformat_vnet_sw_interface,
                            fmp->vnet_main, &sw_if_index))
            ;
        else
            break;
    }

    if (sw_if_index == ~0)
        return clib_error_return (0, "Please specify an interface...");

#define _(a, b, c) flow_table_hook_enable_disable (fmp, sw_if_index, b, b "-hook", enable_disable);
    foreach_flow_table_hook    
#undef _
    return 0;
}

VLIB_CLI_COMMAND (flow_table_hook_enable_disable_command, static) = {
    .path = "flow_table enable-disable",
    .short_help = "flow_table enable-disable <interface-name> [disable]",
    .function = flow_table_hook_enable_disable_command_fn,
};

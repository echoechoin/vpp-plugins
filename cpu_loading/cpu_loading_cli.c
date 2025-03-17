#include "vlib/vlib.h"
#include "cpu_loading.h"
#include "vlib/cli.h"

/**
 * @brief 查询CPU使用率
 **/
clib_error_t *cpu_loading_show_cpu_loading(struct vlib_main_t * vm,
        unformat_input_t * input, struct vlib_cli_command_t * cmd)
{
    int i, j;
    clib_error_t *error = 0;
    cpu_loading_main_t *cpm = &cpu_loading_main;

    vlib_cli_output(vm, "sampling time: %.2f", cpm->sampling_time);
    vlib_cli_output(vm, "---------------------", cpm->sampling_time);
    for (i = 0; i < vlib_get_n_threads(); i++) {
        vlib_cli_output(vm, "thread %u:", i);
        for (j = 0; j < POLLING_NODE_MAX; j++) {
            if (cpm->polling_node_index[j][i] == ~0)
                continue;
            vlib_cli_output(vm, "    %s %s %lu", poll_type_str[j], "idle clock:", cpm->idle_time[j][i]);
        }
        vlib_cli_output(vm, "    %s %lu", "total clock:", cpm->total_time[i]);
        vlib_cli_output(vm, "    %s %.2f%%", "last cpu loading:", cpm->last_cpu_loading[i]);
    }
    return error;
}

/**
 * @brief 设置采样时间
 **/
clib_error_t *cpu_loading_set_sampling_time(struct vlib_main_t *vm,
        unformat_input_t *input, struct vlib_cli_command_t *cmd)
{
    clib_error_t *error = 0;
    cpu_loading_main_t *cpm = &cpu_loading_main;

    if (unformat(input, "%f", &cpm->sampling_time) == 0)
        error = clib_error_return(0, "please input a valid sampling time");
    return error;
}

VLIB_CLI_COMMAND (vlib_cli_show_cpu_loading_command, static) = {
    .path = "show cpu-loading",
    .short_help = "Show commands",
    .function = cpu_loading_show_cpu_loading,
};

VLIB_CLI_COMMAND (cpu_loading_set_sampling_time_command, static) = {
    .path = "set cpu-loading sampling-time",
    .long_help = "set cpu-loading <float>",
    .short_help = "set sampling time",
    .function = cpu_loading_set_sampling_time,
};

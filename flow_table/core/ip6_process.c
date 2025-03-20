#include "../vnetfilter/include/vnetfilter.h"

vnetfilter_action_t ip6_pre_process(vlib_buffer_t *b)
{
    return VNF_ACCEPT;
}

vnetfilter_action_t ip6_input_process(vlib_buffer_t *b)
{
    return VNF_ACCEPT;
}

vnetfilter_action_t ip6_output_process(vlib_buffer_t *b)
{
    return VNF_ACCEPT;
}

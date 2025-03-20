#include <vnet/vnet.h>
#include <vnet/plugin/plugin.h>
#include <vppinfra/clib_error.h>

#include "flow_table/vnetfilter/include/vnetfilter.h"
#include "flow_table/vnetfilter/include/hook.h"

#include "include/process.h"
#include "include/flow.h"

/**
 * @brief vnetfilter init function.
 **/
static clib_error_t *flow_table_init(vlib_main_t * vm)
{
	clib_error_t *error = 0;

	/* Register all process hooks. */
#define _(a, b, c) \
	vnetfilter_hook_process_t c##_hook_##b = {							\
		.priority = (a),												\
		.function = (b),												\
	};                                                                  \
	if (c##_hook_register(&c##_hook_##b) != 0) {						\
		error = clib_error_return(0, "failed to register %s hook", #c);	\
		return error;													\
	}
for_each_flow_table_hook
#undef _

	/* Initialize flow table */
	error = flow_table_main_init(vm);
	return error;
}

VLIB_INIT_FUNCTION(flow_table_init);

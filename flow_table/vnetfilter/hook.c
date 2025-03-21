#include <vppinfra/vec.h>
#include "include/hook.h"

#define _(hook, _arc_name, _runs_after)										\
	vnetfilter_hook_process_t *hook##_hook_process_list = NULL;				\
																			\
	void hook##_hook_process(vlib_buffer_t *b, vnetfilter_action_t *action)	\
	{																		\
		if (hook##_hook_process_list != NULL) {								\
			vnetfilter_hook_process_t *hook = hook##_hook_process_list;		\
			for (int i = 0; i < vec_len(hook##_hook_process_list); i++) {	\
				vnetfilter_hook_function function = hook->function;			\
				vnetfilter_action_t result = function(b);					\
				if (result != VNF_ACCEPT) {									\
					*action = result;										\
					return;													\
				}															\
			}																\
		}																	\
		*action = VNF_ACCEPT;												\
	}																		\
																			\
	int hook##_hook_register(vnetfilter_hook_process_t *hook)				\
	{																		\
		if (hook == NULL) {													\
			return -1;														\
		}																	\
		if (hook##_hook_process_list == NULL) {								\
			hook##_hook_process_list = vec_new(vnetfilter_hook_process_t, 1); \
		}																	\
		vec_add1(hook##_hook_process_list, *hook);							\
		return 0;															\
	}																		\

foreach_vnetfilter_hook
#undef _

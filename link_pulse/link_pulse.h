
/*
 * link_pulse.h - Link Status Listener Plugin Header
 *
 * This header file defines the data structures and constants used by the
 * link_pulse plugin for monitoring network interface link state changes
 * and executing callback scripts.
 */

#ifndef __included_link_pulse_h__
#define __included_link_pulse_h__

#include <vlib/log.h>

#define link_pulse_CALLBACK_SCRIPT_PATH_MAX_LEN 128
#define link_pulse_CALLBACK_SCRIPT_NUM 10

typedef struct link_pulse_main_t {
  u8 **callback_scripts;

  /* cached for mt-safe signaling */
  struct vlib_main_t *vlib_main;

  /* logger */
  vlib_log_class_t log_class;
} link_pulse_main_t;

#endif /* __included_link_pulse_h__ */

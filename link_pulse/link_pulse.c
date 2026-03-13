/*
 * link_pulse.c - Link Status Listener Plugin
 *
 * This plugin monitors network interface link state changes (up/down events)
 * and executes user-registered callback scripts when link status changes occur.
 *
 * Features:
 * - Monitors hardware interface link state transitions
 * - Supports registration of multiple callback scripts (up to 10)
 * - Executes scripts asynchronously via fork/exec when link state changes
 * - Scripts receive interface name and status (up/down) as command-line arguments
 *
 * Usage:
 * - Register a callback script: link_pulse script add <script-path>
 * - Remove a callback script: link_pulse script del <script-path>
 * - List registered scripts: show link_pulse scripts
 *
 * When a link state change occurs, each registered script is executed with:
 *   argv[0] = script path
 *   argv[1] = interface name
 *   argv[2] = "up" or "down"
 */


#include "vnet/plugin/plugin.h"
#include "vpp/app/version.h"

#include <link_pulse/link_pulse.h>

link_pulse_main_t link_pulse_main = {
  .callback_scripts = 0,
  .vlib_main = 0,
  .log_class = 0,
};

static void
link_pulse_run_script (u8 *script, u8 *if_name, u8 is_up)
{
  link_pulse_main_t *lslm = &link_pulse_main;
  pid_t pid = fork ();
  if (pid == 0)
    {
      /* child */
      char *argv[4];
      argv[0] = (char *) script;
      argv[1] = (char *) if_name;
      argv[2] = (char *) (is_up ? "up" : "down");
      argv[3] = 0;
      (void) execv (argv[0], argv);
      _exit (127);
    }
  else if (pid < 0)
    {
      vlib_log_notice (lslm->log_class, "failed to fork process for script '%s'", script);
    }
  /* Note: child processes are automatically reaped by the kernel
   * because VPP sets SIGCHLD to SIG_IGN in vlib/unix/main.c */
}

static clib_error_t *
link_pulse_init(vlib_main_t *vm)
{
  clib_error_t *error = NULL;
  link_pulse_main_t *lslm = &link_pulse_main;

  lslm->vlib_main = vm;
  lslm->log_class = vlib_log_register_class ("link_pulse", 0);

  vlib_log_notice (lslm->log_class, "plugin initialized");
  return error;
}

static clib_error_t *
link_pulse_show_scripts_command_fn(vlib_main_t *vm, unformat_input_t *input,
				   vlib_cli_command_t *cmd)
{
  link_pulse_main_t *lslm = &link_pulse_main;
  u32 i;

  if (vec_len (lslm->callback_scripts) == 0)
    {
      vlib_cli_output (vm, "No callback scripts registered");
    }
  else
    {
      vlib_cli_output (vm, "Registered callback scripts:");
      for (i = 0; i < vec_len (lslm->callback_scripts); i++)
	{
	  if (lslm->callback_scripts[i])
	    vlib_cli_output (vm, "  - %s ${ifname} ${status}", lslm->callback_scripts[i]);
	}
    }

  return 0;
}

static clib_error_t *
link_pulse_update_script_command_fn(vlib_main_t *vm, unformat_input_t *input,
                                           vlib_cli_command_t *cmd) {
  clib_error_t *error = NULL;
  link_pulse_main_t *lslm = &link_pulse_main;
  u8 *script_path = 0;
  unformat_input_t _line_input, *line_input = &_line_input;
  u8 is_add = 0;

  if (!unformat_user(input, unformat_line_input, line_input))
    return 0;

  if (unformat (line_input, "add %s", &script_path))
    is_add = 1;
  else if (unformat (line_input, "del %s", &script_path) ||
	   unformat (line_input, "delete %s", &script_path))
    is_add = 0;
  else
    {
      error = clib_error_return (0, "unknown input `%U'",
				 format_unformat_error, line_input);
      goto done;
    }

  if (unformat_check_input (line_input) != UNFORMAT_END_OF_INPUT)
    {
      error = clib_error_return (0, "unknown input `%U'",
				 format_unformat_error, line_input);
      goto done;
    }

  /* ensure NUL-terminated */
  vec_add1 (script_path, 0);

  if (is_add)
    {
      if (vec_len (lslm->callback_scripts) >=
	  link_pulse_CALLBACK_SCRIPT_NUM)
	{
	  error = clib_error_return (0, "Max number of scripts reached");
	  goto done;
	}

      /* de-dup */
      u32 i;
      for (i = 0; i < vec_len (lslm->callback_scripts); i++)
	{
	  if (lslm->callback_scripts[i] &&
	      strcmp ((char *) lslm->callback_scripts[i],
		      (char *) script_path) == 0)
	    {
	      error = clib_error_return (0, "Script already exists");
	      goto done;
	    }
	}

      vec_add1 (lslm->callback_scripts, script_path);
      script_path = NULL; /* ownership transferred */
    }
  else
    {
      u32 i;
      u8 found = 0;
      for (i = 0; i < vec_len (lslm->callback_scripts); i++)
	{
	  if (lslm->callback_scripts[i] &&
	      strcmp ((char *) lslm->callback_scripts[i],
		      (char *) script_path) == 0)
	    {
	      vlib_log_notice (lslm->log_class, "removed callback script '%s'", 
			       lslm->callback_scripts[i]);
	      vec_free (lslm->callback_scripts[i]);
	      /* Use vec_delete to maintain order */
	      vec_delete (lslm->callback_scripts, 1, i);
	      found = 1;
	      break;
	    }
	}
      if (!found)
	{
	  error = clib_error_return (0, "Script not found");
	  goto done;
	}
    }

done:
  if (script_path)
    vec_free (script_path);
  unformat_free (line_input);
  return error;
}

static clib_error_t *
link_pulse_hw_interface_link_up_down (vnet_main_t * vnm, u32 hw_if_index, u32 flags)
{
  clib_error_t *error = NULL;
  link_pulse_main_t *lslm = &link_pulse_main;
  vnet_hw_interface_t *hw = vnet_get_hw_interface(vnm, hw_if_index);
  u8 *if_name = NULL;
  u8 is_up = (flags & VNET_HW_INTERFACE_FLAG_LINK_UP) != 0;
  u32 i;

  /* hw->name is a u8* vector, not a fixed C string */
  if_name = format (0, "%v%c", hw->name, 0);

  vlib_log_notice (lslm->log_class, "interface '%s' link status changed to '%s'",
		   if_name, is_up ? "up" : "down");

  for (i = 0; i < vec_len (lslm->callback_scripts); i++)
  {
    if (lslm->callback_scripts[i])
    {
      link_pulse_run_script(lslm->callback_scripts[i], if_name, is_up);
    }
  }

  vec_free (if_name);
  return error;
}

VNET_HW_INTERFACE_LINK_UP_DOWN_FUNCTION (link_pulse_hw_interface_link_up_down);

VLIB_CLI_COMMAND (link_pulse_enable_disable_command, static) = {
  .path = "link_pulse script",
  .short_help =
    "link_pulse script <add|del|delete> <script-path>",
  .function = link_pulse_update_script_command_fn,
};

VLIB_CLI_COMMAND (link_pulse_show_scripts_command, static) = {
  .path = "show link_pulse scripts",
  .short_help = "show link_pulse scripts - Show registered callback scripts",
  .function = link_pulse_show_scripts_command_fn,
};

VLIB_INIT_FUNCTION (link_pulse_init);

VLIB_PLUGIN_REGISTER () = {
  .version = VPP_BUILD_VER,
  .description = "Link Status Listener Plugin",
};

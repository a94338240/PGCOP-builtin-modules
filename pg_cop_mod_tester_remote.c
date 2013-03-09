/*
    PGCOP - PocoGraph Component Oriented Platform.
    Copyright (C) 2013  David Wu <david@pocograph.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "pg_cop_modules.h"
#include "pg_cop_interface.h"
#include <string.h>
#include <stdlib.h>

static int tester_remote_init(int argc, char **argv);
static void *tester_remote_start(pg_cop_module_t *module);
static char *tester_remote_ping(char *param);

const pg_cop_module_hooks_t pg_cop_module_hooks = {
  .init = tester_remote_init,
  .start = tester_remote_start
};

const pg_cop_module_info_t pg_cop_module_info = {
  .name = "mod_tester_remote"
};

static int tester_remote_init(int argc, char **argv) 
{
  MOD_DEBUG_INFO("Module init OK");
  return 0;
}

static void *tester_remote_start(pg_cop_module_t *module)
{
  char *method;
  char *res;
  char *param1;

  pg_cop_module_interface_t *intf = 
    pg_cop_module_interface_announce(module->info->name, 
                                     MODULE_INTERFACE_TYPE_THREAD);

  for (;;) {
    MOD_DEBUG_INFO("Wait a ping.");
    if (pg_cop_module_interface_wait(intf, &method) != 0)
      goto out;

    if (strcmp(method, "ping") == 0) {
      pg_cop_module_interface_pop(intf, VSTACK_TYPE_STRING, &param1);
      res = tester_remote_ping(param1);
      pg_cop_module_interface_return(intf, 1, VSTACK_TYPE_STRING, res);
      MOD_DEBUG_INFO("Give a pong.");
      free(param1);
      free(res);
    } else {
      MOD_DEBUG_ERROR("No that method named %s.", method);
      pg_cop_module_interface_return(intf, 0);
    }
  
    free(method);
  }

 out:
  DEBUG_INFO("Service down.");
  pg_cop_module_interface_revoke(intf);
  return NULL;
}

static char *tester_remote_ping(char *param)
{
  char *res = (char *)malloc(128);
  sprintf(res, "pong %s", param);
  return res;
}

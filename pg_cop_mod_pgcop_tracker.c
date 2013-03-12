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
#include "pg_cop_seeds.h"
#include <string.h>
#include <stdlib.h>
#include <socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static int init(int argc, char **argv);
static void *start(pg_cop_module_t *module);
static int announce_seed(int sockfd, char *infohash, int port);
static int get_announced_peers(pg_cop_module_interface_t *intf,
                               char *hashinfo);

const pg_cop_module_hooks_t pg_cop_module_hooks = {
  .init = init,
  .start = start
};

const pg_cop_module_info_t pg_cop_module_info = {
  .name = "mod_pgcop_tracker"
};

typedef struct {
  char *infohash;
  char *host;
  int port;
  struct list_head list_head;
} _announced_seeds_t;
static _announced_seeds_t *announced_seeds;

static int init(int argc, char **argv) 
{
  MOD_DEBUG_INFO("Module init OK");
  announced_seeds = malloc(sizeof(_announced_seeds_t));
  INIT_LIST_HEAD(&announced_seeds->list_head);
  
  return 0;
}

static void *start(pg_cop_module_t *module)
{
  char *method;
  int res;
  char *param_str[1];
  int param_i32[1];

  pg_cop_module_interface_t *intf = 
    pg_cop_module_interface_announce(module->info->name, 
                                     MODULE_INTERFACE_TYPE_THREAD);

  for (;;) {
    MOD_DEBUG_INFO("Wait a announcement.");
    if (pg_cop_module_interface_wait(intf, &method) != 0)
      goto out;

    if (strcmp(method, "announce_seed") == 0) {
      pg_cop_module_interface_pop(intf, VSTACK_TYPE_STRING, &param_str[0]);
      pg_cop_module_interface_pop(intf, VSTACK_TYPE_I32, &param_i32[0]);
      res = announce_seed(intf->peer->connection_id,
                          param_str[0], param_i32[0]);
      pg_cop_module_interface_return(intf, 1, VSTACK_TYPE_I32, res);
      free(param_str[0]);
    } 
    else if (strcmp(method, "get_announced_peers")) {
      pg_cop_module_interface_pop(intf, VSTACK_TYPE_STRING, &param_str[0]);
      get_announced_peers(intf, param_str[0]);
      pg_cop_module_interface_return(intf, 0);
      free(param_str[0]);
    } 
    else {
      MOD_DEBUG_ERROR("No that method named %s.", method);
      pg_cop_module_interface_return(intf, 0);
    }
  
    free(method);
  }

 out:
  MOD_DEBUG_INFO("Module shut down.");
  pg_cop_module_interface_revoke(intf);
  return NULL;
}

static int announce_seed(int sockfd, char *infohash, int port)
{
  int res = 1;
  _announced_seeds_t *announcement;
  _announced_seeds_t *announced;
  struct sockaddr addr;
  struct sockaddr_in *inet_addr;
  char *host;
  socklen_t addrlen = sizeof(addr);  
  inet_addr = (struct sockaddr_in *)&addr;
  getpeername(sockfd, &addr, &addrlen);
  host = inet_ntoa(inet_addr->sin_addr);

  list_for_each_entry(announced, &announced_seeds->list_head, list_head) {
    if (strcmp(announced->infohash, infohash) == 0 &&
        strcmp(announced->host, host) == 0) {
      DEBUG_INFO("Duplicated announced peer?");
      return 0;
    }
  }

  announcement = malloc(sizeof(_announced_seeds_t));
  announcement->infohash = strdup(infohash);
  announcement->host = strdup(host);
  announcement->port = port;
  INIT_LIST_HEAD(&announcement->list_head);
  list_add_tail(&announcement->list_head, &announced_seeds->list_head);
  MOD_DEBUG_INFO("Seed with infohash=%s announced, host=%s, port=%d.", 
                 announcement->infohash,
                 announcement->host,
                 announcement->port);
  return res;
}

static int get_announced_peers(pg_cop_module_interface_t *intf,
                               char *hashinfo)
{
  _announced_seeds_t *announced;
  list_for_each_entry(announced, &announced_seeds->list_head, list_head) {
    if (strcmp(announced->infohash, hashinfo) == 0) {
      pg_cop_module_interface_push(intf, VSTACK_TYPE_STRING, announced->host);
      pg_cop_module_interface_push(intf, VSTACK_TYPE_I32, announced->port);
    }
  }

  return 0;
}

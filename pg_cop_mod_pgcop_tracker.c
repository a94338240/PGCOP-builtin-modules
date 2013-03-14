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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static int init(int argc, char **argv);
static void *start(pg_cop_module_t *module);
static int announce_seed(int sockfd, char *infohash, int port);
static int revoke_seed(int sockfd, char *infohash, int port);
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
	char *method = NULL;
	int res;
	char *param_str[1] = {0};
	int param_i32[1];

	pg_cop_module_interface_t *intf =
	    pg_cop_module_interface_announce(module->info->name,
	                                     MODULE_INTERFACE_TYPE_THREAD);
	if (!intf)
		goto announce_intf;

	for (;;) {
		MOD_DEBUG_INFO("Wait a announcement.");
		if (pg_cop_module_interface_wait(intf, &method) != 0)
			goto wait_a_request;

		if (strcmp(method, "announce_seed") == 0) {
			if (pg_cop_module_interface_pop(intf, VSTACK_TYPE_STRING, &param_str[0]))
				goto pop_method_cont;
			pg_cop_module_interface_pop(intf, VSTACK_TYPE_I32, &param_i32[0]);
			res = announce_seed(intf->peer->connection_id,
			                    param_str[0], param_i32[0]);
			if (pg_cop_module_interface_return(intf, 1, VSTACK_TYPE_I32, res))
				goto return_res_cont;
			free(param_str[0]);
			param_str[0] = NULL;
		} else if (strcmp(method, "revoke_seed") == 0) {
			if (pg_cop_module_interface_pop(intf, VSTACK_TYPE_STRING, &param_str[0]))
				goto pop_method_cont;
			pg_cop_module_interface_pop(intf, VSTACK_TYPE_I32, &param_i32[0]);
			res = revoke_seed(intf->peer->connection_id,
			                  param_str[0], param_i32[0]);
			if (pg_cop_module_interface_return(intf, 1, VSTACK_TYPE_I32, res))
				goto return_res_cont;
			free(param_str[0]);
			param_str[0] = NULL;
		} else if (strcmp(method, "get_announced_peers") == 0) {
			if (pg_cop_module_interface_pop(intf, VSTACK_TYPE_STRING, &param_str[0]))
				goto pop_method_cont;
			get_announced_peers(intf, param_str[0]);
			if (pg_cop_module_interface_return(intf, 0))
				goto return_res_cont;
			free(param_str[0]);
			param_str[0] = NULL;
		} else {
			MOD_DEBUG_ERROR("No that method named %s.", method);
			pg_cop_module_interface_return(intf, 0);
		}

		free(method);
		method = NULL;
		continue;
return_res_cont:
		if (param_str[0]) {
			free(param_str[0]);
			param_str[0] = NULL;
		}
		pg_cop_vstack_clear(intf->vstack);
pop_method_cont:
		free(method);
	}

wait_a_request:
	pg_cop_module_interface_revoke(intf);
announce_intf:
	MOD_DEBUG_INFO("Module shut down.");
	pthread_exit(0);
	return NULL;
}

static int announce_seed(int sockfd, char *infohash, int port)
{
	struct sockaddr addr;
	struct sockaddr_in *inet_addr = (struct sockaddr_in *)&addr;
	socklen_t addrlen = sizeof(addr);
	getpeername(sockfd, &addr, &addrlen);
	char *host = inet_ntoa(inet_addr->sin_addr);

	_announced_seeds_t *announced;
	list_for_each_entry(announced, &announced_seeds->list_head, list_head) {
		if (strcmp(announced->infohash, infohash) == 0 &&
		        strcmp(announced->host, host) == 0) {
			DEBUG_INFO("Duplicated announced peer?");
			goto check_dup;
		}
	}

	_announced_seeds_t *announcement = malloc(sizeof(_announced_seeds_t));
	announcement->infohash = strdup(infohash);
	announcement->host = strdup(host);
	announcement->port = port;
	INIT_LIST_HEAD(&announcement->list_head);
	list_add_tail(&announcement->list_head, &announced_seeds->list_head);
	MOD_DEBUG_INFO("Seed with infohash=%s announced, host=%s, port=%d.",
	               announcement->infohash,
	               announcement->host,
	               announcement->port);

	return 0;

check_dup:
	return 0;
}

static int revoke_seed(int sockfd, char *infohash, int port)
{
	struct sockaddr addr;
	struct sockaddr_in *inet_addr = (struct sockaddr_in *)&addr;
	socklen_t addrlen = sizeof(addr);
	getpeername(sockfd, &addr, &addrlen);
	char *host = inet_ntoa(inet_addr->sin_addr);

	_announced_seeds_t *announced, *announced_tmp;
	list_for_each_entry_safe(announced, announced_tmp, &announced_seeds->list_head, list_head) {
		if (strcmp(announced->infohash, infohash) == 0 &&
		        strcmp(announced->host, host) == 0) {
			DEBUG_INFO("Revoke announced seed.");
			list_del(&announced->list_head);
			free(announced->infohash);
			free(announced->host);
			free(announced);
			break;
		}
	}

	return 0;
}

static int get_announced_peers(pg_cop_module_interface_t *intf,
                               char *infohash)
{
	_announced_seeds_t *announced;
	list_for_each_entry(announced, &announced_seeds->list_head, list_head) {
		if (strcmp(announced->infohash, infohash) == 0) {
			pg_cop_module_interface_push(intf, VSTACK_TYPE_STRING, announced->host);
			pg_cop_module_interface_push(intf, VSTACK_TYPE_I32, announced->port);
		}
	}

	return 0;
}

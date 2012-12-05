#include "pg_cop_hooks.h"
#include "pg_cop_debug.h"
#include "pg_cop_rodata_strings.h"
#include "pg_cop_config.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

struct accepted_cli {
  int fd;
  pg_cop_module_t *module;
};

static int transceiver_start();

const pg_cop_module_info_t pg_cop_module_info = {
  .magic = 0xF7280102,/* FIXME */
  .type = PG_COP_MODULE_TYPE_TRANSCEIVER,
  .name = "mod_transceiver"
};

const pg_cop_module_trans_hooks_t pg_cop_module_hooks = {
  .start = transceiver_start
};

static void *transceiver_process(void *acc_cli)
{
  char welcome_info[255] = {};
  struct accepted_cli cli;
  pg_cop_data_in_t data_in;
  pg_cop_data_out_t data_out;
  char block_buffer[4096];
  char private_data[4096];
  int read_size = 0;

  memcpy(&cli, (struct accepted_cli *)acc_cli, sizeof(cli));

  sprintf(welcome_info, rodata_str_module_serv_to_you_format, 
          cli.module->info->name);

  pg_cop_hook_com_send(cli.module, cli.fd, 
                       rodata_str_service_welcome_message, 
                       rodata_size_str_service_welcome_message, 0);
  pg_cop_hook_com_send(cli.module, cli.fd, welcome_info, 
                       sizeof(welcome_info), 0);

  while ((read_size = pg_cop_hook_com_recv(cli.module, 
                                           cli.fd, block_buffer, 
                                           sizeof(block_buffer), 
                                           0)) > 0) {
    data_in.data = block_buffer;
    data_in.size = read_size;
    data_in.private_data = private_data;

    PG_COP_EACH_MODULE_BEGIN(pg_cop_modules_list_for_proto);
    if (!pg_cop_hook_proto_process(_module, data_in, &data_out, 0)) {
      /* TODO Service Process */
      pg_cop_hook_proto_sweep(_module, data_out);
    } else {
      MOD_DEBUG_DEBUG(rodata_str_protocol_process_skipped);
    }
    PG_COP_EACH_MODULE_END;
  }

  MOD_DEBUG_INFO(rodata_str_client_disconnected);

  return NULL;
}

static void *transceiver_routine(void *module)
{
  int fd;
  int s;
  pthread_t child_thread;
  pthread_attr_t child_thread_attr;
  struct accepted_cli accepted_cli;

  if (pg_cop_hook_com_bind(module) != 0)
    return NULL;

  for (;;) {
    fd = pg_cop_hook_com_accept(module);
    if (fd < 0)
      MOD_DEBUG_ERROR(rodata_str_accept_error);

    /* FIXME Stack size. */
    s = pthread_attr_init(&child_thread_attr);
    if (s != 0) {
      MOD_DEBUG_ERROR(rodata_str_cannot_create_thread);
      continue;
    }

    accepted_cli.fd = fd;
    accepted_cli.module = module;
  
    if (pthread_create(&child_thread, &child_thread_attr,
                       transceiver_process, &accepted_cli)) {
      MOD_DEBUG_ERROR(rodata_str_cannot_create_thread);
      continue;
    }
  }
  return NULL;
}

static int transceiver_start()
{
  int s;
  char debug_info[MAXLEN_LOAD_MODULE_DEBUG_INFO];
  int count = 0;
  char *config_mode;

  if (pg_cop_get_module_config_strdup
      ("service.mode", &config_mode))
    goto out;

  if (strcmp(config_mode, "server"))
    goto out;

  PG_COP_EACH_MODULE_BEGIN(pg_cop_modules_list_for_com);
  /* FIXME Stack size. */
  s = pthread_attr_init(&_module->thread_attr);
  if (s != 0) {
    MOD_DEBUG_ERROR(rodata_str_cannot_create_thread);
    continue;
  }
  
  if (pthread_create(&_module->thread, &_module->thread_attr,
                     transceiver_routine, _module)) {
    MOD_DEBUG_ERROR(rodata_str_cannot_create_thread);
    continue;
  }
  
  count++;
  PG_COP_EACH_MODULE_END;

  sprintf(debug_info, rodata_str_com_module_enabled, count);
  MOD_DEBUG_INFO(debug_info);

 out:
  if (config_mode)
    free(config_mode);

  return 0;
}

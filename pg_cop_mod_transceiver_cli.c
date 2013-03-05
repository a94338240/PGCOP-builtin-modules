#include "pg_cop_hooks.h"
#include "pg_cop_debug.h"
#include "pg_cop_rodata_strings.h"
#include "pg_cop_config.h"
#include "pg_cop_modules.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

static int transceiver_start();

const pg_cop_module_info_t pg_cop_module_info = {
  .magic = 0xF7280103,/* FIXME */
  .type = PG_COP_MODULE_TYPE_TRANSCEIVER,
  .name = "mod_transceiver_cli"
};

const pg_cop_module_trans_hooks_t pg_cop_module_hooks = {
  .start = transceiver_start
};

static void *transceiver_routine(void *module)
{
  int fd;
  char private_data[4096] = {0};
  char block_buffer[4096] = {0};
  pg_cop_data_in_t data_in;
  pg_cop_data_out_t data_out = {0};
  int write_size = 1;
  pg_cop_module_t *_module;

  if ((fd = pg_cop_hook_com_connect(module)) < 0) {
    MOD_DEBUG_ERROR("Cannot connect to remote server.");
    return NULL;
  }

  while(write_size > 0) {
    data_in.data = "test";
    data_in.size = 5;
    data_in.private_data = private_data;

    list_for_each_entry(_module, &pg_cop_modules_list_for_proto->list_head,
                        list_head) {
      data_out.data = NULL;
      data_out.size = 0;
      data_out.private_data = private_data;
      // TODO Service module
      pg_cop_hook_proto_pack(_module, data_in, &data_out);
      /* FIXME size */
      memcpy(block_buffer, data_out.data, data_out.size);
      pg_cop_hook_proto_sweep(_module, data_out);
    }
    
    write_size = pg_cop_hook_com_send(module, fd, 
                                      block_buffer, sizeof(block_buffer), 0);
  }

  return NULL;
}

static int transceiver_start()
{
  int s;
  char debug_info[MAXLEN_LOAD_MODULE_DEBUG_INFO];
  int count = 0;
  char *config_mode = NULL;
  pg_cop_module_t *module;

  if (pg_cop_get_module_config_strdup
      ("service.mode", &config_mode))
    goto out;

  if (strcmp(config_mode, "client"))
    goto out;

  list_for_each_entry(module, &pg_cop_modules_list_for_com->list_head,
                      list_head) {
    /* FIXME Stack size. */
    s = pthread_attr_init(&module->thread_attr);
    if (s != 0) {
      MOD_DEBUG_ERROR(rodata_str_cannot_create_thread);
      continue;
    }
    
    if (pthread_create(&module->thread, &module->thread_attr,
                       transceiver_routine, module)) {
      MOD_DEBUG_ERROR(rodata_str_cannot_create_thread);
      continue;
    }
    
    count++;
  }
  
  MOD_DEBUG_INFO(rodata_str_com_module_enabled, count);

 out:
  if (config_mode)
    free(config_mode);

  return 0;
}

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
  struct accepted_cli cli;
  pg_cop_data_in_t data_in;
  pg_cop_data_out_t data_out;
  char block_buffer[4096] = {0};
  char private_data[4096] = {0};
  int read_size = 0;
  pg_cop_module_t *module;
  pg_cop_transfer_request_t *request;
  pg_cop_transfer_request_t request_queue;
  pg_cop_transfer_request_t response_queue;
  int write_size = 0;
  sem_t request_sem;
  sem_init(&request_sem, 0, 0);

  INIT_LIST_HEAD(&request_queue.list_head);
  INIT_LIST_HEAD(&response_queue.list_head);
  cli = *((struct accepted_cli *)acc_cli);

  list_for_each_entry(module, &pg_cop_modules_list_for_service->list_head,
                      list_head) {
    pg_cop_hook_service_start(module, &request_sem, &request_queue, &response_queue);
  }

  while (1) {
    sem_wait(&request_sem);
    list_for_each_entry(request, &request_queue.list_head, list_head) {
      data_in.data = request->out_data;
      data_in.size = request->out_data_size;
      data_in.private_data = private_data;
      data_out.data = NULL;
      data_out.size = 0;
      data_out.private_data = private_data;

      switch (request->type) {
      case TRANSFER_REQUEST_TYPE_RESPONSE:
      case TRANSFER_REQUEST_TYPE_NOTIFY:
      case TRANSFER_REQUEST_TYPE_REQUEST:
        list_for_each_entry(module, &pg_cop_modules_list_for_proto->list_head,
                            list_head) {
          /* TODO Multi protocol */
          if (strcmp(module->info->name, request->proto_name_seq[0]))
            continue;
          pg_cop_hook_proto_pack(module, data_in, &data_out);
          memcpy(block_buffer, data_out.data, data_out.size);
          pg_cop_hook_proto_sweep(module, data_out);
          write_size = pg_cop_hook_com_send(cli.module, cli.fd, 
                                            block_buffer, sizeof(block_buffer), 0);
          if (write_size <=0)
            MOD_DEBUG_ERROR("Request not be written.");
          break;
        }

        if (request->type != TRANSFER_REQUEST_TYPE_REQUEST)
          break;

      case TRANSFER_REQUEST_TYPE_WAIT:
        read_size = pg_cop_hook_com_recv(cli.module, 
                                         cli.fd, block_buffer, 
                                         sizeof(block_buffer), 0);
        if (read_size <= 0) {
          MOD_DEBUG_INFO("Read error occured.");
          break;
        }

        list_for_each_entry(module, &pg_cop_modules_list_for_proto->list_head,
                            list_head) {
          /* TODO Multi protocol */
          if (strcmp(module->info->name, request->proto_name_seq[0]))
            continue;
          if (!pg_cop_hook_proto_unpack(module, data_in, &data_out, 0)) {
            request->in_data = data_out.data;
            request->in_data_size = data_out.size;
            list_move_tail(&request->list_head, &response_queue.list_head);
          } else {
            MOD_DEBUG_DEBUG(rodata_str_protocol_process_skipped);
          }
          pg_cop_hook_proto_sweep(module, data_out);
        }

        break;   
      default:
        MOD_DEBUG_CRITICAL("No that kind of request type.");
      }
    }
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

    pthread_attr_destroy(&child_thread_attr);
  }
  return NULL;
}

static int transceiver_start()
{
  int s = 0;
  int count = 0;
  char *config_mode = NULL;
  pg_cop_module_t *module = NULL;

  if (pg_cop_get_module_config_strdup
      ("service.mode", &config_mode))
    goto out;

  if (strcmp(config_mode, "server"))
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

    pthread_attr_destroy(&module->thread_attr);
    count++;
  }
    
  MOD_DEBUG_INFO(rodata_str_com_module_enabled, count);

 out:
  if (config_mode)
    free(config_mode);

  return 0;
}

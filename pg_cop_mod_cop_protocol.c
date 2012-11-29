#include "pg_cop_hooks.h"
#include "pg_cop_modules.h"
#include "pg_cop_debug.h"
#include "pg_cop_rodata_strings.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
  int is_sub;
  int parent_proto_magic;
} cop_proto_private_data_t;

cop_proto_private_data_t private_data = {0, 0};

static int cop_main_proto_process(pg_cop_data_in_t in, 
                                  pg_cop_data_out_t *out,
                                  int sub_lvl);
static int cop_main_proto_sweep(pg_cop_data_out_t out);

const pg_cop_module_info_t pg_cop_module_info = {
  .magic = 0xF7280201, /* FIXME */
  .type = PG_COP_MODULE_TYPE_PROTO,
  .name = "mod_cop_main_protocol",
  .private_data = (void *)&private_data
};

const pg_cop_module_proto_hooks_t pg_cop_module_hooks = {
  .process = cop_main_proto_process,
  .sweep = cop_main_proto_sweep
};

static int cop_main_proto_process(pg_cop_data_in_t in, 
                                  pg_cop_data_out_t *out,
                                  int sub_lvl)
{
  static char *working_buf = NULL; /* FIXME */
  static int asize = 0; /* FIXME */
  static int last_asize = 0;
  unsigned char *in_data = (unsigned char *)in.data;
  out->data = NULL;
  out->data = 0;

  if (sub_lvl != 0)
    return 0;

  if (in.size < 17)
    return 0;

  if (in_data[0] != 0xC7 || 
      in_data[1] != 0x28 ||
      in_data[2] != 0x07 || 
      in_data[3] != 0x02)
    return 0;

  if (*((int *)&in_data[9]) != in.size - 13)
    return 0;

  /* TODO Check sum */

  last_asize = asize;
  asize += *((int *)&in_data[9]) - 4;

  if (in_data[4] & 0x80) {
    if (working_buf == NULL)
      working_buf = (char *)malloc(asize); // FIXME freed, but...
    else
      working_buf = (char *)realloc(working_buf, asize); // FIXME freed, but...

    if (working_buf == NULL)
      return 0;
    
    memcpy(&working_buf[last_asize], &in_data[13], 
           *((int *)&in_data[9]));

    if (in_data[4] & 0x40) {
      PG_COP_EACH_MODULE_BEGIN(pg_cop_modules_list_for_proto);
      pg_cop_hook_proto_process(_module, in, out, 1);
      PG_COP_EACH_MODULE_END;
    }
  } else {
    working_buf = (char *)malloc(asize); // FIXME freed, but...
    if (working_buf == NULL)
      return 0;

    memcpy(&working_buf[last_asize], &in_data[13], 
           *((int *)&in_data[9]));
    PG_COP_EACH_MODULE_BEGIN(pg_cop_modules_list_for_proto);
    pg_cop_hook_proto_process(_module, in, out, 1);
    PG_COP_EACH_MODULE_END;
  }

  if (working_buf)
    free(working_buf);
    
  return 0;
}

static int cop_main_proto_sweep(pg_cop_data_out_t out)
{
  if (out.data)
    free(out.data);

  return 0;
}

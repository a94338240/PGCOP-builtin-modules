#include "pg_cop_hooks.h"
#include "pg_cop_modules.h"
#include "pg_cop_debug.h"
#include "pg_cop_rodata_strings.h"

#include <stdlib.h>
#include <string.h>

#pragma pack(push, 1)

typedef struct {
  unsigned int magic_num;
  char flag_mlt:1;
  char flag_fin:1;
  char flag_los:1;
  char flag_ext:1;
  char flag_reserved:4;
  unsigned int index;
  unsigned int length;
} cop_proto_header;

#pragma pack(pop)

static int cop_main_proto_unpack(pg_cop_data_in_t in, 
                                 pg_cop_data_out_t *out,
                                 int sub_lvl);
static int cop_main_proto_sweep(pg_cop_data_out_t out);
static int cop_main_proto_pack(pg_cop_data_in_t in, 
                               pg_cop_data_out_t *out);

const pg_cop_module_info_t pg_cop_module_info = {
  .magic = 0xF7280201, /* FIXME */
  .type = PG_COP_MODULE_TYPE_PROTO,
  .name = "mod_cop_main_protocol"
};

const pg_cop_module_proto_hooks_t pg_cop_module_hooks = {
  .unpack = cop_main_proto_unpack,
  .sweep = cop_main_proto_sweep,
  .pack = cop_main_proto_pack
};

static int cop_main_proto_unpack(pg_cop_data_in_t in, 
                                  pg_cop_data_out_t *out,
                                  int sub_lvl)
{
  int *asize = in.private_data; /* FIXME */
  char **working_buf = (void *)asize + sizeof(int);
  // int *index = working_buf + sizeof(char *);
  int last_asize = 0;
  char *last_working_buf = NULL;
  char *pdata;
  cop_proto_header *header;
  pg_cop_module_t *module;
  pg_cop_data_in_t internal_in;
  internal_in.private_data = (void *)working_buf + sizeof(char *);

  if (sub_lvl != 0)
    return 0;

  out->data = NULL;
  out->size = 0;

  if (in.size < sizeof(cop_proto_header) || 
      in.size > 4096) {
    MOD_DEBUG_ERROR("Input stream has wrong size.");
    return -1;
  }

  if (in.data == NULL) {
    MOD_DEBUG_ERROR("Input stream has no data.");
    return -1;
  }

  pdata = in.data;
  header = (cop_proto_header *)malloc(sizeof(cop_proto_header));
  memcpy(header, pdata, sizeof(cop_proto_header));
  pdata += sizeof(cop_proto_header);

  if (header->magic_num != 0xC7280702) {
    MOD_DEBUG_ERROR("Input stream has wrong magic number, MAGIC=0x%08x", 
                    header->magic_num);
    return -1;
  }

  if (header->length > in.size - sizeof(cop_proto_header)) {
    MOD_DEBUG_ERROR("Input data has wrong size.");
    return -1;
  }

  last_asize = *asize;
  *asize += header->length;
  if (header->flag_mlt) {
    if (*working_buf == NULL)
      *working_buf = (char *)malloc(*asize); // FIXME freed, but...
    else
      *working_buf = (char *)realloc(*working_buf, *asize); // FIXME freed, but...

    if (*working_buf == NULL) {
      MOD_DEBUG_ERROR("Memory cannot be allocated!");
      if (last_working_buf)
        free(last_working_buf);
      return -1;
    }

    last_working_buf = *working_buf;
    
    memcpy((*working_buf) + last_asize, pdata, 
           header->length);
    pdata += header->length;

    /* TODO Check sum */

    if (header->flag_fin) {
      out->data = *working_buf;
      out->size = *asize;
      internal_in.data = out->data;
      internal_in.size = out->size;

      *asize = 0;
    }
  } else {
    *working_buf = (char *)malloc(*asize); // FIXME freed, but...
    if (*working_buf == NULL) {
      MOD_DEBUG_ERROR("Cannot allocate memory.");
      return -1;
    }
    memset(*working_buf, 0, *asize);

    memcpy((*working_buf) + last_asize, pdata, 
           header->length);
    pdata += header->length;

    /* TODO Check sum */
    out->data = *working_buf;
    out->size = *asize;
    internal_in.data = out->data;
    internal_in.size = out->size;
    
    *asize = 0;
  }

  if (header)
    free(header);
    
  return 0;
}

static int cop_main_proto_pack(pg_cop_data_in_t in, 
                               pg_cop_data_out_t *out) {
  cop_proto_header header = {};
  char *data;
  char *pdata;

  data = (char *)malloc(sizeof(cop_proto_header) + 
                        in.size + 4);
  pdata = data;

  header.magic_num = 0xC7280702;
  header.flag_mlt = 0;
  header.flag_los = 0;
  header.flag_ext = 0;
  header.length = in.size;

  memcpy(pdata, &header, sizeof(header));
  pdata += sizeof(header);
  memcpy(pdata, in.data, in.size);

  out->data = data;
  out->size = sizeof(header) + in.size;
  
  return 0;
}

static int cop_main_proto_sweep(pg_cop_data_out_t out)
{
  if (out.data)
    free(out.data);

  return 0;
}

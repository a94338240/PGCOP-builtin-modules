#include "pg_cop_hooks.h"
#include "pg_cop_debug.h"
#include "pg_cop_config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>

static int socket_init(int argc, char *argv[]);
static int socket_bind();
static int socket_accept();
static int socket_send(int id, const void *buf, unsigned int len,
                       unsigned int flags);
static int socket_recv(int id, void *buf, unsigned int len,
                       unsigned int flags);
static int socket_connect();

const pg_cop_module_com_hooks_t pg_cop_module_hooks = {
  .init = socket_init,
  .bind = socket_bind,
  .accept = socket_accept,
  .send = socket_send,
  .recv = socket_recv,
  .connect = socket_connect
};

const pg_cop_module_info_t pg_cop_module_info = {
  .magic = 0xF7280001, /* FIXME call pg_cop_get_magic() */
  .type = PG_COP_MODULE_TYPE_COM,
  .name = "mod_socket"
};

static int sockfd;
static int sockfd_cli;
static struct sockaddr_in serv_addr;
static struct sockaddr_in remote_addr;

static int socket_init(int argc, char *argv[])
{
  int config_port;
  char *config_remote_host = NULL;
  int config_remote_port;
  static struct hostent *host_info;

  if (pg_cop_get_module_config_number
      ("mod_socket.server.port", &config_port))
    config_port = 12728;

  if (pg_cop_get_module_config_strdup
      ("mod_socket.remote.host", &config_remote_host))
    config_remote_host = strdup("127.0.0.1");

  if (pg_cop_get_module_config_number
      ("mod_socket.remote.port", &config_remote_port))
    config_remote_port = 12728;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  sockfd_cli = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1 || sockfd_cli == -1)
    return -1;

  bzero((char *)&serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(config_port);
  serv_addr.sin_addr.s_addr = INADDR_ANY;

  bzero((char *)&remote_addr, sizeof(remote_addr));
  host_info = gethostbyname(config_remote_host);
  memcpy(&remote_addr.sin_addr.s_addr, 
         host_info->h_addr, host_info->h_length);
  remote_addr.sin_family = AF_INET;
  remote_addr.sin_port = htons(config_remote_port);

  if (config_remote_host)
    free(config_remote_host);

  return 0;
}

static int socket_bind()
{
  if (bind(sockfd, (struct sockaddr *)&serv_addr,
           sizeof(serv_addr)) < 0) {
    MOD_DEBUG_ERROR("Failed to listen on port.");
    return -1;
  }

  if (listen(sockfd, 5)) {
    MOD_DEBUG_ERROR("Failed to listen on port.");
    return -1;
  }
  MOD_DEBUG_INFO("Listen on port %d", ntohs(serv_addr.sin_port));
  
  return 0;
}

static int socket_accept()
{
  struct sockaddr cli_addr;
  socklen_t clilen = sizeof(cli_addr);
  int cli_sockfd = accept(sockfd, &cli_addr, &clilen);
  if (cli_sockfd < 0)
    return -1;

  return cli_sockfd;
}

static int socket_send(int id, const void *buf, unsigned int len,
                unsigned int flags)
{  
  return send(id, buf, len, 0);
}

static int socket_recv(int id, void *buf, unsigned int len,
                unsigned int flags)
{
  return recv(id, buf, len, MSG_WAITALL);
}

static int socket_connect()
{
  if ((connect(sockfd, (struct sockaddr *)&remote_addr, 
               sizeof(remote_addr))))
    return -1;
  else
    return sockfd;
}

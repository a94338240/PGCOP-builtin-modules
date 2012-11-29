#include "pg_cop_hooks.h"
#include "pg_cop_debug.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/in.h>

static int socket_init(int argc, char *argv[]);
static int socket_bind();
static int socket_accept();
static int socket_send(int id, const void *buf, unsigned int len,
                       unsigned int flags);
static int socket_recv(int id, void *buf, unsigned int len,
                       unsigned int flags);

const pg_cop_module_com_hooks_t pg_cop_module_hooks = {
  .init = socket_init,
  .bind = socket_bind,
  .accept = socket_accept,
  .send = socket_send,
  .recv = socket_recv
};

const pg_cop_module_info_t pg_cop_module_info = {
  .magic = 0xF7280001, /* FIXME call pg_cop_get_magic() */
  .type = PG_COP_MODULE_TYPE_COM,
  .name = "mod_socket"
};

static int sockfd;
static struct sockaddr_in serv_addr;

static int socket_init(int argc, char *argv[])
{
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1)
    return -1;

  bzero((char *)&serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(12728);
  serv_addr.sin_addr.s_addr = INADDR_ANY;

  return 0;
}

static int socket_bind()
{
  if (bind(sockfd, (struct sockaddr *)&serv_addr,
           sizeof(serv_addr)) < 0)
    return -1;

  listen(sockfd, 5);
  MOD_DEBUG_INFO("Listen on port 12728");
  
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
  return recv(id, buf, len, 0);
}

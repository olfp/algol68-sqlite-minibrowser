#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

/* Creates a listening TCP socket on all interfaces at the given port.
   Yields the listening file descriptor, or -1 on error. */
int algol68_socket_listen(int port)
{
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return -1;

  int on = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof on);

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_port = htons((unsigned short) port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(fd, (struct sockaddr *) &addr, sizeof addr) < 0)
    {
      close(fd);
      return -1;
    }

  if (listen(fd, 16) < 0)
    {
      close(fd);
      return -1;
    }

  return fd;
}

/* Blocks until a connection arrives on the listening socket and
   yields the file descriptor of the accepted connection, or -1. */
int algol68_socket_accept(int listen_fd)
{
  struct sockaddr_in peer;
  socklen_t peer_len = sizeof peer;
  int fd = accept(listen_fd, (struct sockaddr *) &peer, &peer_len);
#ifdef SO_NOSIGPIPE
  if (fd >= 0)
    {
      int on = 1;
      setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, (char *) &on, sizeof on);
    }
#endif
  return fd;
}
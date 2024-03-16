#include "listener.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <cstring>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MAX_BACKLOG 10

// creates socket, binds to port, listens for new conncetions
// returns file descriptor
int create_listener(const char *port) {

  int listener_fd, status;
  struct addrinfo hints, *serverinfo, *ptr;

  // hints specifies the criteria for selecting socket address structures 
  // returned by the getaddrinfo function
  memset(&hints, 0, sizeof hints);  // ensure hints is empty
  hints.ai_flags = AI_PASSIVE;      // IP address of current host
  hints.ai_family = AF_INET;        // return the IPv4 address only
  hints.ai_socktype = SOCK_STREAM;  // TCP stream socket

  if ((status = getaddrinfo(NULL, port, &hints, &serverinfo)) != 0) {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    return -1;
  }

  // serverinfo contains linked list of socket address structures returned by 
  // getaddrinfo, iterate through and bind to first available socket
  for (ptr = serverinfo ; ptr != NULL ; ptr = ptr->ai_next) {
    listener_fd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    if (listener_fd == -1) { 
      perror("socket");
      continue; // try next entry in serverinfo
    }

    // allows reuse of local address, prevents 'address already in use' errors 
    // when server crashes
    int opt_val = 1;
    setsockopt(listener_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof opt_val);

    if (bind(listener_fd, ptr->ai_addr, ptr->ai_addrlen) == -1) {
      perror("bind");
      continue;
    }

    break; // successfully created and bound to socket
  }

  // free memeory allocated for serverinfo
  freeaddrinfo(serverinfo);

  if (ptr == NULL) { // ptr == NULL, never binded to socket
    fprintf(stderr, "failed to bind to socket\n");
    return -1;
  }

  if (listen(listener_fd, MAX_BACKLOG) == -1) {
    perror("listen");
    return -1;
  }

  return listener_fd;
}

// sockaddr_storage is at least as large as any other sockaddr struct, pointer
// can be cast to any other sockaddr pointer to access its fields
int log_connection(struct sockaddr_storage *client) {
  char buf[INET6_ADDRSTRLEN];
  if (client->ss_family == AF_INET) {
    inet_ntop(AF_INET, &((struct sockaddr_in*) client)->sin_addr, buf, sizeof buf);
  } else if (client->ss_family == AF_INET6) {
    inet_ntop(AF_INET6, &((struct sockaddr_in6*) client)->sin6_addr, buf, sizeof buf);
  } else {
    fprintf(stderr, "client protocol not supported");
    return -1;
  }

  printf("server: received connection from %s\n", buf);
  return 0;
}

// accepts and returns file descriptor of new socket if is a valid TCP 
// connection, else return -1 and a new socket is closed if created
int accept_new_connection(int _listener_fd) {
  socklen_t address_size;
  struct sockaddr_storage client_addr;

  // creates non-blocking socket for incoming connection
  int conn_fd = 
    accept4(_listener_fd, (struct sockaddr *)&client_addr, &address_size, SOCK_NONBLOCK);
  if (conn_fd == -1) {
    perror("accept");
    return -1;
  }

  if (log_connection(&client_addr) == -1) {
    int status = close(conn_fd);
    if (status == -1)
      perror("close");
    return -1;
  }

  return conn_fd;
}

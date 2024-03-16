#include <stdio.h>
#include <getopt.h>
#include <cstring>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/socket.h>

#include "listener.h"

#define MAX_EVENTS 10
#define BUF_SIZE 1024

static int DEBUG_FLAG = 0;
static const char *PORT_NUMBER = "5432";
static const char *PROTOCOL = "http";

void err_exit(const char *err_msg) {
  fprintf(stderr, "%s\n", err_msg);
  std::exit(EXIT_FAILURE);
}

void print_help() {
  puts("help message");
}

// handles command line options and arguments
void handle_options(int argc, char **argv) {

  // define options expected for program
  static struct option long_options[] = {
    {"help",        no_argument,       0, 'h'},
    {"debug",       no_argument,       &DEBUG_FLAG, 1},
    {"port-number", required_argument, 0, 'n'},
    {"protocol",    required_argument, 0, 'p'},
    {0, 0, 0, 0}
  };

  // getopt_long returns val defined in long_options of the options encountered
  // and strores it in option_char, option_index points to the option definition
  // of the encountered long option in the long_options array
  int option_char, option_index;

  while (true) {

    option_char = 
      getopt_long(argc, argv, "hdn:p:t:", long_options, &option_index);

    // all options handled, break
    if (option_char == -1) break;

    switch (option_char)
    {
    case 'h':
      print_help();
      break;
    case 'd':
      DEBUG_FLAG = 1;
      break;
    case 'n':
      PORT_NUMBER = optarg;
      break;
    case 'p':
      if (strcmp(optarg, "http") != 0)
        err_exit("protocol not supported");
      break;
    case '?':
      err_exit("option not supported");
      break;
    default:
      break;
    }
  }
}

void add_to_epoll_or_exit(int _epoll_fd, int _socket_fd) {
  struct epoll_event event;

  event.events = EPOLLIN; // enable read operation
  event.data.fd = _socket_fd;
  int status = epoll_ctl(_epoll_fd, EPOLL_CTL_ADD , _socket_fd, &event);
  if (status == -1) {
    perror("epoll_ctl:");
    exit(EXIT_FAILURE);
  }
    
}

int main(int argc, char **argv) {

  // handle command line options
  handle_options(argc, argv);

  printf("%s server listening port %s%s\n", PROTOCOL, PORT_NUMBER,
    DEBUG_FLAG ? ", starting in DEBUG mode" : "");

  int listener_fd, epoll_fd, num_fds, status;
  struct epoll_event events[MAX_EVENTS], rw_event;
  char buffer[BUF_SIZE];

  // rw_event allows reading and writing to socket, used for worker TCP sockets
  rw_event.events = EPOLLET | EPOLLIN | EPOLLOUT;

  listener_fd = create_listener(PORT_NUMBER);
  if (listener_fd == -1)
    err_exit("failed to create listener socket");

  // size argument is ignored in epoll_create
  epoll_fd = epoll_create(1);
  if (epoll_fd == -1)
    err_exit("failed to create epoll instance");

  // add listener and stdin to epoll
  add_to_epoll_or_exit(epoll_fd, listener_fd);
  add_to_epoll_or_exit(epoll_fd, STDIN_FILENO);

  while (1) {

    // num_fds indicates the number of file descriptors ready
    num_fds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

    if (num_fds == -1) {
      perror("epoll_wait");
      exit(EXIT_FAILURE);
    }

    for (int i = 0 ; i < num_fds ; ++i) {
      int fd = events[i].data.fd;
      if (fd == STDIN_FILENO) { 
        // handle input from stdin first
        int bytes_read = read(STDIN_FILENO, buffer, BUF_SIZE);
        printf("read from STDIN: %.*s\n", bytes_read, buffer);
      } else if (fd == listener_fd) {
        int conn_fd = accept_new_connection(listener_fd);
        if (conn_fd == -1) 
          continue;
        // conn_fd is a valid TCP socket, add to epoll
        rw_event.data.fd = conn_fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &rw_event) == -1) {
          // error adding conn_fd socket to epoll, close socket fd and continue
          perror("epoll_ctl: add socket:");
          if (close(conn_fd) == -1)
            perror("close");
        }
      } else {
        // ready fd is a connection socket
        char msg[] = 
          "HTTP/1.1 200 OK\r\n\r\n<html><body><p>Hello World!</p></body></html>\r\n";
        if (send(fd, msg, sizeof msg, 0) == -1)
          perror("send");
        // remove fd from epoll
        if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL) == -1)
          perror("epoll_ctl: delete socket");
        // close fd
        if (close(fd) == -1)
          perror("close");
      }
    } 
  }

  return 0;

}
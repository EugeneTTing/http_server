#ifndef LISTENER_H
#define LISTENER_H

/*
 * Create listener socket
 * Returns file descriptor of listener socker, or -1 if error
 */
int create_listener(const char *port);

int accept_new_connection(int _listener_fd);

#endif
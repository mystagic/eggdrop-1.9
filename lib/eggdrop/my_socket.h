#ifndef MY_SOCKET_H
#define MY_SOCKET_H

#define SOCKET_CLIENT	1
#define SOCKET_SERVER	2
#define SOCKET_BIND	4
#define SOCKET_NONBLOCK	8
#define SOCKET_TCP	16
#define SOCKET_UDP	32

int socket_create(const char *dest_ip, int dest_port, const char *src_ip, int src_port, int flags);
int socket_close(int sock);
int socket_set_nonblock(int desc, int value);
int socket_get_peer_name(int sock, char **peer_ip, int *peer_port);
int socket_get_error(int sock);
int socket_accept(int sock, char **peer_ip, int *peer_port);
int socket_valid_ip(const char *ip);

#endif
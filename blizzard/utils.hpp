#ifndef __BLIZZARD_UTILS_HPP__
#define __BLIZZARD_UTILS_HPP__

#include <stdint.h>
#include <coda/logger.h>

namespace lz_utils {

void uwait(long N, long M = 0);
uint64_t fine_clock();

void close_connection(int fd);
void set_socket_timeout(int fd, long timeout);
int set_nonblocking(int fd);
int add_listener(const char * host_desc, const char * port_desc, int listen_q_sz = 1024);
int add_sender(const char * host_desc, const char * port_desc);
int accept_new_connection(int fd, struct in_addr& ip);

}

#endif /* __BLIZZARD_UTILS_HPP__ */

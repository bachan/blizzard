#ifndef __LIZARD_UTILS_HPP___
#define __LIZARD_UTILS_HPP___

#include <stdint.h>
#include <coda/logger.h>

/* namespace blizzard { */
/* extern int MSG_LIZARD_ID; */
/* } */

namespace lz_utils {

void uwait(long N, long M = 0);
uint64_t fine_clock();

void close_connection(int fd);
void set_socket_timeout(int fd, long timeout);
int set_nonblocking(int fd);
int add_listener(const char * host_desc, const char * port_desc, int listen_q_sz = 1024);
int add_sender(const char * host_desc, const char * port_desc);
int accept_new_connection(int fd, struct in_addr& ip);

/* bool pid_file_init(const char * pid_fn); */
/* bool pid_file_check(const char * pid_fn); */
/* bool pid_file_free(const char * pid_fn); */

}

#endif

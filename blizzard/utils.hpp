#ifndef __BLIZZARD_UTILS_HPP__
#define __BLIZZARD_UTILS_HPP__

#include <stdint.h>
#include <sys/time.h>
#include <coda/logger.h>

namespace lz_utils {

static inline
void uwait(long N, long M = 0)
{
	struct timespec ts = {N, M};
	nanosleep(&ts, 0);
}

static inline
uint64_t fine_clock()
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	return (tv.tv_sec * 1000000LLU + tv.tv_usec);
}

static inline
void set_socket_timeout(int fd, long timeout)
{
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = timeout;

	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(struct timeval)) < 0)
	{
		log_error("setsockopt(SO_RCVTIMEO): %s", coda_strerror(errno));
	}
}

}

#endif /* __BLIZZARD_UTILS_HPP__ */

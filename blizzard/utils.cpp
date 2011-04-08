#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <stdexcept>
#include "config.hpp"
#include "utils.hpp"

/* #define DEFER_ACCEPT_TIME 200 */

void lz_utils::uwait(long N, long M)
{
	struct timespec ts = {N, M};

	nanosleep(&ts, 0);
}

uint64_t lz_utils::fine_clock()
{
	struct timeval tv;
	gettimeofday(&tv, 0);

	return (tv.tv_sec * 1000000LLU + tv.tv_usec);
}

void lz_utils::set_socket_timeout(int fd, long timeout)
{
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = timeout;

	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(struct timeval)) < 0)
	{
		log_error("setsockopt(SO_RCVTIMEO): %s", coda_strerror(errno));
	}
}

int lz_utils::add_listener(const char * host_desc, const char * port_desc, int listen_q_sz)
{
	struct addrinfo hints;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	struct addrinfo* addr = 0;
	int repl = getaddrinfo(host_desc, port_desc, &hints, &addr);

	if (0 != repl || 0 == addr)
	{
		throw coda_error("getaddrinfo(%s:%s) failed: %s", host_desc, port_desc, gai_strerror(repl));
	}

	int fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

	if (fd < 0)
	{
		throw coda_error("socket error: %s", coda_strerror(errno));
	}

	int is_true = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &is_true, sizeof(is_true)) < 0)
	{
		close(fd);
		
		throw coda_error("setsockopt - %s", coda_strerror(errno));
	}

	if (bind(fd, addr->ai_addr, addr->ai_addrlen) < 0)
	{
		close(fd);
		
		throw coda_error("bind(%s:%s) failed: %s", host_desc, port_desc, coda_strerror(errno));
	}

#if DEFER_ACCEPT_TIME
	int defer_accept = DEFER_ACCEPT_TIME;
	setsockopt(fd, SOL_TCP, TCP_DEFER_ACCEPT, (char *) &defer_accept, sizeof(int));
#endif

	freeaddrinfo(addr);

	if (listen(fd, listen_q_sz) < 0)
	{
		close(fd);

		throw coda_errno(errno, "listen(%s:%s) failed", host_desc, port_desc);
	}

	return fd;
}

int lz_utils::add_sender(const char * host_desc, const char * port_desc)
{
	struct addrinfo hints;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	struct addrinfo* addr = 0;
	int repl = getaddrinfo(host_desc, port_desc, &hints, &addr);

	if (0 != repl || 0 == addr)
	{
		throw coda_error("getaddrinfo(%s:%s) failed: %s", host_desc, port_desc, gai_strerror(repl));
	}

	int fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

	if (fd < 0)
	{
		throw coda_error("socket error: %s", coda_strerror(errno));
	}

	if (connect(fd, addr->ai_addr, addr->ai_addrlen) < 0)
	{
		throw coda_error("connect(%s:%s) failed: %s", host_desc, port_desc, coda_strerror(errno));
	}

	freeaddrinfo(addr);

	return fd;
}

int lz_utils::accept_new_connection(int fd, struct in_addr& ip)
{
	int connection;
	struct sockaddr_in sa;
	socklen_t lsa = sizeof(sa);

	do
	{
		connection = accept(fd, (struct sockaddr *) &sa, &lsa);
	}
	while (connection < 0 && errno == EINTR);

	if (connection < 0 && errno != EAGAIN)
	{
		log_error("accept failure: %s", coda_strerror(errno));
	}

	ip = sa.sin_addr;

	return connection;
}


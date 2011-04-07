#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <stdexcept>
#include "config.hpp"
#include "utils.hpp"

#define DEFER_ACCEPT_TIME 200

//-----------------------------------------------------------------------------------------------------------
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

//-----------------------------------------------------------------------------------------------------------

void lz_utils::close_connection(int fd)
{
	/* log_debug("close_connection %d", fd); */
	shutdown(fd, SHUT_RDWR);
	close(fd);
}

void lz_utils::set_socket_timeout(int fd, long timeout)
{
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = timeout;

	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv,sizeof(struct timeval)) < 0)
	{
		log_err(errno, "setsockopt(SO_RCVTIMEO)");
	}
}

int lz_utils::set_nonblocking(int fd)
{
	log_debug("set_nonblocking %d", fd);

	int flags = fcntl(fd, F_GETFL, 0);
	if ((flags == -1) || (flags & O_NONBLOCK))
		return flags;
	else
		return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

//-----------------------------------------------------------------------------------------------------------

int lz_utils::add_listener(const char * host_desc, const char * port_desc, int listen_q_sz)
{
	struct addrinfo hints;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	struct addrinfo* addr = 0;
	int repl = getaddrinfo(host_desc, port_desc, &hints, &addr);

	if(0 != repl || 0 == addr)
	{
		throw std::logic_error((std::string)"getaddrinfo(" +
					(std::string)host_desc + ":" +
					(std::string)port_desc +
					") failed : " + gai_strerror(repl));
	}

	int fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

	if (fd < 0)
	{
		char buff[1024];
		throw std::logic_error((std::string)"socket error: " + strerror_r(errno, buff, 1024));
	}

	int is_true = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &is_true, sizeof(is_true)) < 0)
	{
		close_connection(fd);
		
		char buff[1024];
		
		throw std::logic_error((std::string)"setsockopt - " + strerror_r(errno, buff, 1024));
	}

	if (bind(fd, addr->ai_addr, addr->ai_addrlen) < 0)
	{
		close_connection(fd);
		
		char buff[1024];

		throw std::logic_error((std::string)"bind(" +
				(std::string)host_desc + ":" +
				(std::string)port_desc +
				(std::string)") failed : " +
				(std::string)strerror_r(errno, buff, 1024));

	}

#if DEFER_ACCEPT_TIME
	int defer_accept = DEFER_ACCEPT_TIME;
	setsockopt(fd, SOL_TCP, TCP_DEFER_ACCEPT, (char *) &defer_accept, sizeof(int));
#endif
	freeaddrinfo(addr);

	if (listen(fd, listen_q_sz) < 0)
	{
		close_connection(fd);

		throw coda_errno (errno, "listen(%s:%s) failed",
			host_desc, port_desc);
#if 0
		char buff[1024];
		throw std::logic_error((std::string)"listen(" +
				(std::string)host_desc + ":" +
				(std::string)port_desc +
				(std::string)") failed : " +
				(std::string)strerror_r(errno, buff, 1024));
#endif
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

	if(0 != repl || 0 == addr)
	{
		throw std::logic_error((std::string)"getaddrinfo(" +
				(std::string)host_desc + ":" +
				(std::string)port_desc +
				") failed : " + gai_strerror(repl));
	}

	int fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

	if (fd < 0)
	{
		char buff[1024];
		throw std::logic_error((std::string)"socket error: " + strerror_r(errno, buff, 1024));
	}

	if(connect(fd, addr->ai_addr, addr->ai_addrlen) < 0)
	{
		char buff[1024];
		throw std::logic_error((std::string)"connect(" +
				(std::string)host_desc + ":" +
				(std::string)port_desc +
				(std::string)") failed : " +
				(std::string)strerror_r(errno, buff, 1024));
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
	while(connection < 0 && errno == EINTR);

	if(connection < 0 && errno != EAGAIN)
	{
		log_err(errno, "accept failure");
	}

	ip = sa.sin_addr;

	return connection;
}

#if 0
bool lz_utils::pid_file_init(const char * pid_fn)
{
	char buf[32];
	int sz = snprintf(buf, 32, "%d\n", getpid());

	int pidf = open(pid_fn, O_CREAT | O_RDWR, 0664);

	if(-1 == pidf)
	{
		return false;
	}

	ssize_t wsz = write(pidf, buf, sz);

	close(pidf);

	return wsz != -1;
}

bool lz_utils::pid_file_check(const char * pid_fn)
{
	return ::access(pid_fn, F_OK) == 0;
}

bool lz_utils::pid_file_free(const char * pid_fn)
{
	return ::unlink(pid_fn) == 0;
}
#endif

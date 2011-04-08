#ifndef __BLIZZARD_FD_MAP_HPP__
#define __BLIZZARD_FD_MAP_HPP__

#include <stdint.h>
#include <signal.h>
#include <pthread.h>
#include <Judy.h>
#include "pool.hpp"
#include "plugin.hpp"
#include "http.hpp"
#include "timeline.hpp"
#include "statistics.hpp"

namespace blizzard {

class fd_map
{
	class container : public http
	{
		uint64_t first_access;
		uint64_t last_access;

	public:
		container();
		~container();

		void init_time();
		void touch_time();

		uint64_t get_lifetime()const;
	};

	pool_ns::pool<container, 500> elements_pool;

	Pvoid_t map_handle;

	timeline timeouts;

	double min_lifetime, mid_lifetime, max_lifetime;

public:
	fd_map();
	~fd_map();

	http * create(int fd, const in_addr& ip);
	http * acquire(int fd);

	bool release(http *);

	bool del(int fd);

	void kill_oldest(int timeout);

	int min_timeout()const;

	size_t fd_count()const;
};

}

extern blizzard::statistics stats;

#endif /* __BLIZZARD_FD_MAP_HPP__ */

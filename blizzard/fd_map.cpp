#include "fd_map.hpp"
#include "utils.hpp"

enum {EPOLL_TIMEOUT = 100};

//TODO: min_timeout(): we should calculate next timeout for epoll here but I decided to set it to 10ms manually for now

blizzard::fd_map::container::container() : first_access(0), last_access(0)
{
}

blizzard::fd_map::container::~container()
{
}

void blizzard::fd_map::container::init_time()
{
	last_access = first_access = lz_utils::fine_clock();
}

void blizzard::fd_map::container::touch_time()
{
	last_access = lz_utils::fine_clock();
}

uint64_t blizzard::fd_map::container::get_lifetime()const
{
	return last_access - first_access;
}

blizzard::fd_map::fd_map() : map_handle(0), timeouts(10)
{
}

blizzard::fd_map::~fd_map()
{
	if (map_handle)
	{
		JudyLFreeArray(&map_handle, 0);
	}
}

blizzard::http * blizzard::fd_map::create(int fd, const in_addr& ip)
{
	PPvoid_t h = JudyLIns(&map_handle, (Word_t)fd, 0);

	if (h)
	{
		if (0 == *h)
		{
			container * new_el = elements_pool.allocate();

			new_el->init(fd, ip);
			new_el->init_time();

			*h = new_el;

			timeouts.reg(fd, lz_utils::fine_clock());
		}
		else
		{
			return NULL;
		}
	}

	return (blizzard::http *) *h;
}

blizzard::http* blizzard::fd_map::acquire(int fd)
{
	http* ret = 0;

	PPvoid_t h = JudyLGet(map_handle, (Word_t)fd, 0);

	if (h && *h)
	{
		container* c = (container*)(*h);
		c->touch_time();

		timeouts.reg(fd, lz_utils::fine_clock());

		ret = c;
	}

	return ret;
}

bool blizzard::fd_map::release(http * el)
{
	container * c = static_cast<container*>(el);

	if (false == c->is_locked())
	{
		c->destroy();
		elements_pool.free(c);
		return true;
	}
	else
	{
		c->destroy();
		return false;
	}
}

bool blizzard::fd_map::del(int fd)
{
	bool ret = false;

	PWord_t h = (PWord_t)JudyLGet(map_handle, fd, 0);
	if (h)
	{
		Word_t key = *h;

		if (key)
		{
			container * ob = (container *)key;
			stats.report_response_time(ob->get_lifetime());

			ret = release(ob);
		}

		ret &= JudyLDel(&map_handle, (Word_t)fd, 0);

		timeouts.del(fd);
	}

	return ret;
}

void blizzard::fd_map::kill_oldest(int timeout)
{
	timeline::iterator it;

	Word_t obj = 0;
	Word_t time = lz_utils::fine_clock();

	while (timeouts.enumerate(it, obj, time - timeout))
	{
		del(obj);
	}

	timeouts.erase_oldest(time - timeout);

	stats.objects_in_http_pool = elements_pool.allocated_objects();
	stats.pages_in_http_pool = elements_pool.allocated_pages();

}

int blizzard::fd_map::min_timeout() const
{
	return EPOLL_TIMEOUT;
}

size_t blizzard::fd_map::fd_count() const
{
	 return (size_t)JudyLCount(map_handle, 0, -1, 0);
}


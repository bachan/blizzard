#ifndef __BLIZZARD_SERVER_HPP__
#define __BLIZZARD_SERVER_HPP__

#include <ev.h>
#include <stdarg.h>
#include <stdexcept>
#include <deque>
#include "config.hpp"
#include "http.hpp"
#include "pool.hpp"
#include "plugin_factory.hpp"
#include "statistics.hpp"

namespace blizzard {

struct server
{
	enum {LISTEN_QUEUE_SZ = 1024};
	enum {HINT_EPOLL_SIZE = 10000};
	enum {EPOLL_EVENTS = 2000};

	pthread_t event_th;
	std::vector<pthread_t> easy_th;
	std::vector<pthread_t> hard_th;
	pthread_t idle_th;

	mutable pthread_mutex_t easy_proc_mutex;
	mutable pthread_cond_t  easy_proc_cond;
	mutable pthread_mutex_t hard_proc_mutex;
	mutable pthread_cond_t  hard_proc_cond;
	mutable pthread_mutex_t	done_mutex;

	std::deque<http*> easy_queue;
	std::deque<http*> hard_queue;
	std::deque<http*> done_queue;

	pool_ns::pool<http, 500> http_pool;

	plugin_factory factory;
	blz_config config;

	int incoming_sock;
	int wakeup_isock;
	int wakeup_osock;
	int threads_num;
	time_t start_time;

	bool was_daemonized;

	struct ev_loop *loop;
	ev_io incoming_watcher;
	ev_io wakeup_watcher;
	ev_timer silent_timer;

	void accept_connection();

	/* network part */

	void timeouts_kill_oldest();

	void event_processing_loop();
	void  easy_processing_loop();
	void  hard_processing_loop();
	void  idle_processing_loop();

	/* pthreads part */

	bool process(http *);

	void send_wakeup();
	void recv_wakeup();

	bool push_easy(http*);
	bool pop_easy_or_wait(http**);

	bool push_hard(http*);
	bool pop_hard_or_wait(http**);

	bool push_done(http*);
	bool pop_done(http**);

	void fire_all_threads();

	friend void* event_loop_function(void* ptr);
	friend void*  easy_loop_function(void* ptr);
	friend void*  hard_loop_function(void* ptr);
	friend void*  idle_loop_function(void* ptr);

public:
	server();
	~server();

	void load_config(const char* xml_in, const char *pid_fn, bool is_daemon);
	void prepare();
	void finalize();

	void init_threads();
	void join_threads();
};

}

extern blizzard::statistics stats;

#endif /* __BLIZZARD_SERVER_HPP__ */

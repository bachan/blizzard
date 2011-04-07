#ifndef _T_SERV_SERVER_HPP____
#define _T_SERV_SERVER_HPP____

#include <ev.h>
#include <stdarg.h>
#include <sys/epoll.h>
#include <stdexcept>
#include <deque>
#include "config.hpp"
#include "fd_map.hpp"
#include "plugin_factory.hpp"
#include "statistics.hpp"
#include "utils.hpp"

#define aux_memberof(t,n,p) (((t*)(((unsigned char *)(p))-offsetof(t,n))))

namespace blizzard
{

struct server
{
	enum {LISTEN_QUEUE_SZ = 1024};
	enum {HINT_EPOLL_SIZE = 10000};
	enum {EPOLL_EVENTS = 2000};

	struct epoll_event events[EPOLL_EVENTS];

	pthread_t epoll_th;
	std::vector<pthread_t> easy_th;
	std::vector<pthread_t> hard_th;
	pthread_t stats_th;
	pthread_t idle_th;

	mutable pthread_mutex_t  easy_proc_mutex;
	mutable pthread_cond_t   easy_proc_cond;
	mutable pthread_mutex_t  hard_proc_mutex;
	mutable pthread_cond_t   hard_proc_cond;
	mutable pthread_mutex_t stats_proc_mutex;
	mutable pthread_cond_t  stats_proc_cond;
	mutable pthread_mutex_t	   done_mutex;

	std::deque<http*> easy_queue;
	std::deque<http*> hard_queue;
	std::deque<http*> done_queue;

	fd_map fds;
	plugin_factory factory;
	blz_config config;

	int incoming_sock;
	int stats_sock;
	int epoll_sock;
	int epoll_wakeup_isock;
	int epoll_wakeup_osock;
	int threads_num;
	time_t start_time;

	struct ev_loop *loop;
	ev_io incoming_watcher;
	ev_io wakeup_watcher;
	ev_timer silent_timer;

	void accept_connection();

	/* network part */

	int  init_epoll();
	void add_epoll_action(int fd, int action, uint32_t mask);
	void timeouts_kill_oldest();

	void epoll_processing_loop();
	void  easy_processing_loop();
	void  hard_processing_loop();
	void  idle_processing_loop();

	//void stats_print();

	/* pthreads part */

	bool process_event(const epoll_event&);
	bool process(http *);

	void epoll_send_wakeup();
	void epoll_recv_wakeup();

	bool push_easy(http *);
	bool pop_easy_or_wait(http**);

	bool push_hard(http *);
	bool pop_hard_or_wait(http**);

	bool push_done(http *);
	bool pop_done(http**);

	void fire_all_threads();

	friend void* epoll_loop_function(void* ptr);
	friend void*  easy_loop_function(void* ptr);
	friend void*  hard_loop_function(void* ptr);
	friend void* stats_loop_function(void* ptr);
	friend void*  idle_loop_function(void* ptr);

public:
	server();
	~server();

	void load_config(const char* xml_in, bool is_daemon);
	void prepare();
	void finalize();

	void init_threads();
	void join_threads();
};

}

extern blizzard::statistics stats;

#endif

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdexcept>
#include <coda/daemon.h>
#include "server.hpp"

blizzard::statistics stats;

inline std::string events2string(int events)
{
	std::string rep;

	if(events & EPOLLIN)
	{
		if(!rep.empty())
			rep += " | EPOLLIN";
		else
			rep += " EPOLLIN";
	}

	if(events & EPOLLOUT)
	{
		if(!rep.empty())
			rep += " | EPOLLOUT";
		else
			rep += " EPOLLOUT";
	}

	/*if(events & EPOLLRDHUP)
	{
		if(!rep.empty())
			rep += " | EPOLLRDHUP";
		else
			rep += " EPOLLRDHUP";
	}*/

	if(events & EPOLLPRI)
	{
		if(!rep.empty())
			rep += " | EPOLLPRI";
		else
			rep += " EPOLLPRI";
	}

	if(events & EPOLLERR)
	{
		if(!rep.empty())
			rep += " | EPOLLERR";
		else
			rep += " EPOLLERR";
	}

	if(events & EPOLLHUP)
	{
		if(!rep.empty())
			rep += " | EPOLLHUP";
		else
			rep += " EPOLLHUP";
	}

	if(events & EPOLLET)
	{
		if(!rep.empty())
			rep += " | EPOLLET";
		else
			rep += " EPOLLET";
	}

	if(events & EPOLLONESHOT)
	{
		if(!rep.empty())
			rep += " | EPOLLONESHOT";
		else
			rep += " EPOLLONESHOT";
	}

	return  "[" + rep + " ]";
}

inline std::string events2string(const epoll_event& ev)
{
	std::string rep = "[";

	char buff[256];
	snprintf(buff, 256, "%d", ev.data.fd);
	rep += buff;

	rep += "]: " + events2string(ev.events);

	return rep;
}

namespace blizzard
{
	void* epoll_loop_function(void* ptr);
	void*  easy_loop_function(void* ptr);
	void*  hard_loop_function(void* ptr);
	void* stats_loop_function(void* ptr);
	void*  idle_loop_function(void* ptr);
}

blizzard::server::server()
	: incoming_sock(-1)
	, stats_sock(-1)
	, epoll_sock(-1)
	, epoll_wakeup_isock(-1)
	, epoll_wakeup_osock(-1)
	, threads_num(0)
	, start_time(0)
{
	pthread_mutex_init(&done_mutex, 0);

	pthread_mutex_init(&easy_proc_mutex, 0);
	pthread_mutex_init(&hard_proc_mutex, 0);
	pthread_mutex_init(&stats_proc_mutex, 0);

	pthread_cond_init(&easy_proc_cond, 0);
	pthread_cond_init(&hard_proc_cond, 0);
	pthread_cond_init(&stats_proc_cond, 0);

	start_time = time(NULL);
}

blizzard::server::~server()
{
	log_debug("~server()");

	coda_terminate = 1;
	join_threads();

	finalize();

	fire_all_threads();

	pthread_cond_destroy(&stats_proc_cond);
	pthread_cond_destroy(&hard_proc_cond);
	pthread_cond_destroy(&easy_proc_cond);

	pthread_mutex_destroy(&stats_proc_mutex);
	pthread_mutex_destroy(&hard_proc_mutex);
	pthread_mutex_destroy(&easy_proc_mutex);

	pthread_mutex_destroy(&done_mutex);

	log_debug("/~server()");
}

void blizzard::server::init_threads()
{
	if(0 == pthread_create(&epoll_th, NULL, &epoll_loop_function, this))
	{
		log_debug("epoll thread created");
		threads_num++;
	}
	else
	{
		throw std::logic_error("error creating epoll thread");
	}

	if(0 == pthread_create(&idle_th, NULL, &idle_loop_function, this))
	{
		threads_num++;
		log_debug("idle thread created");
	}
	else
	{
		throw std::logic_error("error creating idle thread");
	}

	if(0 == pthread_create(&stats_th, NULL, &stats_loop_function, this))
	{
		threads_num++;
		log_debug("stats thread created");
	}
	else
	{
		throw std::logic_error("error creating stats thread");
	}
	log_info("%d internal threads created", threads_num);

	log_info("requested worker threads {easy: %d, hard: %d}", config.blz.plugin.easy_threads, config.blz.plugin.hard_threads);

	for(int i = 0; i < config.blz.plugin.easy_threads; i++)
	{
		pthread_t th;
		int r = pthread_create(&th, NULL, &easy_loop_function, this);
		if(0 == r)
		{
			log_debug("easy thread created");
			easy_th.push_back(th);

			threads_num++;
		}
		else
		{
			char s[256];
			char buff[100];
			snprintf(s, 256, "error creating easy thread #%d : %s", i, strerror_r(r, buff, 100));
			throw std::logic_error(s);
		}
	}

	for(int i = 0; i < config.blz.plugin.hard_threads; i++)
	{
		pthread_t th;
		int r = pthread_create(&th, NULL, &hard_loop_function, this);
		if(0 == r)
		{
			log_debug("hard thread created");
			hard_th.push_back(th);

			threads_num++;
		}
		else
		{
			char s[256];
			char buff[100];
			snprintf(s, 256, "error creating hard thread #%d : %s", i, strerror_r(r, buff, 100));
			throw std::logic_error(s);
		}
	}

	log_info("all worker threads created");
}

void blizzard::server::join_threads()
{
	if(0 == threads_num)
	{
		return;
	}

	pthread_join(epoll_th,  0);
	log_info("epoll_th joined");
	threads_num--;

	pthread_join(idle_th,  0);
	log_info("idle_th joined");
	threads_num--;

	pthread_join(stats_th,  0);
	log_info("stats_th joined");
	threads_num--;

	for(size_t i = 0; i < easy_th.size(); i++)
	{
		log_info("pthread_join(easy_th[%d], 0)", (int)i);
		pthread_join(easy_th[i], 0);
		threads_num--;
	}

	easy_th.clear();

	for(size_t i = 0; i < hard_th.size(); i++)
	{
		log_info("pthread_join(hard_th[%d], 0)", (int)i);
		pthread_join(hard_th[i], 0);
		threads_num--;
	}

	hard_th.clear();

	log_notice("%d threads left", (int)threads_num);
}

void blizzard::server::fire_all_threads()
{
	pthread_mutex_lock(&easy_proc_mutex);
	pthread_cond_broadcast(&easy_proc_cond);
	pthread_mutex_unlock(&easy_proc_mutex);

	pthread_mutex_lock(&hard_proc_mutex);
	pthread_cond_broadcast(&hard_proc_cond);
	pthread_mutex_unlock(&hard_proc_mutex);

	log_debug("fire_all_threads");
}

void blizzard::server::epoll_send_wakeup()
{
	log_debug("epoll_send_wakeup()");
	char b[1] = {'w'};

	int ret;
	do
	{
		ret = write(epoll_wakeup_osock, b, 1);
		if(ret < 0 && errno != EINTR)
		{
			char buff[1024];
			log_error("epoll_send_wakeup():write failure: '%s'", strerror_r(errno, buff, 1024));
		}
	}
	while(ret < 0);
}

void blizzard::server::epoll_recv_wakeup()
{
	log_debug("epoll_recv_wakeup()");
	char b[1024];

	int ret;
	do
	{
		ret = read(epoll_wakeup_isock, b, 1024);
		if(ret < 0 && errno != EAGAIN && errno != EINTR)
		{
			char buff[1024];
			log_error("epoll_recv_wakeup()::read failure: '%s'", strerror_r(errno, buff, 1024));
		}
	}
	while(ret == 1024 || errno == EINTR);
}

bool blizzard::server::push_easy(http * el)
{
	bool res = false;

	pthread_mutex_lock(&easy_proc_mutex);

	size_t eq_sz = easy_queue.size(); 
	stats.report_easy_queue_len(eq_sz);

	if(config.blz.plugin.easy_queue_limit == 0 || (eq_sz < (size_t)config.blz.plugin.easy_queue_limit))
	{
		easy_queue.push_back(el);
		res = true;

		log_debug("push_easy %d", el->get_fd());

		pthread_cond_signal(&easy_proc_cond);
	}

	pthread_mutex_unlock(&easy_proc_mutex);

	return res;
}

bool blizzard::server::pop_easy_or_wait(http** el)
{
	bool ret = false;

	pthread_mutex_lock(&easy_proc_mutex);

	size_t eq_sz = easy_queue.size();
	
	stats.report_easy_queue_len(eq_sz);

	if(eq_sz)
	{
		*el = easy_queue.front();

		log_debug("pop_easy %d", (*el)->get_fd());

		easy_queue.pop_front();

		ret = true;
	}
	else
	{
		log_debug("pop_easy : events empty");

		pthread_cond_wait(&easy_proc_cond, &easy_proc_mutex);
	}

	pthread_mutex_unlock(&easy_proc_mutex);

	return ret;
}

bool blizzard::server::push_hard(http * el)
{
	bool res = false;

	pthread_mutex_lock(&hard_proc_mutex);

	size_t hq_sz = hard_queue.size();

	stats.report_hard_queue_len(hq_sz);

	if(config.blz.plugin.hard_queue_limit == 0 || (hq_sz < (size_t)config.blz.plugin.hard_queue_limit))
	{
		hard_queue.push_back(el);

		res = true;

		log_debug("push_hard %d", el->get_fd());

		pthread_cond_signal(&hard_proc_cond);
	}

	pthread_mutex_unlock(&hard_proc_mutex);

	return res;
}

bool blizzard::server::pop_hard_or_wait(http** el)
{
	bool ret = false;

	pthread_mutex_lock(&hard_proc_mutex);
		
	size_t hq_sz = hard_queue.size(); 
	
	stats.report_hard_queue_len(hq_sz);

	if(hq_sz)
	{
		*el = hard_queue.front();

		log_debug("pop_hard %d", (*el)->get_fd());

		hard_queue.pop_front();

		ret = true;
	}
	else
	{
		log_debug("pop_hard : events empty");

		pthread_cond_wait(&hard_proc_cond, &hard_proc_mutex);
	}

	pthread_mutex_unlock(&hard_proc_mutex);

	return ret;
}
bool blizzard::server::push_done(http * el)
{
	pthread_mutex_lock(&done_mutex);

	done_queue.push_back(el);

	stats.report_done_queue_len(done_queue.size());

	log_debug("push_done %d", el->get_fd());

	pthread_mutex_unlock(&done_mutex);

	epoll_send_wakeup();

	return true;
}

bool blizzard::server::pop_done(http** el)
{
	bool ret = false;

	pthread_mutex_lock(&done_mutex);

	size_t dq_sz = done_queue.size(); 

	stats.report_done_queue_len(dq_sz);

	if(dq_sz)
	{
		*el = done_queue.front();

		log_debug("pop_done %d", (*el)->get_fd());

		done_queue.pop_front();

		ret = true;
	}

	pthread_mutex_unlock(&done_mutex);

	return ret;
}

void blizzard::server::load_config(const char* xml_in, bool is_daemon)
{
	if(::access(xml_in, F_OK) < 0)
	{
		std::string s = "access('" + config.blz.log_file_name + "') failed : ";
		char buff[1024];

		s += strerror_r(errno, buff, 1024); 

		throw std::logic_error(s);
	}

	config.clear();
	config.load_from_file(xml_in);
	config.check();

	if (!is_daemon) return;

	int res;
	
	if (0 > (res = log_create_from_str(config.blz.log_file_name.c_str(), config.blz.log_level.c_str())))
	{
		throw coda_errno (errno, "logger init from (%s, %s) failed"
			, config.blz.log_file_name.c_str()
			, config.blz.log_level.c_str()
		);
	}
}

static void incoming_callback(EV_P_ ev_io *w, int tev)
{
	blizzard::server *s = (blizzard::server *) ev_userdata(loop);
	s->accept_connection();
}

static void wakeup_callback(EV_P_ ev_io *w, int tev)
{
	blizzard::server *s = (blizzard::server *) ev_userdata(loop);
	s->epoll_recv_wakeup();
}

static void silent_callback(EV_P_ ev_timer *w, int tev)
{
	if (0 != coda_terminate)
	{
		ev_timer_stop(EV_A_ w);
		ev_unloop(EV_A_ EVUNLOOP_ALL);
		return;
	}

	if (0 != coda_rotatelog)
	{
		blizzard::server *s = (blizzard::server *) ev_userdata(loop);
		log_rotate(s->config.blz.log_file_name.c_str());
		coda_rotatelog = 0;
	}
}

void blizzard::server::accept_connection()
{
	struct in_addr ip;

	int client = lz_utils::accept_new_connection(incoming_sock, ip);

	if(client >= 0)
	{
		log_debug("accept_new_connection: %d from %s", client, inet_ntoa(ip));

		lz_utils::set_nonblocking(client);

		http *con = fds.create(client, ip);

		if (NULL != con)
		{
			// add_epoll_action(client, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT[> | EPOLLRDHUP<] | EPOLLET);
			con->add_watcher(loop);
		}
		else
		{
			log_error("ERROR: fd[%d] is still in list of fds", client);
		}
	}
}

void blizzard::server::prepare()
{
	factory.load_module(config.blz.plugin);
#if 0
	epoll_sock = init_epoll();
#endif
	loop = ev_default_loop(0);
	ev_set_userdata(loop, this); /* hack to simplify things in http.cpp, couldn't be REALLY needed, if blizzard were written more libev friendly */
	ev_set_io_collect_interval(loop, 0.01); /* hack to emulate old blizzard behaviour (epolling with timeout 100ms (we set it to 50ms here)) */
	// ev_set_timeout_collect_interval(loop, 0.01);

	//----------------------------
	//add incoming sock

	incoming_sock = lz_utils::add_listener(config.blz.plugin.ip.c_str(), config.blz.plugin.port.c_str(), LISTEN_QUEUE_SZ);
	if(-1 != incoming_sock)
	{
		log_info("blizzard is bound to %s:%s", config.blz.plugin.ip.c_str(), config.blz.plugin.port.c_str());
	}

	lz_utils::set_nonblocking(incoming_sock);
#if 0
	add_epoll_action(incoming_sock, EPOLL_CTL_ADD, EPOLLIN);
#endif
	ev_io_init(&incoming_watcher, incoming_callback, incoming_sock, EV_READ);
	ev_io_start(loop, &incoming_watcher);

	//----------------------------
	//add stats sock

	stats_sock = lz_utils::add_listener(config.blz.stats.ip.c_str(), config.blz.stats.port.c_str());
	if(-1 != stats_sock)
	{
		log_info("blizzard statistics is bound to %s:%s", config.blz.stats.ip.c_str(), config.blz.stats.port.c_str());
	}

	lz_utils::set_socket_timeout(stats_sock, 50000);

	//----------------------------
	//add epoll wakeup fd

	int pipefd[2];
	if(::pipe(pipefd) == -1)
	{
		char buff[1024];
		throw std::logic_error((std::string)"server::prepare():pipe() failed : " + strerror_r(errno, buff, 1024));
	}

	epoll_wakeup_osock = pipefd[1];
	epoll_wakeup_isock = pipefd[0];

	lz_utils::set_nonblocking(epoll_wakeup_isock);
#if 0
	add_epoll_action(epoll_wakeup_isock, EPOLL_CTL_ADD, EPOLLIN | EPOLLET);
#endif
	ev_io_init(&wakeup_watcher, wakeup_callback, epoll_wakeup_isock, EV_READ);
	ev_io_start(loop, &wakeup_watcher);

	ev_timer_init(&silent_timer, silent_callback, 0, 1);
	ev_timer_again(loop, &silent_timer);
}

void blizzard::server::finalize()
{
	if (-1 != incoming_sock)
	{
		lz_utils::close_connection(incoming_sock);
		incoming_sock = -1;
	}

	if (-1 != stats_sock)
	{
		lz_utils::close_connection(stats_sock);
		stats_sock = -1;
	}

	if (-1 != epoll_wakeup_isock)
	{
		close(epoll_wakeup_isock);
		epoll_wakeup_isock = -1;
	}

	if (-1 != epoll_wakeup_osock)
	{
		close(epoll_wakeup_osock);
		epoll_wakeup_osock = -1;
	}

#if 0
	if (-1 != epoll_sock)
	{
		close(epoll_sock);
		epoll_sock = -1;
	}
#endif

	factory.stop_module();
}

#if 0
int blizzard::server::init_epoll()
{
	log_debug("init_epoll");

	int res = epoll_create(HINT_EPOLL_SIZE);
	if(-1 == res)
	{
		char buff[1024];
		throw std::logic_error((std::string)"epoll_create : " + strerror_r(errno, buff, 1024));
	}

	log_debug("epoll inited: %d", res);

	return res;
}
#endif

#if 0
void blizzard::server::add_epoll_action(int fd, int action, uint32_t mask)
{
	log_debug("add_epoll_action %d", fd);

	struct epoll_event evt;
	memset(&evt, 0, sizeof(evt));

	evt.events = mask;
	evt.data.fd = fd;

	int r;
	do
	{
		r = epoll_ctl(epoll_sock, action, fd, &evt);
	}
	while(r < 0 && errno == EINTR);

	if (-1 == r)
	{
		char buff[1024];
		std::string err_str = "add_epoll_action:epoll_ctl(";

		switch(action)
		{
			case EPOLL_CTL_ADD:
				err_str += "EPOLL_CTL_ADD";
				break;
			case EPOLL_CTL_MOD:
				err_str += "EPOLL_CTL_MOD";
				break;
			case EPOLL_CTL_DEL:
				err_str += "EPOLL_CTL_DEL";
				break;
		}
		err_str += ", " + events2string(evt.events) + ") : ";
		err_str += strerror_r(errno, buff, 1024);

		throw std::logic_error(err_str);
	}
}
#endif

void blizzard::server::epoll_processing_loop()
{
	http * done_task = 0;
	while(pop_done(&done_task))
	{
		done_task->unlock();

		if(-1 != done_task->get_fd())
		{
			log_debug("%d is still alive", done_task->get_fd());
			process(done_task);
		}
		else
		{
			log_debug("%d is already dead while travelling through queues", done_task->get_fd());
			fds.release(done_task);
		}
	}

	ev_run(loop, EVRUN_ONCE);

#if 0
	int nfds = 0;
	do
	{
		nfds = epoll_wait(epoll_sock, events, EPOLL_EVENTS, fds.min_timeout()/*EPOLL_TIMEOUT*/);
	}
	while(nfds == -1 && (errno == EINTR || errno == EAGAIN));

	if(-1 == nfds)
	{
		char buff[1024];
		throw std::logic_error((std::string)"epoll_wait : " + strerror_r(errno, buff, 1024));
	}

	if(nfds)
	{
		if(nfds > 1)
		{
			log_debug("==  epoll_processing %d messages ==", nfds);
		}
		else
		{
			log_debug("==  epoll_processing 1 message ==");
		}

		//log_info("==  %d messages ==", nfds);
	}

	for(int i = 0; i < nfds; i++)
	{
		if(events[i].data.fd == epoll_wakeup_isock)
		{
			epoll_recv_wakeup();
		}
		else if(events[i].data.fd == incoming_sock)
		{
			struct in_addr ip;

			int client = lz_utils::accept_new_connection(incoming_sock, ip);

			if(client >= 0)
			{
				log_debug("accept_new_connection: %d from %s", client, inet_ntoa(ip));

				lz_utils::set_nonblocking(client);

				if(true == fds.create(client, ip))
				{
					add_epoll_action(client, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT/* | EPOLLRDHUP*/ | EPOLLET);
				}
				else
				{
					log_error("ERROR: fd[%d] is still in list of fds", client);
				}
			}
		}
		else
		{
			process_event(events[i]);
		}
	}
#endif

	fds.kill_oldest(1000 * config.blz.plugin.connection_timeout);

	stats.process();

#if 0
	if (0 != coda_rotatelog)
	{
		log_rotate(config.blz.log_file_name.c_str());
		coda_rotatelog = 0;
	}
#endif
}

#if 0
bool blizzard::server::process_event(const epoll_event& ev)
{
	log_debug("query event: %s", events2string(ev).c_str());

	http * con = fds.acquire(ev.data.fd);

	if(con)
	{
		if(ev.events & (EPOLLHUP | EPOLLERR))
		{
			log_debug("closing connection: got HUP/ERR!");
			con->set_rdeof();
			con->set_wreof();

			fds.del(ev.data.fd);
		}
		else
		{
			if(ev.events & EPOLLIN)
			{
				con->allow_read();
			}

			if(ev.events & EPOLLOUT)
			{
				con->allow_write();
			}

			/*if(ev.events & EPOLLRDHUP)
			{
				log_debug("push_event: RDHUP!");

				con->set_rdeof();
			}*/

			process(con);
		}
	}
	else
	{
		log_error("ERROR: process_event: unregistered fd in epoll set: %d", ev.data.fd);
	}

	return true;
}
#endif

bool blizzard::server::process(http * con)
{
	if(!con->is_locked())
	{
		con->process();

		if(con->state() == http::sReadyToHandle)
		{
			log_debug("push_easy(%d)", con->get_fd());

			con->lock();

			if(false == push_easy(con))
			{
				log_debug("easy queue full: easy_queue_size == %d", config.blz.plugin.easy_queue_limit);

				con->set_response_status(503);
				con->add_response_header("Content-type", "text/plain");
				con->add_response_buffer("easy queue filled!", strlen("easy queue filled!"));

				push_done(con);
			}
		}
		else if(con->state() == http::sDone || con->state() == http::sUndefined)
		{
			log_debug("%d is done, closing write side of connection", con->get_fd());

			ev_io_stop(loop, &con->watcher);

			fds.del(con->get_fd());
		}
	}

	return true;
}

void blizzard::server::easy_processing_loop()
{
	blz_plugin* plugin = factory.open_plugin();

	http* task = 0;

	if(pop_easy_or_wait(&task))
	{
		log_debug("blizzard::easy_loop_function.fd = %d", task->get_fd());

		switch(plugin->easy(task))
		{
		case BLZ_OK:
			log_debug("easy_loop: processed %d", task->get_fd());
			push_done(task);
			break;

		case BLZ_ERROR:
			log_error("easy thread reports error");
			task->set_response_status(503);
			task->add_response_header("Content-type", "text/plain");
			task->add_response_buffer("easy loop error", strlen("easy loop error"));
			push_done(task);
			break;

		case BLZ_AGAIN:
			log_debug("easy thread -> hard thread");
			if(config.blz.plugin.hard_threads)
			{
				bool ret = push_hard(task);
				if(false == ret)
				{
					log_debug("hard queue full: hard_queue_size == %d", config.blz.plugin.hard_queue_limit);
					task->set_response_status(503);
					task->add_response_header("Content-type", "text/plain");
					task->add_response_buffer("hard queue filled!", strlen("hard queue filled!"));
					push_done(task);
				}
			}
			else
			{
				log_error("easy-thread tried to enqueue hard-thread, but config::plugin::hard_threads = 0");
				task->set_response_status(503);
				task->add_response_header("Content-type", "text/plain");
				task->add_response_buffer("easy loop error", strlen("easy loop error"));
				push_done(task);
			}
			break;
		}
	}
}

void blizzard::server::hard_processing_loop()
{
	blz_plugin* plugin = factory.open_plugin();

	http* task = 0;

	if (pop_hard_or_wait(&task))
	{
		log_debug("blizzard::hard_loop_function.fd = %d", task->get_fd());

		switch (plugin->hard(task))
		{
		case BLZ_OK:
			log_debug("hard_loop: processed %d", task->get_fd());
			push_done(task);
			break;

		case BLZ_ERROR:
		case BLZ_AGAIN:
			log_error("hard_loop reports error");
			task->set_response_status(503);
			task->add_response_header("Content-type", "text/plain");
			task->add_response_buffer("hard loop error", strlen("hard loop error"));
			push_done(task);
			break;
		}
	}
}

void blizzard::server::idle_processing_loop()
{
	if (0 == config.blz.plugin.idle_timeout)
	{
		factory.idle();

		while (0 == coda_terminate && 0 == coda_changecfg)
		{
			sleep(1);
		}
	}
	else
	{
		while (0 == coda_terminate && 0 == coda_changecfg)
		{
			factory.idle();
			coda_msleep(config.blz.plugin.idle_timeout);
		}
	}
}

void *blizzard::epoll_loop_function(void *ptr)
{
	blizzard::server *srv = (blizzard::server *) ptr;

	try
	{
		while (0 == coda_terminate && 0 == coda_changecfg)
		{
			srv->epoll_processing_loop();
		}
	}
	catch (const std::exception &e)
	{
		coda_terminate = 1;
		log_crit("epoll_loop: exception: %s", e.what());
	}

	srv->fire_all_threads();
	pthread_exit(NULL);
}

void *blizzard::easy_loop_function(void *ptr)
{
	blizzard::server *srv = (blizzard::server *) ptr;

	try
	{
		while (0 == coda_terminate && 0 == coda_changecfg)
		{
			srv->easy_processing_loop();
		}
	}
	catch (const std::exception &e)
	{
		coda_terminate = 1;
		log_crit("easy_loop: exception: %s", e.what());
	}

	srv->fire_all_threads();
	pthread_exit(NULL);
}

void *blizzard::hard_loop_function(void *ptr)
{
	 blizzard::server *srv = (blizzard::server *) ptr;

	try
	{
		while (0 == coda_terminate && 0 == coda_changecfg)
		{
			 srv->hard_processing_loop();
		}
	}
	catch (const std::exception &e)
	{
		coda_terminate = 1;
		log_crit("hard_loop: exception: %s", e.what());
	}

	srv->fire_all_threads();
	pthread_exit(NULL);
}

void *blizzard::idle_loop_function(void *ptr)
{
	blizzard::server *srv = (blizzard::server *) ptr;

	try
	{
		while (0 == coda_terminate && 0 == coda_changecfg)
		{
			srv->idle_processing_loop();
		}
	}
	catch (const std::exception &e)
	{
		coda_terminate = 1;
		log_crit("idle_loop: exception: %s", e.what());
	}

	srv->fire_all_threads();
	pthread_exit(NULL);
}

void *blizzard::stats_loop_function(void *ptr)
{
	blizzard::server *srv = (blizzard::server *) ptr;
	blizzard::http stats_parser;

	try
	{
		while (0 == coda_terminate && 0 == coda_changecfg)
		{
			if(srv->stats_sock != -1)
			{
				struct in_addr ip;
				int stats_client = lz_utils::accept_new_connection(srv->stats_sock, ip);

				if(stats_client >= 0)
				{

					lz_utils::set_socket_timeout(stats_client, 50000);

					log_debug("stats: accept_new_connection: %d from %s", stats_client, inet_ntoa(ip));

					stats_parser.init(stats_client, ip);

					while(true)
					{
						stats_parser.allow_read();
						stats_parser.allow_write();

						stats_parser.process();

						if(stats_parser.state() == http::sReadyToHandle)
						{
							stats_parser.set_response_status(200);
							stats_parser.add_response_header("Content-type", "text/plain");

							std::string resp = "<blizzard_stats>\n";

							char buff[1024];

							time_t up_time = time(0) - srv->start_time;

							snprintf(buff, 1024, "\t<blizzard_version>"BLZ_VERSION"</blizzard_version>\n\t<uptime>%d</uptime>\n", (int)up_time);
							resp += buff;

							snprintf(buff, 1024, "\t<rps>%.4f</rps>\n", stats.get_rps());
							resp += buff;

							snprintf(buff, 1024, "\t<fd_count>%d</fd_count>\n", (int)srv->fds.fd_count());
							resp += buff; 

							snprintf(buff, 1024, "\t<queues>\n\t\t<easy>%d</easy>\n\t\t<max_easy>%d</max_easy>\n"
								"\t\t<hard>%d</hard>\n\t\t<max_hard>%d</max_hard>\n\t\t<done>%d</done>\n"
								"\t\t<max_done>%d</max_done>\n\t</queues>\n",
									(int)stats.easy_queue_len,
																		(int)stats.easy_queue_max_len,
									(int)stats.hard_queue_len,
									(int)stats.hard_queue_max_len,
									(int)stats.done_queue_len,
									(int)stats.done_queue_max_len);
							resp += buff;

							snprintf(buff, 1024, "\t<conn_time>\n\t\t<min>%.4f</min>\n\t\t<avg>%.4f</avg>\n\t\t<max>%.4f</max>\n\t</conn_time>\n",
									stats.get_min_lifetime(), stats.get_mid_lifetime(), stats.get_max_lifetime());
							resp += buff;

							snprintf(buff, 1024, "\t<mem_allocator>\n\t\t<pages>%d</pages>\n\t\t<objects>%d</objects>\n\t</mem_allocator>\n",
									(int)stats.pages_in_http_pool, (int)stats.objects_in_http_pool);
							resp += buff;

							struct rusage usage;
							::getrusage(RUSAGE_SELF, &usage);

							snprintf(buff, 1024, "\t<rusage>\n\t\t<utime>%d</utime>\n\t\t<stime>%d</stime>\n\t</rusage>\n",
									(int)usage.ru_utime.tv_sec, (int)usage.ru_stime.tv_sec);
							resp += buff;

							resp += "</blizzard_stats>\n";

							stats_parser.add_response_buffer(resp.data(), resp.size());
						}
						else if (stats_parser.state() == http::sDone)
						{
							log_debug("%d is done, closing write side of connection", stats_parser.get_fd());
							break;
						}
					}

					stats_parser.destroy();

					stats.process();
				}
			}
			else
			{
				sleep(1);
			}
		}
	}
	catch (const std::exception &e)
	{
		coda_terminate = 1;
		log_crit("stats_loop: exception: %s", e.what());
	}

	srv->fire_all_threads();
	pthread_exit(NULL);
}


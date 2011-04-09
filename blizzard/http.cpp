#include <errno.h>
#include <stdlib.h>
#include <netdb.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "http.hpp"
#include "server.hpp"

//TODO: Error messages support etc
//TODO: Expect: 100-continue header support for POST requests

struct HTTP_CODES_DESC
{
	int code;
	const char * desc;
}
codes_table[] = {
	{100, "Continue"},
	{101, "Switching Protocols"},
	{200, "OK"},
	{201, "Created"},
	{202, "Accepted"},
	{203, "Non-Authoritative Information"},
	{204, "No Content"},
	{205, "Reset Content"},
	{206, "Partial Content"},
	{300, "Multiple Choices"},
	{301, "Moved Permanently"},
	{302, "Moved Temporarily"},
	{303, "See Other"},
	{304, "Not Modified"},
	{305, "Use Proxy"},
	{400, "Bad Request"},
	{401, "Unauthorized"},
	{402, "Payment Required"},
	{403, "Forbidden"},
	{404, "Not Found"},
	{405, "Method Not Allowed"},
	{406, "Not Acceptable"},
	{407, "Proxy Authentication Required"},
	{408, "Request Timeout"},
	{409, "Conflict"},
	{410, "Gone"},
	{411, "Length Required"},
	{412, "Precondition Failed"},
	{413, "Request Entity Too Large"},
	{414, "Request-URI Too Long"},
	{415, "Unsupported Media Type"},
	{500, "Internal Server Error"},
	{501, "Not Implemented"},
	{502, "Bad Gateway"},
	{503, "Service Unavailable"},
	{504, "Gateway Timeout"},
	{505, "HTTP Version Not Supported"}
};

#define codes_table_sz (sizeof(codes_table) / sizeof(codes_table[0]))


enum {HTTP_CODES_MAX_SIZE = 1024};
const char * http_codes_array[HTTP_CODES_MAX_SIZE];

const char ** blizzard::http::http_codes = 0;
int blizzard::http::http_codes_num = 0;

blizzard::http::http() :
	fd(-1),
	want_read(false),
	want_write(false),
	can_read(false),
	can_write(false),
	stop_reading(false),
	stop_writing(false),
	locked(false),
	state_(sUndefined),
	header_items_num(0),
	protocol_major(0),
	protocol_minor(0),
	keep_alive(false),
	cache(false),
	uri_path(0),
	uri_params(0),
	response_status(0)
{
	memset(&in_ip, 0, sizeof(in_ip));

	out_post.set_expand(true);

	if(0 == http_codes)
	{
		for(uint32_t i = 0; i < HTTP_CODES_MAX_SIZE; i++)
		{
			 http_codes_array[i] = "Unknown";
		}

		for(uint32_t j = 0; j < codes_table_sz; j++)
		{
			int key = codes_table[j].code;

			if(key < HTTP_CODES_MAX_SIZE)
			{
				http_codes_array[key] = codes_table[j].desc;

				if(http_codes_num < key)
				{
					 http_codes_num = key;
				}
			}
		}

		http_codes = http_codes_array;
	}
}

blizzard::http::~http()
{
	if (-1 != fd)
	{
		close(fd);
		fd = -1;
	}

	locked = false;

	state_ = sUndefined;
}

#define memberof(t,n,p) (((t*)(((unsigned char *)(p))-offsetof(t,n))))

static void recv_callback(EV_P_ ev_io *w, int tev)
{
	blizzard::server *s = (blizzard::server *) ev_userdata(loop);
	blizzard::events *e = memberof(blizzard::events, watcher_recv, w);
	blizzard::http *con = e->con;

	con->allow_read();
	s->process(con);
}

static void send_callback(EV_P_ ev_io *w, int tev)
{
	blizzard::server *s = (blizzard::server *) ev_userdata(loop);
	blizzard::events *e = memberof(blizzard::events, watcher_send, w);
	blizzard::http *con = e->con;

	con->allow_write();
	s->process(con);
}

static void timeout_callback(EV_P_ ev_timer *w, int tev)
{
	blizzard::server *s = (blizzard::server *) ev_userdata(loop);
	blizzard::events *e = memberof(blizzard::events, watcher_timeout, w);
	blizzard::http *con = e->con;

	if (!con->is_locked() && (con->state() == blizzard::http::sUndefined || con->state() == blizzard::http::sDone))
	{
		ev_io_stop(loop, &e->watcher_recv);
		ev_io_stop(loop, &e->watcher_send);
		ev_timer_stop(loop, &e->watcher_timeout);

		con->destroy();
		s->http_pool.free(con);
	}
}

void blizzard::http::add_watcher(struct ev_loop *loop)
{
	e.con = this;

	ev_io_init(&e.watcher_recv, recv_callback, fd, EV_READ);
	ev_io_start(loop, &e.watcher_recv);

	ev_io_init(&e.watcher_send, send_callback, fd, EV_WRITE);

	ev_timer_init(&e.watcher_timeout, timeout_callback, 0, 1);
	ev_timer_again(loop, &e.watcher_timeout);
}

void blizzard::http::init(int new_fd, const struct in_addr& ip)
{
	if (-1 == fd)
	{
		fd = new_fd;
	}
	else
	{
		log_warn("blizzard::http::[%d] tried to double-init on %d", fd, new_fd);
	}

	in_ip = ip;

	want_read = false;
	want_write = false;
	can_read = false;
	can_write = false;
	stop_reading = false;
	stop_writing = false;

	locked = false;

	state_ = sUndefined;
	header_items_num = 0;
	protocol_major = 0;
	protocol_minor = 0;
	keep_alive = false;
	cache = false;

	uri_path = 0;
	uri_params = 0;
	response_status = 0;

	in_headers.reset();
	in_post.reset();
	out_title.reset();
	out_headers.reset();
	out_post.reset();

	out_post.set_expand(true);

	state_ = sUndefined;

	/* log_debug("blizzard::http::init(%d, %s)", new_fd, inet_ntoa(in_ip)); */
}

void blizzard::http::destroy()
{
	/* log_debug("blizzard::http::destroy(%d)", fd); */

	if (-1 != fd)
	{
		close(fd);
		fd = -1;
	}

	in_headers.reset();
	in_post.reset();
	out_title.reset();
	out_headers.reset();
	out_post.reset();

	state_ = sUndefined;
}


bool blizzard::http::ready_read()const
{
	return can_read/* && want_read*/;// && !stop_reading;
}

bool blizzard::http::ready_write()const
{
	return can_write && want_write;// && !stop_writing;
}

//-------------------------------------------------------------------------------------------------------------------

void blizzard::http::allow_read()
{
	can_read = true;
	/* log_debug("blizzard::http::process::allow_read(%d)", fd); */
}

void blizzard::http::allow_write()
{
	can_write = true;
	/* log_debug("blizzard::http::process::allow_write(%d)", fd); */
}

bool blizzard::http::ready()const
{
	/* log_debug("blizzard::http::ready() : read{c:%d w:%d st:%d}, write{c:%d w:%d st:%d}", */
		/* can_read, want_read, stop_reading, can_write, want_write, stop_writing); */

	return ready_read() || ready_write();
}

bool blizzard::http::get_rdeof()
{
	return stop_reading;
}

void blizzard::http::set_rdeof()
{
	stop_reading = true;
	//can_read = false;
	/* log_debug("set_rdeof()"); */
}

bool blizzard::http::get_wreof()
{
	return stop_writing;
}

void blizzard::http::set_wreof()
{
	stop_writing = true;
	can_write = false;
	/* log_debug("set_wreof()"); */
}

int blizzard::http::get_fd()const
{
	return fd;
}

void blizzard::http::lock()
{
// log_debug("http(%d)::lock()", fd); 
	locked = true;
}

void blizzard::http::unlock()
{
// log_debug("http(%d)::unlock()", fd); 
	locked = false;
}

bool blizzard::http::is_locked()const
{
	return locked;
}

blizzard::http::http_state blizzard::http::state()const
{
	return state_;
}

int blizzard::http::get_request_method()const
{
	return method;
}

int blizzard::http::get_version_major()const
{
	return protocol_major;
}
int blizzard::http::get_version_minor()const
{
	return protocol_minor;
}

bool blizzard::http::get_keepalive()const
{
	return keep_alive;
}

bool blizzard::http::get_cache()const
{
	return cache;
}

struct in_addr blizzard::http::get_request_ip()const
{
	return in_ip;
}

const char * blizzard::http::get_request_uri_path()const
{
	return uri_path;
}

const char * blizzard::http::get_request_uri_params()const
{
	return uri_params;
}

size_t blizzard::http::get_request_body_len()const
{
	return in_post.size();
}

const uint8_t * blizzard::http::get_request_body()const
{
	return (const uint8_t *)in_post.get_data();
}

const char * blizzard::http::get_request_header(const char * hk)const
{
	for(size_t i = 0; i < get_request_headers_num(); i++)
	{
		if(!strcasecmp(header_items[i].key, hk))
			return header_items[i].value;
	}

	return 0;
}

size_t blizzard::http::get_request_headers_num()const
{
	return header_items_num;
}

const char* blizzard::http::get_request_header_key(int sz)const
{
	return header_items[sz].key;
}

const char* blizzard::http::get_request_header_value(int sz)const
{
	return header_items[sz].value;
}

void blizzard::http::set_keepalive(bool st)
{
	keep_alive = st;
}

void blizzard::http::set_cache(bool ch)
{
	cache = ch;
}

void blizzard::http::set_response_status(int st)
{
	response_status = st;
}

void blizzard::http::add_response_header(const char* name, const char* data)
{
	out_headers.append_data(name, strlen(name));
	out_headers.append_data(":", 1);
	out_headers.append_data(data, strlen(data));
	out_headers.append_data("\r\n", 2);
}

void blizzard::http::add_response_buffer(const char* data, size_t size)
{
	out_post.append_data(data, size);
}

void blizzard::http::process()
{
	/* log_debug("blizzard::http::process()"); */

	bool quit = false;
	int res = 0;

	while(!quit)
	{
		switch(state_)
		{
		case sUndefined:

			want_read = true;
			want_write = false;

			state_ = sReadingHead;

			break;

		case sReadingHead:
			/* log_debug("blizzard::http::process(): sReadingHead"); */
			res = parse_title();

			if(res > 0)
			{
				/* log_debug("process():  parse_title() error %d", res); */
				state_ = sDone;
				quit = true;
			}
			else if(res < 0)
			{
				quit = true;
			}

			break;

		case sReadingHeaders:
			/* log_debug("blizzard::http::process(): sReadingHeaders"); */
			res = parse_header_line();

			if(res > 0)
			{
				/* log_debug("process():  parse_header_line() error %d", res); */
				state_ = sDone;
				quit = true;
			}
			else if(res < 0)
			{
				quit = true;
			}

			if(state_ == sReadyToHandle)
			{
				quit = true;
			}

			break;

		case sReadingPost:
			//in_post.print();
			res = parse_post();
			//in_post.print();
			if(res < 0)
			{
				quit = true;
			}

			break;

		case sReadyToHandle:
			/* log_debug("blizzard::http::process(): sReadyToHandle"); */

			commit();
			state_ = sWriting;
//            commit();

			break;

		case sWriting:
			/* log_debug("sWriting:"); */
			want_write = true;

			res = write_data();

			if(res < 0)
			{
				quit = true;
			}

			break;

		case sDone:
			/* log_debug("blizzard::http::process(): sDone reached!"); */
			quit = true;

			break;

		default:

			break;
		}
	}
}

bool blizzard::http::network_tryread()
{
	if (-1 != fd)
	{
		/* log_debug("blizzard::http::process::ready_read(%d)", fd); */
		/* log_debug("read{c:%d w:%d st:%d}, write{c:%d w:%d st:%d}", can_read, want_read, stop_reading, can_write, want_write, stop_writing); */

		while (can_read && !stop_reading/* && want_read*/)
		{
			if (!in_headers.read_from_fd(fd, can_read, want_read, stop_reading))
			{
				state_ = sDone;
				response_status = 400;

				return false;
			}
		}
	}

	return true;
}

bool blizzard::http::network_trywrite()
{
	if(-1 != fd)
	{
		/* log_debug("blizzard::http::process::ready_write(%d)", fd); */
		/* log_debug("read{c:%d w:%d st:%d}, write{c:%d w:%d st:%d}", */
		 //                   can_read,  want_read, stop_reading,
		  //                  can_write, want_write, stop_writing);

		bool want_write = true;

		if(can_write && !stop_writing)
		{
			/* log_debug("out_title.write_to_fd"); */
			out_title.write_to_fd(fd, can_write, want_write, stop_writing);
		}

#if 0
		want_write = true;

		if (can_write && !stop_writing)
		{
			/* log_debug("out_headers.write_to_fd"); */
			out_headers.write_to_fd(fd, can_write, want_write, stop_writing);
		}
#endif

		want_write = true;

		if(out_post.get_data_size() && can_write && !stop_writing)
		{
			/* log_debug("out_post.write_to_fd"); */
			out_post.write_to_fd(fd, can_write, want_write, stop_writing);
		}

		/* log_debug("all writings done"); */
		set_wreof();
	}

	return 0;
}

int blizzard::http::write_data()
{
	while(can_write)
	{
		network_trywrite();
	}

	if(false == get_wreof())
	{
		state_ = sWriting;

		return -1;
	}
	else
	{
		state_ = sDone;

		return 0;
	}
}

char * blizzard::http::read_header_line()
{
	while(true)
	{
		want_read = true;

		if(!network_tryread())
		{
			break;
		}

		char * headers_data = (char*)in_headers.get_data();
		headers_data[in_headers.get_data_size()] = 0;

		char * begin = headers_data + in_headers.marker();

		char * nl = strchr(begin, '\n');

		if(nl)
		{
			in_headers.marker() = nl - headers_data + 1;

			*nl = 0;
			nl--;
			if(*nl == '\r')
			{
				*nl = 0;
			}

			return begin;
		}
		else if(!can_read || stop_reading)
		{
			break;
		}
		else //can read && !want_read && !stop_reading
		{
			/* log_message_r(LOG_ERROR, "header is larger than %d", (int)in_headers.page_size()); */
			state_ = sDone;
			break;
		}
	}

	return 0;
}

int blizzard::http::parse_title()
{
	/* log_debug("parse_title()"); */
	char * line = read_header_line();
	if(!line)
	{
		/* log_debug("got 0"); */
		return -1;
	}

	/* log_debug("read_header_line() = '%s'", line); */

	state_ = sDone;

	char * mthd = line;

	while(*mthd == ' ')mthd++;

	char * url = strchr(mthd, ' ');
	if(!url)
	{
		return 400;
	}

	while(*url == ' ')*url++ = 0;

	char * version = strchr(url, ' ');
	if(!version)
	{
		return 400;
	}

	while(*version == ' ')*version++ = 0;

	if(strncasecmp(version, "HTTP/", 5))
	{
		return 400;
	}

	version += 5;
	char * mnr = strchr(version, '.');
	if(!mnr)
	{
		return 400;
	}

	char * delim = strchr(url, '?');
	if(!delim)
	{
		delim = url - 1;
	}
	else
	{
		*delim++ = 0;
	}

	switch(mthd[0])
	{
		case 'g':
		case 'G':
			method = BLZ_METHOD_GET;
			break;

		case 'h':
		case 'H':
			method = BLZ_METHOD_HEAD;
			break;

		case 'p':
		case 'P':
			if(mthd[1] == 'o' || mthd[1] == 'O')
			{
				method = BLZ_METHOD_POST;
			}
			break;
		default:
			method = BLZ_METHOD_UNDEF;
			return 501;
	}

	protocol_major = atoi(version);
	protocol_minor = atoi(mnr + 1);

	uri_path = url;
	uri_params = delim;

	state_ = sReadingHeaders;

	return 0;
}

int blizzard::http::parse_header_line()
{
	/* log_debug("parse_header_line()"); */
	char * key = read_header_line();
	if(!key)
	{
		return -1;
	}

	/* log_debug("read_header_line() = '%s'", key); */

	if(0 == *key)
	{
		if(method == BLZ_METHOD_POST)
		{
			state_ = sReadingPost;
			/* log_debug("->sReadingPost"); */

			in_post.append_data((char*)in_headers.get_data() + in_headers.marker(), in_headers.get_data_size() - in_headers.marker());
		}
		else
		{
			state_ = sReadyToHandle;

			/* log_debug("->sReadyToHandle"); */
		}

		return 0;
	}

	state_ = sDone;

	while(*key == ' ')key++;
	char * val = strchr(key, ':');
	if(!val)
	{
		return 400;                              
	}

	*val++ = 0;
	while(*val == ' ')val++;

	if(header_items_num < MAX_HEADER_ITEMS && *key)
	{
		header_items[header_items_num].key = key;
		header_items[header_items_num].value = val;

		header_items_num++;

		/* log_debug("header_items['%s']='%s'", key, val); */

	}

	if(!strcasecmp(key, "connection") && !strcmp(val, "keep-alive"))
	{
		keep_alive = false;
	}
	else if(!strncasecmp(key, "content-len", 11))
	{
		int sz = atoi(val);
		in_post.resize(sz);

		/* log_debug("post body found (%d bytes)", sz); */
	}
	else if(!strcasecmp(key, "expect") && !strcasecmp(val, "100-continue")) //EVIL HACK for answering on "Expect: 100-continue"
	{       
		const char * ret_str = "HTTP/1.1 100 Continue\r\n\r\n";
		int ret_str_sz = 25;//strlen(ret_str);
		
		ssize_t wr = write(fd, ret_str, ret_str_sz);
		if(wr == -1 || wr < ret_str_sz)
		{
					  log_warn("client didn't receive '100 Continue'");       
		}
	}

	state_ = sReadingHeaders;

	return 0;
}

int blizzard::http::parse_post()
{
	/* log_debug("parse_post() (%d bytes)", (int)in_post.size()); */

	in_post.read_from_fd(fd, can_read, want_read, stop_reading);

	if(in_post.size() == in_post.capacity())
	{
		state_ = sReadyToHandle;
	}
	
	if(stop_reading && state_ != sReadyToHandle)
	{
		response_status = 400;
		state_ = sDone;
	}

	return -1;
}

int blizzard::http::commit()
{
	char buff[1024];

	char now_str[128];
	time_t now_time;
	time(&now_time);
	memset(now_str, 0, 128);
	strftime(now_str, 127, "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now_time));

		const char * resp_status_str = "Unknown";

	if(response_status >= http_codes_num || response_status == 0)
	{
		response_status = 404;
	}
	else
	{
			 resp_status_str = http_codes[response_status];
	}


	int l = snprintf(buff, 1023,
			"HTTP/%d.%d %d %s\r\n"
			"Server: blizzard/" BLZ_VERSION "\r\n"
			"Date: %s\r\n",
			   protocol_major, protocol_minor, response_status, resp_status_str, now_str);

	out_title.append_data(buff, l);

	if(!cache)
	{
		const char m[] = "Pragma: no-cache\r\nCache-control: no-cache\r\n";
		out_headers.append_data(m, sizeof(m) - 1);

		/* add_response_header("Pragma", "no-cache"); */
		/* add_response_header("Cache-control", "no-cache"); */
	}

	if (keep_alive)
	{
		add_response_header("Connection", "keep-alive");
	}
	else
	{
		add_response_header("Connection", "close");
	}

	if(out_post.get_data_size())
	{
		add_response_header("Accept-Ranges", "bytes");

		l = snprintf(buff, 1023, "Content-Length: %d\r\n", (int)out_post.get_total_data_size());
		out_headers.append_data(buff, l);
	}

	out_headers.append_data("\r\n", 2);

	out_title.append_data(out_headers.get_data(), out_headers.get_data_size());

	//log_debug("out_headers:---\n%s\n---", (char*)out_headers.get_data());

	return 0;
}

//-------------------------------------------------------------------------------------------------------------------

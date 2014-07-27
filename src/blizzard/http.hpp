#ifndef __BLIZZARD_HTTP_HPP__
#define __BLIZZARD_HTTP_HPP__

#include <ev.h>
#include <stdint.h>
#include <stddef.h>
#include "mem_chunk.hpp"
#include "plugin.hpp"

namespace blizzard {

struct http;

struct events
{
	http *con;
	ev_io watcher_recv;
	ev_io watcher_send;
	ev_timer watcher_timeout;
};

struct http : public blz_task
{
public:
	enum http_state {sUndefined, sReadingHead, sReadingHeaders, sReadingPost, sReadyToHandle, sWriting, sDone};

	events e;

protected:
	static int http_codes_num;
	static const char ** http_codes;

	enum {MAX_HEADER_ITEMS = 16};
	enum {READ_HEADERS_SZ = 8192};
	enum {WRITE_TITLE_SZ = 8192};
	enum {WRITE_HEADERS_SZ = 4096};
	enum {WRITE_BODY_SZ = 32768};

	int fd;

	struct ev_loop *server_loop;
	double response_time;

	bool want_read;
	bool want_write;
	bool can_read;
	bool can_write;

	bool stop_reading;
	bool stop_writing;

	volatile bool locked;

	mem_chunk<READ_HEADERS_SZ>    in_headers;
	mem_block                     in_post;

	mem_chunk<WRITE_TITLE_SZ>     out_title;
	mem_chunk<WRITE_HEADERS_SZ>   out_headers;
	mem_chunk<WRITE_BODY_SZ>      out_post;

	http_state state_;

	struct header_item
	{
		const char *key;
		const char *value;
	}
	header_items[MAX_HEADER_ITEMS];

	int header_items_num;
	int method;

	int protocol_major;
	int protocol_minor;
	bool cache;

	struct in_addr in_ip;

	const char *uri_path;
	const char *uri_params;

	int response_status;

	bool ready_read()const;
	bool ready_write()const;

	bool network_tryread();
	bool network_trywrite();

	char * read_header_line();
	int parse_title();
	int parse_header_line();
	int parse_post();

	int commit();
	int write_data();

public:
	http();
	~http();

	void init(int fd, const struct in_addr& ip);
	void add_watcher(struct ev_loop *loop);
	void destroy();

	bool ready()const;

	void allow_read();
	void allow_write();

	bool get_rdeof();
	void set_rdeof();
	bool get_wreof();
	void set_wreof();

	void lock();
	void unlock();
	bool is_locked()const;

	int get_fd() const;

	double get_response_time() const;

	void process();

	http_state state()const;

	/* API */

	int              get_request_method() const;
	int              get_version_major() const;
	int              get_version_minor() const;
	bool             get_cache() const;
	struct in_addr   get_request_ip() const;
	const char*      get_request_uri_path() const;
	const char*      get_request_uri_params() const;
	size_t           get_request_body_len() const;
	const uint8_t*   get_request_body() const;
	const char*      get_request_header(const char *) const;
	size_t           get_request_headers_num() const;
	const char*      get_request_header_key(int) const;
	const char*      get_request_header_value(int) const;
	double           get_current_server_time() const;

	void             set_cache(bool);
	void             set_response_status(int);
	void             add_response_header(const char* name, const char* data);
	void             add_response_buffer(const char* data, size_t size);
};

}

#endif /* __BLIZZARD_HTTP_HPP__ */

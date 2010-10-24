#ifndef __BLZ_PLUGIN_HPP__
#define __BLZ_PLUGIN_HPP__

#include <stdint.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include <sys/types.h>

#define BLZ_VERSION "0.2.4" /* major.minor.patch[.quickfix] */
#define BLZ_VERSION_BINARY (0 << 24) + (2 << 16) + (4 << 8) + 0

#define BLZ_METHOD_UNDEF 0
#define BLZ_METHOD_GET   1
#define BLZ_METHOD_POST  2
#define BLZ_METHOD_HEAD  3

struct blz_task
{
	virtual ~blz_task(){};
	virtual int get_request_method() const = 0;

	virtual int              get_version_major() const = 0;
	virtual int              get_version_minor() const = 0;
	virtual bool             get_keepalive() const = 0;
	virtual bool             get_cache() const = 0;
	virtual struct in_addr   get_request_ip() const = 0;
	virtual const char*      get_request_uri_path() const = 0;
	virtual const char*      get_request_uri_params() const = 0;
	virtual size_t           get_request_body_len() const = 0;
	virtual const uint8_t*   get_request_body() const = 0;
	virtual const char*      get_request_header(const char*) const = 0;
	virtual size_t           get_request_headers_num() const = 0;
	virtual const char*      get_request_header_key(int) const = 0;
	virtual const char*      get_request_header_value(int) const = 0;

	virtual void set_response_status(int) = 0;
	virtual void set_keepalive(bool) = 0;
	virtual void set_cache(bool) = 0;

	virtual void add_response_header(const char* name, const char* data) = 0;
	virtual void add_response_buffer(const char* data, size_t sz) = 0;
};

#define BLZ_OK 0
#define BLZ_ERROR 1
#define BLZ_AGAIN 2

struct blz_plugin
{
	blz_plugin() {}
	virtual ~blz_plugin() throw() {}

	virtual int load(const char* cfg) = 0;
	virtual int easy(blz_task* tsk) = 0;
	virtual int hard(blz_task* tsk) = 0;
	virtual int idle() { return BLZ_OK; }
};

extern "C" blz_plugin* get_plugin_instance();

#endif /* __BLZ_PLUGIN_HPP__ */

#include <stdio.h>
#include <inttypes.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <coda/string.hpp>
#include "plugin.hpp"
#include "statistics.hpp"

#define MAX_TIME 1e10

blizzard::statistics::statistics()
{
	last_processed_time = time(0);

	c_reqs_count = 0;
	c_resp_time_total = 0;
	c_resp_time_min = MAX_TIME;
	c_resp_time_max = 0;
	c_easy_queue_max_len = 0;
	c_hard_queue_max_len = 0;
	c_done_queue_max_len = 0;
	c_easy_queue_len = 0;
	c_hard_queue_len = 0;
	c_done_queue_len = 0;

	p_resp_time_min = 0;
	p_resp_time_avg = 0;
	p_resp_time_max = 0;
	p_avg_rps = 0;
	p_easy_queue_max_len = 0;
	p_hard_queue_max_len = 0;
	p_done_queue_max_len = 0;
}

void blizzard::statistics::process(double now)
{
	if (TIME_DELTA < now - last_processed_time)
	{
		if (c_reqs_count)
		{
			p_resp_time_min = c_resp_time_min;
			p_resp_time_avg = c_resp_time_total / c_reqs_count;
			p_resp_time_max = c_resp_time_max;
		}
		else
		{
			p_resp_time_min = 0;
			p_resp_time_avg = 0;
			p_resp_time_avg = 0;
		}

		p_avg_rps = (double) c_reqs_count / TIME_DELTA;

		p_easy_queue_max_len = c_easy_queue_max_len;
		p_hard_queue_max_len = c_hard_queue_max_len;
		p_done_queue_max_len = c_done_queue_max_len;

		c_reqs_count = 0;
		c_resp_time_total = 0;
		c_resp_time_min = MAX_TIME;
		c_resp_time_max = 0;
		c_easy_queue_max_len = 0;
		c_hard_queue_max_len = 0;
		c_done_queue_max_len = 0;

		last_processed_time = now;
	}
}

void blizzard::statistics::report_response_time(double t)
{
	c_resp_time_total += t;
	if (t > c_resp_time_max) c_resp_time_max = t;
	if (t < c_resp_time_min) c_resp_time_min = t;
	c_reqs_count++;
}

void blizzard::statistics::report_easy_queue_len(size_t len)
{
	c_easy_queue_len = len;
	if (len > c_easy_queue_max_len) c_easy_queue_max_len = len;
}

void blizzard::statistics::report_hard_queue_len(size_t len)
{
	c_hard_queue_len = len;
	if (len > c_hard_queue_max_len) c_hard_queue_max_len = len;
}

void blizzard::statistics::report_done_queue_len(size_t len)
{
	c_done_queue_len = len;
	if (len > c_done_queue_max_len) c_done_queue_max_len = len;
}

void blizzard::statistics::generate_xml(std::string &xml, time_t start_time, uint32_t pages_in_http_pool, uint32_t objects_in_http_pool)
{
	time_t uptime = time(NULL) - start_time;

	struct rusage usage;
	::getrusage(RUSAGE_SELF, &usage);

	coda_strappend(xml,
		"<blizzard_stats>\n"
		"	<blizzard_version>"BLZ_VERSION"</blizzard_version>\n"
		"	<uptime>%d</uptime>\n"
		"	<rps>%.3f</rps>\n"
		"	<queues>\n"
		"		<easy>%"PRIuMAX"</easy>\n"
		"		<max_easy>%"PRIuMAX"</max_easy>\n"
		"		<hard>%"PRIuMAX"</hard>\n"
		"		<max_hard>%"PRIuMAX"</max_hard>\n"
		"		<done>%"PRIuMAX"</done>\n"
		"		<max_done>%"PRIuMAX"</max_done>\n"
		"	</queues>\n"
		"	<response_time>\n"
		"		<min>%.6f</min>\n"
		"		<avg>%.6f</avg>\n"
		"		<max>%.6f</max>\n"
		"	</response_time>\n"
		"	<mem_allocator>\n"
		"		<pages>%"PRIu32"</pages>\n"
		"		<objects>%"PRIu32"</objects>\n"
		"	</mem_allocator>\n"
		"	<rusage>\n"
		"		<utime>%d</utime>\n"
		"		<stime>%d</stime>\n"
		"	</rusage>\n"
		"</blizzard_stats>\n"

		, (int) uptime
		, p_avg_rps
		, (uintmax_t) c_easy_queue_len
		, (uintmax_t) p_easy_queue_max_len
		, (uintmax_t) c_hard_queue_len
		, (uintmax_t) p_hard_queue_max_len
		, (uintmax_t) c_done_queue_len
		, (uintmax_t) p_done_queue_max_len
		, p_resp_time_min
		, p_resp_time_avg
		, p_resp_time_max
		, pages_in_http_pool
		, objects_in_http_pool
		, (int) usage.ru_utime.tv_sec
		, (int) usage.ru_stime.tv_sec
	);
}


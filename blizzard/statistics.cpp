#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "plugin.hpp"
#include "statistics.hpp"

#define MAX_TIME 1e10

blizzard::statistics::statistics()
{
	last_processed_time = time(0);

	easy_queue_len = 0;
	hard_queue_len = 0;
	done_queue_len = 0;
	easy_queue_max_len = 0;
	hard_queue_max_len = 0;
	done_queue_max_len = 0;  
	eq_ml = 0;
	hq_ml = 0;
	dq_ml = 0;

	avg_rps = 0;

	requests_count = 0;
	resp_time_total = 0;
	resp_time_min = (double) MAX_TIME + 1;
	resp_time_mid = 0;
	resp_time_max = 0;
	min_t = 0;
	max_t = 0;
}

double blizzard::statistics::get_min_lifetime() const
{
	return resp_time_min < MAX_TIME ? resp_time_min / 1000.0 : 0;
}

double blizzard::statistics::get_mid_lifetime() const
{
	return resp_time_mid / 1000.0;
}

double blizzard::statistics::get_max_lifetime() const
{
	return resp_time_max / 1000.0;
}

double blizzard::statistics::get_rps()const
{
	return avg_rps;
}

void blizzard::statistics::process(double now)
{
	if (TIME_DELTA < now - last_processed_time)
	{
		resp_time_min = min_t;
		resp_time_mid = requests_count ? (resp_time_total / (requests_count)) : 0;
		resp_time_max = max_t;

		avg_rps = (double) requests_count / TIME_DELTA;

		easy_queue_max_len = eq_ml;
		hard_queue_max_len = hq_ml;
		done_queue_max_len = dq_ml;

		resp_time_total = 0;
		requests_count = 0;
		min_t = 0;
		max_t = 0;
		eq_ml = 0;
		hq_ml = 0;
		dq_ml = 0;

		last_processed_time = now;
	}
}

void blizzard::statistics::report_response_time(double t)
{
	resp_time_total += t;
	if (t > max_t) max_t = t;
	if (t < min_t) min_t = t;
	requests_count++;
}

void blizzard::statistics::report_easy_queue_len(size_t len)
{
	easy_queue_len = len;
	if (len > eq_ml) eq_ml = len;
}

void blizzard::statistics::report_hard_queue_len(size_t len)
{
	hard_queue_len = len;
	if (len > hq_ml) hq_ml = len;
}

void blizzard::statistics::report_done_queue_len(size_t len)
{
	done_queue_len = len;
	if (len > dq_ml) dq_ml = len;
}

void blizzard::statistics::generate_xml(std::string &xml, time_t start_time)
{
	xml = "<blizzard_stats>\n";

	char buff [1024];

	time_t up_time = time(0) - start_time;

	snprintf(buff, 1024, "\t<blizzard_version>"BLZ_VERSION"</blizzard_version>\n\t<uptime>%d</uptime>\n", (int)up_time);
	xml += buff;

	snprintf(buff, 1024, "\t<rps>%.4f</rps>\n", get_rps());
	xml += buff;

	snprintf(buff, 1024, "\t<queues>\n\t\t<easy>%d</easy>\n\t\t<max_easy>%d</max_easy>\n"
		"\t\t<hard>%d</hard>\n\t\t<max_hard>%d</max_hard>\n\t\t<done>%d</done>\n"
		"\t\t<max_done>%d</max_done>\n\t</queues>\n",
			(int) easy_queue_len,
			(int) easy_queue_max_len,
			(int) hard_queue_len,
			(int) hard_queue_max_len,
			(int) done_queue_len,
			(int) done_queue_max_len
	);
	xml += buff;

	snprintf(buff, 1024, "\t<conn_time>\n\t\t<min>%.4f</min>\n\t\t<avg>%.4f</avg>\n\t\t<max>%.4f</max>\n\t</conn_time>\n",
			get_min_lifetime(), get_mid_lifetime(), get_max_lifetime());
	xml += buff;

	snprintf(buff, 1024, "\t<mem_allocator>\n\t\t<pages>%d</pages>\n\t\t<objects>%d</objects>\n\t</mem_allocator>\n",
			(int) pages_in_http_pool, (int) objects_in_http_pool);
	xml += buff;

	struct rusage usage;
	::getrusage(RUSAGE_SELF, &usage);

	snprintf(buff, 1024, "\t<rusage>\n\t\t<utime>%d</utime>\n\t\t<stime>%d</stime>\n\t</rusage>\n",
			(int)usage.ru_utime.tv_sec, (int)usage.ru_stime.tv_sec);
	xml += buff;

	xml += "</blizzard_stats>\n";
}


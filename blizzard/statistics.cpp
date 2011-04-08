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

void blizzard::statistics::process()
{
	time_t curr_time = time(0);

	double diff = curr_time - last_processed_time;

	if (diff > TIME_DELTA)
	{
		resp_time_min = min_t;
		resp_time_mid = requests_count ? (resp_time_total / (requests_count)) : 0;
		resp_time_max = max_t;

		avg_rps = (double)requests_count / TIME_DELTA;

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

		last_processed_time = curr_time;
	}
}

void blizzard::statistics::report_response_time(uint64_t t)
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


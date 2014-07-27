#ifndef __BLIZZARD_STATISTICS_HPP__
#define __BLIZZARD_STATISTICS_HPP__

#include <stdint.h>
#include <time.h>
#include <string>

namespace blizzard {

struct statistics
{
	enum {TIME_DELTA = 4};

	double last_processed_time;

	volatile int c_reqs_count;
	volatile double c_resp_time_total;
	volatile double c_resp_time_min;
	volatile double c_resp_time_max;
	volatile size_t c_easy_queue_max_len;
	volatile size_t c_hard_queue_max_len;
	volatile size_t c_done_queue_max_len;
	volatile size_t c_easy_queue_len;
	volatile size_t c_hard_queue_len;
	volatile size_t c_done_queue_len;

	volatile double p_resp_time_min;
	volatile double p_resp_time_avg;
	volatile double p_resp_time_max;
	volatile double p_avg_rps;
	volatile size_t p_easy_queue_max_len; 
	volatile size_t p_hard_queue_max_len; 
	volatile size_t p_done_queue_max_len;  

public:
	statistics();

	void process(double now);
	void report_response_time(double t);
	void report_easy_queue_len(size_t len);
	void report_hard_queue_len(size_t len);
	void report_done_queue_len(size_t len); 

	void generate_xml(std::string &xml, time_t start_time, uint32_t pages_in_http_pool, uint32_t objects_in_http_pool);
};

} /* namespace blizzard */

#endif /* __BLIZZARD_STATISTICS_HPP__ */

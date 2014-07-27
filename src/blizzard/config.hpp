#ifndef __BLIZZARD_CONFIG_HPP__
#define __BLIZZARD_CONFIG_HPP__

#include <string>
#include <inttypes.h>
#include <coda/error.hpp>
#include <coda/logger.h>
#include <coda/txml.hpp>

#define SRV_BUF 4096

struct blz_config : public coda::txml_determination_object
{
	struct BLZ : public coda::txml_determination_object
	{
		std::string pid_file_name;
		std::string log_file_name;
		std::string log_level;

		struct STATS : public coda::txml_determination_object
		{
			std::string uri;

			STATS() {}

			void determine(coda::txml_parser* p)
			{
				txml_member(p, uri);
			}

			void clear()
			{
				uri.clear();
			}

			void check(const char *par, const char *ns)
			{
				char curns [SRV_BUF];
				snprintf(curns, SRV_BUF, "%s:%s", par, ns);

				if (uri.empty()) throw coda_error("<%s:uri> is empty in config", curns);
			}
		};

		struct PLUGIN : public coda::txml_determination_object
		{
			std::string ip;
			std::string port;
			int connection_timeout;
			int idle_timeout;

			std::string library;
			std::string params;

			int easy_threads;
			int hard_threads;

			int easy_queue_limit;
			int hard_queue_limit;

			PLUGIN()
				: connection_timeout(0)
				, idle_timeout(0)
				, easy_threads(1)
				, hard_threads(0)
				, easy_queue_limit(0)
				, hard_queue_limit(0)
			{}

			void determine(coda::txml_parser* p)
			{
				txml_member(p, ip);
				txml_member(p, port);
				txml_member(p, connection_timeout);
				txml_member(p, idle_timeout);
				txml_member(p, library);
				txml_member(p, params);
				txml_member(p, easy_threads);
				txml_member(p, hard_threads);
				txml_member(p, easy_queue_limit);
				txml_member(p, hard_queue_limit);
			}

			void clear()
			{
				ip.clear();
				port.clear();
				connection_timeout = 0;
				idle_timeout = 0;

				library.clear();
				params.clear();

				easy_threads = 1;
				hard_threads = 0;

				easy_queue_limit = 0;
				hard_queue_limit = 0;
			}

			void check(const char *par, const char *ns)
			{
				char curns [SRV_BUF];
				snprintf(curns, SRV_BUF, "%s:%s", par, ns);

				if (ip     .empty()) throw coda_error ("<%s:ip> is empty in config", curns);
				if (port   .empty()) throw coda_error ("<%s:port> is empty in config", curns);
				if (library.empty()) throw coda_error ("<%s:library> is empty in config", curns);

				if (0 == connection_timeout) throw coda_error ("<%s:connection_timeout> is not set or set to 0", curns);
				if (0 == easy_threads) throw coda_error ("<%s:easy_threads> is set to 0", curns);
			}
		};

		STATS stats;
		PLUGIN plugin;

		void determine(coda::txml_parser* p)
		{
			txml_member(p, pid_file_name);
			txml_member(p, log_file_name);
			txml_member(p, log_level);
			txml_member(p, stats);
			txml_member(p, plugin);
		}

		void clear()
		{
			pid_file_name.clear();
			log_file_name.clear();
			log_level.clear();
			stats.clear();
			plugin.clear();
		}

		void check()
		{
			const char *curns = "blizzard";

			if (log_level.empty()) throw coda_error ("<%s:log_level> is empty in config", curns);

			stats .check(curns, "stats");
			plugin.check(curns, "plugin");
		}
	};

	BLZ blz;

	void determine(coda::txml_parser* p)
	{
		p->determineMember("blizzard", blz);
		txml_member(p, blz);
	}

	void clear()
	{
		blz.clear();
	}

	void check()
	{
		blz.check();
	}
};

#endif /* __BLIZZARD_CONFIG_HPP__ */

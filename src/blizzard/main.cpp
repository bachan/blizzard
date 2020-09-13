#include <getopt.h>
#include <signal.h>
#include <sys/time.h>
#include <coda/error.h>
#include <coda/daemon.h>
#include <coda/logger.h>
#include "server.hpp"

int main(int argc, char** argv)
{
	coda_getopt_t opt;

	if (0 > coda_getopt_parse(argc, argv, &opt) || NULL == opt.config)
	{
		coda_getopt_usage(argc, argv);
		log_die(errno, "coda_getopt_parse(%d, %s)", argc, argv[0]);
	}

	if (0 > coda_daemon_load(&opt))
	{
		log_die(errno, "coda_daemon_load()");
	}

/* WRITE YOUR CODE HERE */

	try
	{
		blizzard::server server;

		while (0 == coda_terminate)
		{
			server.load_config(opt.config, opt.pid, opt.daemon);
			server.prepare();
			server.init_threads();
			server.join_threads();
			server.finalize();

			coda_changecfg = 0; /* TODO: remove this crap */
		}
	}
	catch (const std::exception& e)
	{
		coda_terminate = 1; /* TODO: remove this crap */

		log_crit("main: exception: %s", e.what());
	}

/* STOP IT */

	if (0 > coda_daemon_stop(&opt))
	{
		log_die(errno, "coda_daemon_stop()");
	}

	return 0;
}


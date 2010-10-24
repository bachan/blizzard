#ifndef __BLZ_PLUGIN_FACTORY_HPP__
#define __BLZ_PLUGIN_FACTORY_HPP__

#include <pthread.h>
#include "config.hpp"
#include "plugin.hpp"

namespace blizzard {

class plugin_factory
{
    void* loaded_module;
    blz_plugin* plugin_handle;

public:
	 plugin_factory();
	~plugin_factory();

	blz_plugin* open_plugin() const;

	void load_module(const blz_config::BLZ::PLUGIN& pd);
	void stop_module();
	void idle();
};

}

#endif /* __BLZ_PLUGIN_FACTORY_HPP__ */

#include <dlfcn.h>
#include "server.hpp"

blizzard::plugin_factory::plugin_factory()
	: loaded_module(NULL)
	, plugin_handle(NULL)
{
}

blizzard::plugin_factory::~plugin_factory()
{
	stop_module();
}

void blizzard::plugin_factory::load_module(const blz_config::BLZ::PLUGIN& pd)
{
	if (0 == pd.easy_threads)
	{
		return;
	}

	if (0 == loaded_module)
	{
		loaded_module = dlopen(pd.library.c_str(), RTLD_NOW | RTLD_LOCAL);

		if (loaded_module == 0)
		{
			throw coda_error("loading module %s failed: %s", pd.library.c_str(), dlerror());
		}
	}
	else
	{
		throw coda_error("module already loaded");
	}

	dlerror();

	union conv_union
	{
		void* v;
		blz_plugin* (*f)();
	} conv;

	conv.v = dlsym(loaded_module, "get_plugin_instance");
	blz_plugin* (*func)() = conv.f;

	const char* errmsg = dlerror();

	if (0 != errmsg)
	{
		throw coda_error("error searching 'get_plugin_instance' in module %s: %s", pd.library.c_str(), errmsg);
	}

	plugin_handle = (*func)();

	if (0 == plugin_handle)
	{
		throw coda_error("module %s: instance of plugin is not created", pd.library.c_str());
	}

	if (BLZ_OK != plugin_handle->load(pd.params.c_str()))
	{
		delete plugin_handle;
		plugin_handle = 0;
		throw coda_error("module init failed");
	}
}

void blizzard::plugin_factory::stop_module()
{
	if (plugin_handle)
	{
		delete plugin_handle;
		plugin_handle = 0;
	}

	if (loaded_module)
	{
		//FIXME: valgrind looses symbol names if we close the library
		//dlclose(loaded_module);
		loaded_module = 0;
	}
}

blz_plugin* blizzard::plugin_factory::open_plugin() const
{
	return plugin_handle;
}

void blizzard::plugin_factory::idle()
{
	if (plugin_handle)
	{
		plugin_handle->idle();
	}
}


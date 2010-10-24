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
			char buff[2048];
			snprintf(buff, 2048, "loading module '%s' failed: %s", pd.library.c_str(), dlerror());

			throw std::logic_error(buff);
		}
	}
	else
	{
		throw std::logic_error("module already loaded!!!");
	}

	dlerror();

	union conv_union
	{
		void* v;
		blz_plugin* (*f)();
	} conv;

	conv.v = dlsym(loaded_module, "get_plugin_instance");
	blz_plugin* (*func)() = conv.f;

	const char * errmsg = dlerror();
	if(0 != errmsg)
	{
		char buff[2048];
		snprintf(buff, 2048, "error searching 'get_plugin_instance' in module '%s': '%s'", pd.library.c_str(), errmsg);

		throw std::logic_error(buff);
	}

	plugin_handle = (*func)();

	if(0 == plugin_handle)
	{
		char buff[2048];
		snprintf(buff, 2048, "module '%s': instance of plugin is not created", pd.library.c_str());

		throw std::logic_error(buff);
	}

	if (BLZ_OK != plugin_handle->load(pd.params.c_str()))
	{
		throw std::logic_error("Plugin init failed");
	}
}

void blizzard::plugin_factory::stop_module()
{
	if (plugin_handle)
	{
//		delete plugin_handle;
		plugin_handle = 0;
	}

	if (loaded_module)
	{
		dlclose(loaded_module);
		loaded_module = 0;
	}
}

blz_plugin* blizzard::plugin_factory::open_plugin() const
{
	return plugin_handle;
}

void blizzard::plugin_factory::idle()
{
	if(plugin_handle)
	{
		plugin_handle->idle();
	}
}

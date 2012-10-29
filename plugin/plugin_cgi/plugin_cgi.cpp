#include "plugin.h"
#include "client.h"
#include "http.h"

#include <iostream>

#define CGI "[CGI] "
#define TRACE() do { std::cerr << CGI << __FUNCTION__ << std::endl; }while(0)

class PluginCgi: public Plugin
{
    virtual bool OnTimer(Server *server, int plugin_index)
    {
        TRACE();
        return true;
    }
};

extern "C" Plugin* SetupPlugin()
{
    TRACE();
    return new PluginCgi();
}
extern "C" Plugin* RemovePlugin(Plugin *plugin)
{
    TRACE();
    delete plugin;
}

#include "plugin.h"

#include <stdlib.h>

Plugin::Plugin()
{
    m_plugin_data   = NULL;
    m_is_loaded     = false;
    m_setup_plugin  = NULL;
    m_remove_plugin = NULL;
}
Plugin::~Plugin(){}

bool Plugin::OnLoad(Server *server, int plugin_index){return true;}
bool Plugin::OnInit(Client *client, int plugin_index){return true;}
bool Plugin::BeforeRequest(Client *client, int plugin_index){return true;}
bool Plugin::OnRequest(Client *client, int plugin_index){return true;}
bool Plugin::AfterRequest(Client *client, int plugin_index){return true;}
bool Plugin::BeforeResponse(Client *client, int plugin_index){return true;}
PluginStatus Plugin::OnResponse(Client *client, int plugin_index){return OK;}
bool Plugin::AfterResponse(Client *client, int plugin_index){return true;}
void Plugin::OnClose(Client *client, int plugin_index){}
void Plugin::OnDestroy(Server *server, int plugin_index){}

// add new plugin here, absolute path is welcomed ;)
const char * plugin_config[] = {
   "plugin/slow_query/plugin_slow_query.so",
   "plugin/fake_mysql/plugin_fake_mysql.so", 
   NULL 
};

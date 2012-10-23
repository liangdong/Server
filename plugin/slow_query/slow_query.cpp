#include "plugin.h"
#include "client.h"

#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <sstream>

#define PNAME "[PluginSlowQuery] "
#define TRACE() do { std::cerr << PNAME << __FUNCTION__ << std::endl; } while(0)

struct SlowQueryData
{
    time_t start_time;
    bool   ok;
};

class PluginSlowQuery: public Plugin
{
    public:
        virtual bool OnLoad(Server *server, int plugin_index)
        {
            TRACE();
            return true;
        }
        virtual void OnDestroy(Server *server, int plugin_index)
        {
            TRACE();
        }
        virtual bool OnInit(Client *client, int plugin_index)
        {
            TRACE();
            
            SlowQueryData *data = new SlowQueryData();
            client->m_plugin_data_slots[plugin_index] = (void*)data;

            return true;
        }
        virtual void OnClose(Client *client, int plugin_index)
        {
            TRACE();
            delete (SlowQueryData*)client->m_plugin_data_slots[plugin_index];
        }
        virtual bool BeforeRequest(Client *client, int plugin_index)
        {
            TRACE();

            SlowQueryData *data = (SlowQueryData*)client->m_plugin_data_slots[plugin_index];
            data->start_time = time(NULL);
            data->ok = false;

            return true;
        }
        virtual bool AfterRequest(Client *client, int plugin_index)
        {
            TRACE();
            return true; 
        }
        virtual PluginStatus OnResponse(Client *client, int plugin_index)
        {
            //TRACE();
            
            SlowQueryData *data = (SlowQueryData*)client->m_plugin_data_slots[plugin_index];
            
            if (data->ok)
            {
                return OK;
            }

            time_t delta = time(NULL) - data->start_time;
            
            if (delta > 5)
            {
                std::ostringstream ostream;
                ostream << PNAME "echo: Time Used=" << delta << "\n";
                client->m_response += ostream.str();
                data->ok = true;
                
                return OK;
            }
            else
            {
                return NOT_OK;
            }
        }
};

extern "C" Plugin* SetupPlugin()
{
    TRACE();
    return new PluginSlowQuery();
}
extern "C" void RemovePlugin(Plugin *plugin)
{
    TRACE();
    delete plugin;
}

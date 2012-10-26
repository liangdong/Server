#include "plugin.h"
#include "client.h"
#include "http.h"

#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <sstream>

#define PNAME "[PluginSlowQuery] "
#define TRACE() do { std::cerr << PNAME << __FUNCTION__ << std::endl; } while(0)

struct SlowQueryData
{
    time_t start_time;
    //@2012/10/26
    //New Fix: A plugin callback ON_RESPONE will never callback twiceif it has already return OK before!
    //So Plugin developer needn't worry about to keep the plugin's status with a client any more
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
            
            //@2012/10/26 
            time_t delta = time(NULL) - data->start_time;
             
            if (delta > 1) 
            {
                std::ostringstream ostream;
                
                ostream << PNAME << " echo:\n";
                ostream << "Method=" << client->m_request.m_method << "\n"
                        << "Url=" << client->m_request.m_url << "\n" 
                        << "Headers=" << "{\n";
                
                HttpRequest::HeaderDict &dict = client->m_request.m_headers;
                HttpRequest::HeaderIter iter = dict.begin();

                while (iter != dict.end())
                {
                    ostream << iter->first << "=" << iter->second << "\n";
                    ++ iter;
                }

                ostream << "}\n" 
                        << "Body=" << client->m_request.m_body << "\n";

                client->m_response += ostream.str();
                return OK; //once we return OK, the main frame won't call this OnResponse again
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

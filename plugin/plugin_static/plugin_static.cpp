#include "plugin.h"
#include "client.h"
#include "server.h"
#include "http.h"

#include "event2/event.h"
#include "event2/util.h"

#include <string.h>
#include <stdlib.h>

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <regex.h>

#include <iostream>
#include <string>

#define STATIC "[Static] "
#define TRACE() do { std::cerr << STATIC << __FUNCTION__ << std::endl; }while(0)

enum StaticStatus
{
    INIT,
    FLOW,
    OVER,
    NON_EXIST,
    NON_ACCESS
};

struct StaticData
{
    StaticData()
    {
        m_fd     = -1;
        m_status = INIT;
    }

    int          m_fd;
    std::string  m_buf;
    std::string  m_all;
    StaticStatus m_status;
};

class PluginStatic: public Plugin
{
    virtual bool OnTimer(Server *server, int plugin_index)
    {
        TRACE();
        return true;
    }
    
    virtual bool OnInit(Client *client, int plugin_index)
    {
        TRACE();
        
        StaticData *sdata = new StaticData();
        
        sdata->m_fd = -1;
        sdata->m_buf.reserve(10 * 1024 * 1024);
        
        client->m_plugin_data_slots[plugin_index] = sdata;

        return true;
    }

    virtual bool BeforeResponse(Client *client, int plugin_index)
    {
        TRACE();
        
        StaticData  *sdata   = (StaticData*)client->m_plugin_data_slots[plugin_index];
        HttpRequest *request = client->m_request;

        regex_t    reg;
        regmatch_t pmatch;
        int        nmatch;

        regcomp(&reg, "^/doc/[^/]*$", REG_EXTENDED);
        nmatch = regexec(&reg, request->m_url.c_str(), 1, &pmatch, 0);
        
        if (nmatch)
        {
            sdata->m_status = NON_ACCESS;
        }
        else
        {
            std::string path = request->m_url.substr(1);

            if (access(path.c_str(), R_OK) == -1)
            {
                sdata->m_status = NON_EXIST;
            }
            else
            {
                sdata->m_status = INIT;
            }
        }
        
        return true;
    }
    
    virtual PluginStatus OnResponse(Client *client, int plugin_index)
    {
        TRACE();

        HttpRequest *request = client->m_request;
        StaticData  *sdata   = (StaticData*)client->m_plugin_data_slots[plugin_index];

        if (sdata->m_status == INIT)
        {
            sdata->m_status = FLOW;
            sdata->m_fd     = open(request->m_url.substr(1).c_str(), O_RDONLY);
            return NOT_OK;
        }
        else if (sdata->m_status == NON_ACCESS)
        {
            client->m_response.m_code    = 404;
            client->m_response.m_explain = "Access Deny";
        }
        else if (sdata->m_status == NON_EXIST)
        {
            client->m_response.m_code    = 403;
            client->m_response.m_explain = "File don't exist";
        }
        else
        {
            int ret = read(sdata->m_fd, &sdata->m_buf[0], sdata->m_buf.capacity());

            if (ret <= 0)
            {
                sdata->m_status = OVER;
                client->m_response.m_body += sdata->m_all;
            }
            else
            {
                sdata->m_all.append(&sdata->m_buf[0], 0, ret);
                return NOT_OK;
            }
        }

        return OK;
    }

    virtual bool AfterResponse(Client *client, int plugin_index)
    {
        TRACE();
        
        StaticData *sdata = (StaticData*)client->m_plugin_data_slots[plugin_index];

        if (sdata->m_status == OVER)
        {
            close(sdata->m_fd);
            sdata->m_fd = -1;
            sdata->m_all.clear();
        }
        
        return true;
    }

    virtual void OnClose(Client *client, int plugin_index)
    {
        TRACE();

        StaticData *sdata = (StaticData*)client->m_plugin_data_slots[plugin_index];

        if (sdata->m_fd != -1)
        {
            close(sdata->m_fd);
        }

        delete sdata;
    }

    virtual void OnDestroy(Server *server, int plugin_index)
    {
    }
};

extern "C" Plugin* SetupPlugin()
{
    TRACE();
    return new PluginStatic();
}
extern "C" Plugin* RemovePlugin(Plugin *plugin)
{
    TRACE();
    delete plugin;
}

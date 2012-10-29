#include "plugin.h"
#include "client.h"
#include "server.h"
#include "http.h"

#include "event2/event.h"
#include "event2/util.h"

#include <stdlib.h>

#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <iostream>
#include <string>

#define CGI "[CGI] "
#define TRACE() do { std::cerr << CGI << __FUNCTION__ << std::endl; }while(0)

enum CgiStatus
{
    CGI_INIT,
    CGI_IGNORE,
    CGI_NOT_EXIST,
    CGI_OK,
    CGI_NOT_OK,
    CGI_ERROR,
    CGI_FINISH
};

struct CgiData
{
    CgiStatus     m_status;

    int           m_data_pipe[2];
    std::string   m_data_recv;
    std::string   m_data_temp;

    struct event *m_event;

    Client       *m_client;
    int           m_plugin_index;
};

class PluginCgi: public Plugin
{
    virtual bool OnTimer(Server *server, int plugin_index)
    {
        TRACE();
        
        while (1)
        {
            pid_t pid = waitpid(-1, NULL, WNOHANG);

            if (pid > 0)
            {
                std::cerr << CGI << "waitpid=" << pid << std::endl;
            }
            else if (pid == 0)
            {
                return true;
            }
            else
            {
                if (errno != ECHILD)
                {
                    continue;
                }
                return true;
            }
        }

        return true;
    }
    
    virtual bool OnInit(Client *client, int plugin_index)
    {
        TRACE();

        CgiData *cgi_data = new CgiData();

        if (!cgi_data)
        {
            return false;
        }
        
        cgi_data->m_status       = CGI_FINISH;        
        cgi_data->m_client       = client;
        cgi_data->m_plugin_index = plugin_index;
        cgi_data->m_data_temp.reserve(10 * 1024 * 1024);
        
        cgi_data->m_event        = NULL;
        cgi_data->m_data_pipe[0] = -1;
        cgi_data->m_data_pipe[1] = -1;

        client->m_plugin_data_slots[plugin_index] = cgi_data;
        
        return true;
    }

    virtual bool BeforeResponse(Client *client, int plugin_index)
    {
        TRACE();

        CgiData *cgi_data    = (CgiData*)client->m_plugin_data_slots[plugin_index];
        HttpRequest *request = client->m_request;

        std::string::size_type suffix_index = request->m_url.rfind(".cgi");
        
        if (suffix_index == std::string::npos || 
            suffix_index != request->m_url.size() - 4)
        {
            cgi_data->m_status = CGI_IGNORE;
            return true; //uri is not a cgi, we ignore it.
        }

        std::string cgi_path = request->m_url.substr(1);
        
        if (access(cgi_path.c_str(), X_OK) == -1)
        {
            cgi_data->m_status = CGI_NOT_EXIST;
            return true;
        }

        cgi_data->m_status = CGI_INIT;

        return true;
    }
    
    virtual PluginStatus OnResponse(Client *client, int plugin_index)
    {
        TRACE();

        CgiData     *cgi_data = (CgiData*)client->m_plugin_data_slots[plugin_index];
        HttpRequest *request  = client->m_request;

        if (cgi_data->m_status == CGI_INIT)
        {
            pipe(cgi_data->m_data_pipe);
            evutil_make_socket_nonblocking(cgi_data->m_data_pipe[0]);

            pid_t pid = fork();
            
            if (pid == -1)
            {
                cgi_data->m_status = CGI_ERROR;
                return ERROR;
            }
            else if (pid == 0)
            {
                dup2(cgi_data->m_data_pipe[1], 1);
                close(cgi_data->m_data_pipe[1]);

                //this can be attack easily
                std::string path = "." + request->m_url;
                
                // temp test
                execlp("sh", "sh", "-c", path.c_str(), NULL);
                exit(127);
            }
            
            close(cgi_data->m_data_pipe[1]);
            cgi_data->m_data_pipe[1] = -1;
            
            cgi_data->m_event = event_new(client->m_server->m_server_base,
                                           cgi_data->m_data_pipe[0], 
                                           EV_READ | EV_PERSIST, 
                                           PluginCgiEventCallback, cgi_data); 
            event_add(cgi_data->m_event, NULL);
            
            cgi_data->m_status = CGI_NOT_OK;
        }
        else if (cgi_data->m_status == CGI_IGNORE)
        {
            return OK;
        }
        else if (cgi_data->m_status == CGI_NOT_EXIST)
        {
            client->m_response.m_code    = 403;
            client->m_response.m_explain = "CGI NOT EXSIT";
            return OK;
        }
        else if (cgi_data->m_status == CGI_OK)
        {
            client->m_response.m_body = cgi_data->m_data_recv;
            std::cerr << client->m_response.m_body << std::endl;
            return OK;
        }
        else if (cgi_data->m_status == CGI_ERROR)
        {
            return ERROR;
        }
        
        return NOT_OK;
    }

    virtual bool AfterResponse(Client *client, int plugin_index)
    {
        TRACE();

        CgiData *cgi_data  = (CgiData*)client->m_plugin_data_slots[plugin_index];
        
        if (cgi_data->m_status == CGI_OK)
        {
            close(cgi_data->m_data_pipe[0]);
            event_free(cgi_data->m_event);
        }

        cgi_data->m_event = NULL;
        cgi_data->m_data_pipe[0] = -1;
        cgi_data->m_data_pipe[1] = -1;
        cgi_data->m_data_recv.clear();

        cgi_data->m_status = CGI_FINISH;

        return true;
    }

    virtual void OnClose(Client *client, int plugin_index)
    {
        TRACE();

        CgiData *cgi_data  = (CgiData*)client->m_plugin_data_slots[plugin_index];
        
        if (cgi_data->m_event)
        {
            event_free(cgi_data->m_event);
        }

        if (cgi_data->m_data_pipe[0] != -1)
        {
            close(cgi_data->m_data_pipe[0]);
        }

        if (cgi_data->m_data_pipe[1] != -1)
        {
            close(cgi_data->m_data_pipe[1]);
        }
        
        delete cgi_data;
    }

    virtual void OnDestroy(Server *server, int plugin_index)
    {
        //kill(0, SIGKILL);

        //pid_t pid;

        //while ((pid = wait(NULL)) == -1 && errno != ECHILD || pid > 0);
    }

    static void PluginCgiEventCallback(evutil_socket_t sockfd, short event, void *userdata)
    {
        TRACE();

        CgiData *cgi_data = (CgiData*)userdata;
        
        int ret = read(cgi_data->m_data_pipe[0], &cgi_data->m_data_temp[0], cgi_data->m_data_temp.capacity());

        if (ret == -1)
        {
            if (errno != EAGAIN && errno != EINTR)
            {
                cgi_data->m_status = CGI_ERROR; 
            }
        }
        else if (ret == 0)
        {
            cgi_data->m_status = CGI_OK;
        }
        else
        {
            cgi_data->m_data_recv.append(&cgi_data->m_data_temp[0], 0, ret);
        }
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

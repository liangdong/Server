#include "plugin.h"
#include "client.h"
#include "server.h"
#include "http.h"

#include "event2/event.h"
#include "event2/util.h"

#include <string.h>
#include <stdlib.h>

#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <regex.h>

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
    char* * PutEnv(char* *env, int &size, const std::string &key, const std::string &value)
    {
        env = (char* *)realloc(env, (++ size) * sizeof(char*));
        
        std::string pair = key + "=" + value;

        env[size - 2] = strdup(pair.c_str());
        env[size - 1] = NULL;

        return env;
    }

    char* const * PrepareCgiEnviron(HttpRequest *request)
    {
        int    size = 1;
        char* *env  = (char* *)malloc(sizeof(char*));
        env[0]      = NULL;
        
        env = PutEnv(env, size, "method", request->m_method);
        env = PutEnv(env, size, "url", request->m_url);
        env = PutEnv(env, size, "body", request->m_body);
        
        HeaderIter iter = request->m_headers.begin();

        while (iter != request->m_headers.end())
        {
            env = PutEnv(env, size, iter->first, iter->second);
            ++ iter;
        }

        return env;
    }

    CgiStatus ValidateCgiUri(const std::string &uri)
    {
        std::string::size_type suffix_index = uri.rfind(".cgi");
        
        if (suffix_index == std::string::npos || suffix_index != uri.size() - 4)
        {
            return CGI_IGNORE;
        }

        regex_t    reg;
        regmatch_t pmatch;
        int        nmatch;

        regcomp(&reg, "^/cgi/[^\\.]*.cgi$", REG_EXTENDED);
        nmatch = regexec(&reg, uri.c_str(), 1, &pmatch, 0);

        if (nmatch)
        {
            return CGI_IGNORE;
        }
    
        if (access(uri.substr(1).c_str(), X_OK) == -1)
        {
            return CGI_NOT_EXIST;
        }

        return CGI_INIT;
    }

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

        CgiData     *cgi_data = (CgiData*)client->m_plugin_data_slots[plugin_index];
        HttpRequest *request  = client->m_request;

        cgi_data->m_status = ValidateCgiUri(request->m_url);

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

                std::string path = request->m_url.substr(1);
                
                char* const *env = PrepareCgiEnviron(request);             

                execle("/bin/bash", "sh", "-c", path.c_str(), NULL, env);
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

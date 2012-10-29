#include "server.h"
#include "client.h"
#include "plugin.h"

#include <stdlib.h>
#include <dlfcn.h>

#include <iostream>

Server::Server(const std::string &ip, short port): m_listener(ip, port)
{
    m_server_base  = NULL;
    m_plugins      = NULL;
    m_plugin_count = 0;
}

Server::~Server()
{
    if (m_server_base)
    {
        // plugins must be unloaded before client dies
        // cause some plugin may now use client's data in a running thread
        
        UnloadPlugins();
        
        ClientMapIter iter = m_client_map.begin();
        
        while (iter != m_client_map.end())
        {
            Client *client = iter->second;
            delete(client);
            ++ iter;
        }
        
        // plugins can't be remove until client's plugin_data_slots has been cleared by plugins
        RemovePlugins();

        event_free(m_exit_event);    
        event_base_free(m_server_base);

        std::cerr << "Total Client: " << m_listener.m_count_client << std::endl;
    }

    std::cerr << "Server Closed" << std::endl;
}

bool Server::SetupPlugins()
{
    const char *plugin_path;
    int         plugin_index;

    for (plugin_index = 0; plugin_config[plugin_index]; ++ plugin_index)
    {
        plugin_path = plugin_config[plugin_index];
        
        void *so    = dlopen(plugin_path, RTLD_LAZY);
        
        if (!so)
        {
            std::cerr << dlerror() << std::endl;
            return false;
        }
    
        Plugin::SetupPlugin  sp = (Plugin::SetupPlugin )dlsym(so, "SetupPlugin");
        Plugin::RemovePlugin rp = (Plugin::RemovePlugin)dlsym(so, "RemovePlugin");

        if (!sp || !rp)
        {
            std::cerr << dlerror() << std::endl;
            dlclose(so);
            return false;
        }
    
        Plugin *plugin = sp();
        
        if (!plugin)
        {
            dlclose(so);
            return false;
        }

        plugin->m_setup_plugin  = sp;
        plugin->m_remove_plugin = rp;
    
        m_plugins = (Plugin* *) realloc(m_plugins, sizeof(*m_plugins) * (m_plugin_count + 1));
        m_plugins[m_plugin_count++] = plugin;

        plugin->m_plugin_so    = so;
        plugin->m_plugin_index = plugin_index;
    }

    return true;
}

void Server::RemovePlugins()
{
    int i;
    
    for (i = 0; i < m_plugin_count; ++ i)
    {
        Plugin *plugin = m_plugins[i];
        Plugin::RemovePlugin rp = plugin->m_remove_plugin; //must get a func copy, because plugin will be deleted
        rp(plugin);
        dlclose(plugin->m_plugin_so);
    }

    free(m_plugins);
}

bool Server::LoadPlugins()
{
    int     i;
    Plugin *plugin;
    
    for (i = 0; i < m_plugin_count; ++ i)
    {
        plugin = m_plugins[i];

        if (plugin->OnLoad(this, i))
        {
            plugin->m_is_loaded = true;
        }
        else
        {
            return false;
        }
    }
    
    return true;
}

void Server::UnloadPlugins()
{
    Plugin *plugin;
    int     i;

    for (i = 0; i < m_plugin_count; ++ i)
    {
        plugin = m_plugins[i];
        
        if (plugin->m_is_loaded)
        {
            plugin->OnDestroy(this, i);
        }
    }
}

void Server::ServerOnTimer(evutil_socket_t, short, void *userdata)
{
    Server *server = (Server*)userdata;

    Plugin *plugin;
    int     i;
    
    for (i = 0; i < server->m_plugin_count; ++ i)
    {
        plugin = server->m_plugins[i];

        if (!plugin->OnTimer(server, i))
        {
            event_base_loopexit(server->m_server_base, NULL);
            return;
        }
    }

    struct timeval timer;
    timer.tv_sec  = 1;
    timer.tv_usec = 0;
    
    evtimer_del(server->m_timer_event);
    evtimer_assign(server->m_timer_event, server->m_server_base, ServerOnTimer, server);
    evtimer_add(server->m_timer_event, &timer);
}

bool Server::StartServer()
{
    m_server_base = event_base_new();
    
    m_listener.InitListener(this);
    
    m_exit_event = evsignal_new(m_server_base, SIGINT, Server::ServerExitSignal, m_server_base);
    evsignal_add(m_exit_event, NULL);
    
    if (SetupPlugins() && LoadPlugins())
    {
        m_timer_event = evtimer_new(m_server_base, ServerOnTimer, this);

        struct timeval timer;
        timer.tv_sec  = 1;
        timer.tv_usec = 0;

        evtimer_add(m_timer_event, &timer);

        event_base_dispatch(m_server_base);
    }
    else
    {
        return false;
    }

    return true;
}

void Server::ServerExitSignal(evutil_socket_t signo, short, void *userdata)
{
    event_base_loopexit((struct event_base*)userdata, NULL);
}

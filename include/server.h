#ifndef _SERVER_H
#define _SERVER_H

#include "event2/event.h"
#include "event2/util.h"

#include "listener.h"

#include <map>
#include <string>

class Client;
class Plugin;

class Server
{
    public:
        typedef std::map<evutil_socket_t, Client*> ClientMap;
        typedef ClientMap::iterator ClientMapIter;

        Server(const std::string &ip, short port);
        ~Server();

        bool StartServer();

        static void ServerExitSignal(evutil_socket_t signo, short, void *userdata);
        
        Plugin*           *m_plugins;
        int                m_plugin_count;

        struct Listener    m_listener;
        struct event_base *m_server_base;
        struct event      *m_exit_event;
        ClientMap          m_client_map;

    private:
        bool SetupPlugins();
        void RemovePlugins();

        bool LoadPlugins();
        void UnloadPlugins();
};

#endif

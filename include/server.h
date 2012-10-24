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

        //watch for the ctrl+c(SIGINT) signal, once catched -> exit libevent loop
        static void ServerExitSignal(evutil_socket_t signo, short, void *userdata);
        
        Plugin*           *m_plugins;       //all register plugins
        int                m_plugin_count;  //count of plugins

        struct Listener    m_listener;      //listen for client
        struct event_base *m_server_base;   
        struct event      *m_exit_event;

        ClientMap          m_client_map;    //map a fd to a client online

    private:
        bool SetupPlugins();  //get plugin object from so
        void RemovePlugins(); //delete plugin object from so

        bool LoadPlugins();   //call each plugin's OnLoad callback to init some global plugin data
        void UnloadPlugins(); //call each plugin's UnLoad callback to free any  global plugin data
};

#endif

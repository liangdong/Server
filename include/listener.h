#ifndef _LISTENER_H
#define _LISTENER_H

#include "event2/event.h"
#include "event2/util.h"
#include "util.h"

#include <string>

class  Server;

struct Listener
{
    Server            *m_server;

    evutil_socket_t    m_sockfd;
    struct sockaddr_in m_listen_addr;
    struct event      *m_event;
    
    uint64_t           m_count_client;
    
    Listener(const std::string &ip, short port);
    ~Listener();
    
    void InitListener(Server *server);

    static void ListenerEventCallback(evutil_socket_t sockfd, short event, void *userdata);    
};

#endif

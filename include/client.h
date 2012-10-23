#ifndef _CLIENT_H
#define _CLIENT_H

#include "event2/event.h"
#include "event2/util.h"
#include "util.h"
#include "plugin.h"

#include <string>

enum ClientStatus
{
    BEFORE_REQUEST,
    ON_REQUEST,
    AFTER_REQUEST,
    BEFORE_RESPONSE,
    ON_RESPONSE,
    AFTER_RESPONSE,
};

class Server;

struct Client
{
    Server            *m_server;

    evutil_socket_t    m_sockfd;
    struct sockaddr_in m_client_addr;
    struct event      *m_event;
    
    std::string        m_inbuf;
    std::string        m_intemp;
    std::string        m_outbuf;
    
    std::string        m_request;
    std::string        m_response; 
    
    ClientStatus       m_status;
    
    void*             *m_plugin_data_slots;
    int                m_plugin_count;

    bool               m_want_write;
    bool               m_want_read;
    
    Client();
    ~Client(); 
    
    bool InitClient(Server *server);
    
    void WantRead();
    void NotWantRead();
    void WantWrite();
    void NotWantWrite();
    void SetWriteEvent();
    void UnsetWriteEvent();
    bool StatusMachine();
    void SetStatus(ClientStatus status);

    bool MakePluginDataSlots();
    void DelPluginDataSlots();
    
    bool PluginBeforeRequest();
    bool PluginOnRequest();
    bool PluginAfterRequest();
    bool PluginBeforeResponse();
    PluginStatus PluginOnResponse();
    bool PluginAfterResponse();

    static void ClientEventCallback(evutil_socket_t sockfd, short event, void *userdata);
};

#endif

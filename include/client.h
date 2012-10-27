#ifndef _CLIENT_H
#define _CLIENT_H

#include "event2/event.h"
#include "event2/util.h"

#include "util.h"
#include "plugin.h"
#include "http.h"

#include <queue>
#include <string>

enum ClientStatus
{
    BEFORE_REQUEST,
    ON_REQUEST,
    AFTER_REQUEST,
    BEFORE_RESPONSE,
    ON_RESPONSE,
    AFTER_RESPONSE,
    BEFORE_ERROR,
    ON_ERROR
};

enum RequestStatus
{
    REQ_ERROR,
    REQ_IS_COMPLETE,
    REQ_NOT_COMPLETE
};

class Server;

struct Client
{
    typedef std::queue<HttpRequest*> RequestQueue;

    Server            *m_server;

    evutil_socket_t    m_sockfd;
    struct sockaddr_in m_client_addr;
    struct event      *m_event;
    
    std::string        m_inbuf;             //all data read from client
    std::string        m_intemp;            //just for once read(man 3p read)
    std::string        m_outbuf;            //all data waiting to write to client
    RequestQueue       m_request_queue;     //queue to hold parsed request
    
    HttpRequest       *m_request_building;  //the request being built  
    HttpRequest       *m_request;           //the request being processed
    std::string        m_response;          //one response that will be appended to m_outbuf
    
    ClientStatus       m_status;  
    
    void*             *m_plugin_data_slots;     //plugin's data for this client
    int                m_plugin_count;      
    int                m_plugin_response_index; //@2012/10/26 which plugin to be called next on response

    bool               m_want_write;        //really want to keep write event
    bool               m_want_read;         //really want to keep read  event
    
    HttpParser         m_http_parser;       //Parse http byte-stream and get a http-request
    
    Client();
    ~Client();                              // unused 

    bool InitClient(Server *server);

    void WantRead();
    void NotWantRead();
    void WantWrite();
    void NotWantWrite();
    void SetWriteEvent();
    void UnsetWriteEvent();
    
    RequestStatus GetHttpRequest();
   
    bool StatusMachine();                   //core algorithm: status machine
    void SetStatus(ClientStatus status);

    bool MakePluginDataSlots();             //prepare for plugin's data-slot for this client
    void DelPluginDataSlots();              //free the plugin data for this client
    
    // a series of plugin callback wrapper
    bool PluginBeforeRequest();
    bool PluginOnRequest();
    bool PluginAfterRequest();
    bool PluginBeforeResponse();
    PluginStatus PluginOnResponse();
    bool PluginAfterResponse();

    static void ClientEventCallback(evutil_socket_t sockfd, short event, void *userdata);
};

#endif

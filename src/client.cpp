#include "client.h"
#include "server.h"
#include "plugin.h"
#include "event2/event.h"
#include "event2/util.h"

#include <iostream>
#include <sstream>

Client::Client()
{ 
    m_event = NULL; 
    m_plugin_data_slots = NULL;
    m_plugin_count = 0;
    m_request = NULL;
    m_request_building = NULL;
}

Client::~Client()
{
    if (m_event) 
    {
        DelPluginDataSlots();

        event_free(m_event);
        close(m_sockfd);
        
        HttpRequest *request;

        while (!m_request_queue.empty())
        {
            request = m_request_queue.front();    
            m_request_queue.pop();
            delete request;
        }

        if (m_request)
        {
            delete m_request;
        }
        if (m_request_building)
        {
            delete m_request_building;
        }
    }
}

//fix the ~Server->~Client Bug
void Client::FreeClient(Client *client)
{
    Server *server = client->m_server;

    if (client->m_event)
    {
        Server::ClientMapIter iter = server->m_client_map.find(client->m_sockfd);
        server->m_client_map.erase(iter);
    }

    delete client;
}

bool Client::InitClient(Server *server) 
{
    //this is what we first should do
    //TODO:m_ref_count = 1;
    m_server = server;

    m_intemp.reserve(10 * 1024 * 1024);
    m_inbuf.reserve(10 * 1024 * 1024);
    m_outbuf.reserve(10 * 1024 * 1024);
    
    m_http_parser.InitParser(this);

    evutil_make_socket_nonblocking(m_sockfd);
    m_event = event_new(m_server->m_server_base, m_sockfd, EV_PERSIST, Client::ClientEventCallback, this);
    event_add(m_event, NULL); 

    if (!MakePluginDataSlots())
    {
        return false;
    }

    SetStatus(BEFORE_REQUEST);
    
    // check to make sure plugin works well
    if (!StatusMachine())
    {
        return false;
    }
    
    return true;
}

bool Client::MakePluginDataSlots()
{    
    m_plugin_count = m_server->m_plugin_count;
    
    //there's no plugin at all, return true directly
    if (!m_plugin_count)
    {
        return true;
    }

    m_plugin_data_slots = new void*[m_plugin_count]; //no check for NULL, just keep it easy
    
    int i;

    for (i = 0; i < m_plugin_count; ++ i)
    {
        m_plugin_data_slots[i] = NULL;
    }

    Plugin * *plugins = m_server->m_plugins;

    for (i = 0; i < m_plugin_count; ++ i)
    {
        if (!plugins[i]->OnInit(this, i)) 
        {
            return false;
        }
    }
    return true;
}

void Client::DelPluginDataSlots()
{
    int i;

    Plugin* *plugins = m_server->m_plugins;

    for (i = 0; i < m_plugin_count; ++ i)
    {
        if (m_plugin_data_slots[i])
        {
            plugins[i]->OnClose(this, i);
        }
    }
    
    if (m_plugin_data_slots)
    { 
        delete []m_plugin_data_slots;
    }
}

void Client::WantRead()
{
    m_want_read = true;
    short event = event_get_events(m_event);
    event_del(m_event);
    event_assign(m_event, m_server->m_server_base, m_sockfd, event | EV_READ, Client::ClientEventCallback, this);
    event_add(m_event, NULL); 
}

void Client::NotWantRead()
{
    m_want_read = false;
    short event = event_get_events(m_event);
    event_del(m_event);
    event_assign(m_event, m_server->m_server_base, m_sockfd, event & (~EV_READ), Client::ClientEventCallback, this);
    event_add(m_event, NULL); 
}

// client really want write, 
// which means we should keep the write event whether or not there's anything to write right now.
void Client::WantWrite() 
{
    m_want_write = true;
    SetWriteEvent();
}

void Client::NotWantWrite()
{
    m_want_write = false; //we really don't want to write any more.
    
    //but if there's anything left, just keep the write event to send off the left bytes.
    if (!m_outbuf.size())     
    {
        UnsetWriteEvent();
    }
}

void Client::SetWriteEvent()
{
    short event = event_get_events(m_event);
    event_del(m_event);
    event_assign(m_event, m_server->m_server_base, m_sockfd, event | EV_WRITE, Client::ClientEventCallback, this);
    event_add(m_event, NULL); 
}

void Client::UnsetWriteEvent() 
{
    short event = event_get_events(m_event);
    event_del(m_event);
    event_assign(m_event, m_server->m_server_base, m_sockfd, event & (~EV_WRITE), Client::ClientEventCallback, this);
    event_add(m_event, NULL);
}

RequestStatus Client::GetHttpRequest()
{
    if (m_request_queue.size())
    {
        m_request = m_request_queue.front();
        m_request_queue.pop();
        return REQ_IS_COMPLETE;
    }

    int ret = m_http_parser.HttpParseRequest(m_inbuf);
    
    if (ret == -1)
    {
        return REQ_ERROR;
    }

    // this is import, because empty string or not complete field name or value 
    // is also allowed for http-parser, in which case it just return 0, which is what we don't regard as am error.
    if (ret == 0)
    {
        return REQ_NOT_COMPLETE;
    }

    m_inbuf.erase(0, ret);

    if (m_request_queue.size())
    {
        m_request = m_request_queue.front();
        m_request_queue.pop();
        return REQ_IS_COMPLETE;
    }
    
    return REQ_NOT_COMPLETE;
}

void Client::ClientEventCallback(evutil_socket_t sockfd, short event, void *userdata)
{
    Client *client = (Client*)userdata;

    if (event & EV_READ) 
    {
        int cap = client->m_intemp.capacity();
        int ret = read(sockfd, &client->m_intemp[0], cap);
    
        if (ret == -1)
        {
            if (errno != EAGAIN && errno != EINTR)
            {
                FreeClient(client);
                return;
            }
        }
        else if (ret == 0)
        {
            FreeClient(client); //client wants to leave, 
                           //so let it leave whether or not there's still any data waiting to send to it.
            return;
        }
        else
        {
            client->m_inbuf.append(client->m_intemp.c_str(), ret);
        }
    }
    
    if (event & EV_WRITE)
    {
        int ret = write(sockfd, client->m_outbuf.c_str(), client->m_outbuf.size());

        if (ret == -1)
        {
            if (errno != EAGAIN && errno != EINTR)
            {
                if (client->m_status != ON_RESPONSE)
                {
                    FreeClient(client);
                    return;
                }
            }
        }
        else
        {
            client->m_outbuf.erase(client->m_outbuf.begin(), client->m_outbuf.begin() + ret);

            //if client really want to keep write event, we can't unregister write event, even though there's nothing to send.
            if (client->m_outbuf.size() == 0 && !client->m_want_write)
            {
                client->UnsetWriteEvent();
            }
        }
    }

    // core logic, just call status machine to handle the whole thing, it's so easy :)
    if (!client->StatusMachine()) //only return false when m_status == ERROR
    {
        FreeClient(client); //some error happend, kick out the client
    }
}

//core algorithm !!
bool Client::StatusMachine()
{
    std::string::size_type newline;
    
    PluginStatus  plugin_status;
    RequestStatus request_status;
    
    std::ostringstream ostream;

    while (true)
    {
        switch (m_status)
        {
            case BEFORE_REQUEST:  //transitional status
                if (!PluginBeforeRequest()) {
                    return false;
                }
                
                WantRead();
                SetStatus(ON_REQUEST);
                break;
            case ON_REQUEST:      //lasting status
                if (!PluginOnRequest()) {
                    return false;
                }
                
                request_status = GetHttpRequest();
                
                if (request_status == REQ_ERROR) {
                    return false;
                } 
                else if (request_status == REQ_IS_COMPLETE) {
                    SetStatus(AFTER_REQUEST);
                    break;
                }
                else {
                    return true;
                }
            case AFTER_REQUEST:   //transitional status
                if (!PluginAfterRequest()) {
                    return false;
                }
                NotWantRead();
                SetStatus(BEFORE_RESPONSE);
                break;
            case BEFORE_RESPONSE: //transitional status
                if (!PluginBeforeResponse()) {
                    return false;
                }
                WantWrite();
                SetStatus(ON_RESPONSE);
                break;
            case ON_RESPONSE:    //lasting status
                plugin_status = PluginOnResponse();
                
                if (plugin_status == ERROR) {
                    SetStatus(BEFORE_ERROR);
                    continue;
                }
                else if (plugin_status == NOT_OK) {
                    return true;
                }
                
                // this is just a temporary response for test :)
                
                ostream << "HTTP/1.1 200 OK\r\n" 
                        << "Connection: keep-alive\r\n"
                        << "Content-Length:" << m_response.size() << "\r\n"
                        << "\r\n" << m_response;

                m_outbuf +=  ostream.str();
                
                SetStatus(AFTER_RESPONSE);
                break;
            case AFTER_RESPONSE:  //transitional status
                if (!PluginAfterResponse()) {
                    return false;
                }
                
                delete m_request;
                m_request = NULL;

                m_response.clear();
                
                NotWantWrite();
                SetStatus(BEFORE_REQUEST);
                break;
            case BEFORE_ERROR:
                m_outbuf += "HTTP/1.1 500 Server Error\r\n";
                m_outbuf += "Date: Fri, 27 October 2012 15:45:00 GMT\r\n";
                m_outbuf += "Server:Async Server by LiangDong\r\n";
                m_outbuf += "\r\n";
                SetStatus(ON_ERROR);
                break;
            case ON_ERROR:
                if (!m_outbuf.size())
                {
                    return false; //let the client leave, error 500 has been sent away
                }
                return true;
        }
    }

    return true;
}

void Client::SetStatus(ClientStatus status)
{
    m_status = status;
}

bool Client::PluginBeforeRequest()
{
    Plugin * *plugins = m_server->m_plugins;
    int i;

    for (i = 0; i < m_plugin_count; ++ i)
    {
        if (!plugins[i]->BeforeRequest(this, i)) {
            return false;
        }
    }

    return true;
}

bool Client::PluginOnRequest()
{
    Plugin * *plugins = m_server->m_plugins;
    int i;

    for (i = 0; i < m_plugin_count; ++ i)
    {
        if (!plugins[i]->OnRequest(this, i)) {
            return false;
        }
    }

    return true;
}

bool Client::PluginAfterRequest()
{
    Plugin * *plugins = m_server->m_plugins;
    int i;

    for (i = 0; i < m_plugin_count; ++ i)
    {
        if (!plugins[i]->AfterRequest(this, i)) {
            return false;
        }
    }

    return true;
}

bool Client::PluginBeforeResponse()
{
    Plugin * *plugins = m_server->m_plugins;
    int i;

    for (i = 0; i < m_plugin_count; ++ i)
    {
        if (!plugins[i]->BeforeResponse(this, i)) {
            return false;
        }
    }

    //Start To Call Plugin From Index=0 While ON_RESPONSE
    m_plugin_response_index = 0;

    return true;
}

//diffrent, but necessary!
PluginStatus Client::PluginOnResponse()
{
    Plugin * *plugins = m_server->m_plugins;
    int i;

    for (i = m_plugin_response_index; i < m_plugin_count; ++ i)
    {
        PluginStatus status     = plugins[i]->OnResponse(this, i);
        
        m_plugin_response_index = i; //@2012/10/26

        if (status == NOT_OK)
        {
            return NOT_OK;
        }
        else if (status == ERROR)
        {
            return ERROR;
        }
    }

    return OK;
}

bool Client::PluginAfterResponse()
{
    Plugin * *plugins = m_server->m_plugins;
    int i;

    for (i = 0; i < m_plugin_count; ++ i)
    {
        if (!plugins[i]->AfterResponse(this, i)) {
            return false;
        }
    }

    return true;
}

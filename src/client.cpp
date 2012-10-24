#include "client.h"
#include "server.h"
#include "plugin.h"
#include "event2/event.h"
#include "event2/util.h"

#include <iostream>

Client::Client() 
{ 
    m_event = NULL; 
    m_plugin_data_slots = NULL;
    m_plugin_count = 0;
}

Client::~Client()
{
    if (m_event) 
    {
        event_free(m_event);
        close(m_sockfd);
        Server::ClientMapIter iter = m_server->m_client_map.find(m_sockfd);
        m_server->m_client_map.erase(iter);
        DelPluginDataSlots();
    }
}

bool Client::InitClient(Server *server) 
{
    m_server = server;

    m_intemp.reserve(10 * 1024 * 1024);
    m_inbuf.reserve(10 * 1024 * 1024);
    m_outbuf.reserve(10 * 1024 * 1024);
    
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
                delete client;
                return;
            }
        }
        else if (ret == 0)
        {
            delete client; //client wants to leave, 
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
                delete client;
                return;
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
    if (!client->StatusMachine())
    {
        delete client; //some error happend, kick out the client
    }
}

//core algorithm !!
bool Client::StatusMachine()
{
    std::string::size_type newline;
    PluginStatus status;
    
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
                if  ((newline = m_inbuf.find('\n')) != std::string::npos) //gain a request
                {
                    m_request = m_inbuf.substr(0, newline);
                    m_inbuf.erase(0, newline + 1);
                    SetStatus(AFTER_REQUEST);
                    break;
                }
                else
                {
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
                status = PluginOnResponse();
                if (status == ERROR) {return false;}
                else if (status == NOT_OK) {return true;}
                m_outbuf += m_response;
                SetStatus(AFTER_RESPONSE);
                break;
            case AFTER_RESPONSE:  //transitional status
                if (!PluginAfterResponse()) {
                    return false;
                }
                m_response.clear();
                NotWantWrite();
                SetStatus(BEFORE_REQUEST);
                break;
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

    return true;
}

//diffrent, but necessary!
PluginStatus Client::PluginOnResponse()
{
    Plugin * *plugins = m_server->m_plugins;
    int i;

    for (i = 0; i < m_plugin_count; ++ i)
    {
        PluginStatus status = plugins[i]->OnResponse(this, i);
        
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

#include "listener.h"
#include "server.h"
#include "client.h"

Listener::Listener(const std::string &ip, short port)
{
    m_event          = NULL;
    m_count_client   = 0;

    //ipv4 only
    m_listen_addr.sin_family      = AF_INET;
    m_listen_addr.sin_port        = htons(port);
    m_listen_addr.sin_addr.s_addr = inet_addr(ip.c_str());
}

Listener::~Listener()
{
    if (m_event)
    {
        //first event_free, close socket secondly
        event_free(m_event);
        close(m_sockfd);
    }
}

void Listener::InitListener(Server *server)
{
    m_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    evutil_make_socket_nonblocking(m_sockfd);
    
    int reuse_flag = 1;
    setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse_flag, sizeof(reuse_flag));

    bind(m_sockfd, (struct sockaddr*)&m_listen_addr, sizeof(m_listen_addr));
    listen(m_sockfd, 5);

    m_server = server;
    m_event  = event_new(m_server->m_server_base, m_sockfd, EV_READ | EV_PERSIST, Listener::ListenerEventCallback, this);
    event_add(m_event, NULL);
}

void Listener::ListenerEventCallback(evutil_socket_t sockfd, short event, void *userdata) 
{
    Listener *listener = (Listener*)userdata;
    
    Client   *client   = new Client();

    socklen_t addr_len = sizeof(client->m_client_addr);
    
    client->m_sockfd   = accept(sockfd, (struct sockaddr*)&client->m_client_addr, &addr_len);

    if (client->m_sockfd != -1)
    {
        bool ok = client->InitClient(listener->m_server);
        client->m_server->m_client_map[client->m_sockfd] = client;
        ++ listener->m_count_client;

        if (!ok)
        {
            delete client;
        }
    }
    else
    {
        delete client;

        if (errno != EAGAIN && errno != EINTR)
        {
            //exit event loop next round
            event_base_loopexit(listener->m_server->m_server_base, NULL);  
        }
    }
}


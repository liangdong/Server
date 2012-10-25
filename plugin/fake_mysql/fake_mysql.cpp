#include "plugin.h"
#include "client.h"
#include "server.h"

#include "event2/event.h"
#include "event2/util.h"

#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include <iostream>
#include <string>
#include <queue>

#define PNAME "[PluginMysql] "

enum MysqlStatus
{
    INIT,
    WAIT,
    DONE
};

struct MysqlTask
{
    MysqlStatus  m_status;
    Client      *m_client;
    std::string  m_result;
};

#define TRACE() do { std::cerr << PNAME << __FUNCTION__ << std::endl; } while(0)

#define THREAD_COUNT 5

class PluginMysql: public Plugin
{
    public:
        virtual bool OnLoad(Server *server, int plugin_index)
        {
            TRACE();

            pthread_mutex_init(&m_request_mutex, NULL);
            pthread_cond_init(&m_request_cond, NULL);
            
            pthread_mutex_init(&m_response_mutex, NULL);
            pipe(m_response_pipe);
            evutil_make_socket_nonblocking(m_response_pipe[0]);
            
            m_event = event_new(server->m_server_base, m_response_pipe[0], EV_PERSIST | EV_READ, PluginMysql::MysqlEventCallback, this);
            event_add(m_event, NULL);

            m_quit_thread = false;
            
            m_tids = new pthread_t[THREAD_COUNT];

            int i;
             
            // Mysql thread Pool Created
            for (i = 0; i != THREAD_COUNT; ++ i)
            {
                pthread_create(m_tids + i, NULL, MysqlThread, this);
            }

            return true;
        }
        virtual void OnDestroy(Server *server, int plugin_index)
        {
            TRACE();
            
            pthread_mutex_lock(&m_request_mutex);
            m_quit_thread = true;
            pthread_cond_broadcast(&m_request_cond); // Kill the Thread Pool, So Broadcast:)
            pthread_mutex_unlock(&m_request_mutex);
            
            int i;

            for (i = 0; i != THREAD_COUNT; ++ i)
            {
                pthread_join(m_tids[i], NULL);
            }
            
            delete []m_tids;

            event_free(m_event);
            pthread_mutex_destroy(&m_request_mutex);
            pthread_mutex_destroy(&m_response_mutex);
            pthread_cond_destroy(&m_request_cond);
            close(m_response_pipe[0]);
            close(m_response_pipe[1]);
        }
        virtual bool OnInit(Client *client, int plugin_index)
        {
            TRACE();

            // to store mysql status of this client
            MysqlTask *task= new MysqlTask();
            task->m_client = client;
            client->m_plugin_data_slots[plugin_index] = (void*)task; 
            
            return true;
        }
        virtual void OnClose(Client *client, int plugin_index)
        {
            TRACE();
             
            delete (MysqlTask*)client->m_plugin_data_slots[plugin_index];
        }
        virtual bool BeforeRequest(Client *client, int plugin_index)
        {
            TRACE();

            return true;
        }
        virtual bool AfterRequest(Client *client, int plugin_index)
        {
            TRACE();
            
            // init the Mysql Request Status
            MysqlTask *task = (MysqlTask*)client->m_plugin_data_slots[plugin_index];
            task->m_result.clear();
            task->m_status = INIT;
            
            return true; 
        }
        virtual PluginStatus OnResponse(Client *client, int plugin_index)
        {
            // TRACE();

            // check the Mysql Request Status
            MysqlTask *task = (MysqlTask*)client->m_plugin_data_slots[plugin_index];
            
            if (task->m_status == INIT)
            {
                task->m_status = WAIT;
                pthread_mutex_lock(&m_request_mutex);
                m_request_queue.push(task);
                pthread_cond_signal(&m_request_cond);
                pthread_mutex_unlock(&m_request_mutex);
            }
            else if (task->m_status == DONE)
            {
                return OK;
            }

            return NOT_OK;
        }
        static void * MysqlThread(void *arg)
        {
            TRACE();
            
            PluginMysql *plugin = (PluginMysql*)arg;
            MysqlTask   *task   = NULL;

            while (true)
            {
                pthread_mutex_lock(&plugin->m_request_mutex);

                while (!plugin->m_request_queue.size() && !plugin->m_quit_thread)
                {
                    pthread_cond_wait(&plugin->m_request_cond, &plugin->m_request_mutex);
                }

                if (plugin->m_quit_thread)
                {
                    pthread_mutex_unlock(&plugin->m_request_mutex);
                    return NULL;
                }
                
                task = plugin->m_request_queue.front();
                plugin->m_request_queue.pop();

                pthread_mutex_unlock(&plugin->m_request_mutex);
            
                sleep(5); // fake Mysql operation here

                task->m_result += "[PluginMysql] echo: Mysql_Query(";
                std::string::size_type len = task->m_client->m_request.size();
                // to ignore \r if exist
                if (task->m_client->m_request[len - 1] == '\r') task->m_result += task->m_client->m_request.substr(0, len - 1);
                else task->m_result += task->m_client->m_request;
                task->m_result += ")\n";

                pthread_mutex_lock(&plugin->m_response_mutex);
                plugin->m_response_queue.push(task);
                pthread_mutex_unlock(&plugin->m_response_mutex);
                write(plugin->m_response_pipe[1], "", 1);
            }

            return NULL;
        }
        static void MysqlEventCallback(evutil_socket_t sockfd, short event, void *userdata)
        {
            TRACE();

            PluginMysql *plugin = (PluginMysql*)userdata;

            char ch;
            MysqlTask *task;

            int ret = read(plugin->m_response_pipe[0], &ch, 1);

            if (ret == 1)
            {
                pthread_mutex_lock(&plugin->m_response_mutex);
                task = plugin->m_response_queue.front();
                plugin->m_response_queue.pop();
                pthread_mutex_unlock(&plugin->m_response_mutex);
                
                task->m_client->m_response += task->m_result;
                task->m_status = DONE;
            }
        }

    private:
        struct event          *m_event;

        pthread_t              *m_tids;
        bool                   m_quit_thread;

        pthread_mutex_t        m_request_mutex;
        pthread_cond_t         m_request_cond;
        std::queue<MysqlTask*> m_request_queue;
        
        int                    m_response_pipe[2];
        pthread_mutex_t        m_response_mutex;
        std::queue<MysqlTask*> m_response_queue;
};

extern "C" Plugin* SetupPlugin()
{
    TRACE();
    return new PluginMysql();
}
extern "C" void RemovePlugin(Plugin *plugin)
{
    TRACE();
    delete plugin;
}

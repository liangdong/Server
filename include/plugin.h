#ifndef _PLUGIN_H
#define _PLUGIN_H

class Server;
class Client;

enum PluginStatus
{
    OK,
    NOT_OK,
    ERROR,
};

class Plugin
{
    public:
        typedef Plugin* (*SetupPlugin)();
        typedef void    (*RemovePlugin)(Plugin *plugin);

        virtual bool OnLoad(Server *server, int plugin_index);
        virtual bool OnInit(Client *client, int plugin_index);
        virtual bool BeforeRequest(Client *client, int plugin_index);
        virtual bool OnRequest(Client *client, int plugin_index);
        virtual bool AfterRequest(Client *client, int plugin_index);
        virtual bool BeforeResponse(Client *client, int plugin_index);
        virtual PluginStatus OnResponse(Client *client, int plugin_index);
        virtual bool AfterResponse(Client *client, int plugin_index);
        virtual void OnClose(Client *client, int plugin_index);
        virtual void OnDestroy(Server *server, int plugin_index);
        Plugin();
        virtual ~Plugin(); 
        
        void *m_plugin_data;
        void *m_plugin_so;
        int   m_plugin_index;
        bool  m_is_loaded;

        SetupPlugin  m_setup_plugin;
        RemovePlugin m_remove_plugin;
};

extern const char * plugin_config[]; //add plugin in plugin.cpp

#endif

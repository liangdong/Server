#ifndef _PLUGIN_H
#define _PLUGIN_H

class Server;
class Client;

enum PluginStatus
{
    OK,     //can goon to the next plugin
    NOT_OK, //not ready, stay at the current one.
    ERROR,  //error! just kill the client right now.
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
        
        //m_plugin_data: may be discard in c++, 
        //because object in c++ can hold the data in member(this is only a choice for plugin developer)
        void *m_plugin_data;         
        void *m_plugin_so;   //the handler created by dlopen
        int   m_plugin_index;//the plugin's register index
        bool  m_is_loaded;   //whether the OnLoad has been called

        SetupPlugin  m_setup_plugin;  //got from so to create plugin object
        RemovePlugin m_remove_plugin; //got from so to delete plugin object
};

extern const char * plugin_config[]; //config & add new plugins in plugin.cpp

#endif

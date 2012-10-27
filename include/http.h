#ifndef _HTTP_H
#define _HTTP_H

#include <map>
#include <string>

#include "http_parser.h"

class Client;

typedef std::map<std::string, std::string> HeaderDict;
typedef HeaderDict::iterator HeaderIter;

struct HttpResponse
{
    int         m_code;
    std::string m_explain;
    std::string m_body; //Content-Length shall be set to body's length automatically
    
    HeaderDict  m_headers;

    std::string SerializeResponse();
    void        ResetResponse();
};

struct HttpRequest
{
    std::string m_method;
    std::string m_url;
    
    HeaderDict  m_headers;
    std::string m_new_field;   //field is waiting for value :)
    
    std::string m_body;
};

//NOTICE:you must learn how to use http-parser first, or you may be confused:)
class HttpParser
{
    public:
        void InitParser(Client *client); 
        int  HttpParseRequest(const std::string &inbuf);

        static int OnMessageBegin(http_parser *parser);
        static int OnUrl(http_parser *parser, const char *at, size_t length);
        static int OnHeaderField(http_parser *parser, const char *at, size_t length);
        static int OnHeaderValue(http_parser *parser, const char *at, size_t length);
        static int OnHeadersComplete(http_parser *parser);
        static int OnBody(http_parser *parser, const char *at, size_t length);
        static int OnMessageComplete(http_parser *parser);

    private:
        bool HttpHasError();

        http_parser          m_parser;
        http_parser_settings m_settings;
};

#endif

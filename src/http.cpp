#include "http.h"
#include "client.h"

#include <iostream>
#include <sstream>

#include <string.h>

std::string HttpResponse::SerializeResponse()
{
    std::ostringstream ostream;

    ostream << "Http/1.1 " << m_code << " " << m_explain << "\r\n" 
            << "Server: A Async Server By LiangDong"     << "\r\n"
            << "Connection: keep-alive"                  << "\r\n";

    HeaderIter iter = m_headers.begin();

    while (iter != m_headers.end())
    {
        ostream << iter->first << ": " << iter->second   << "\r\n";
        ++ iter;
    }

    ostream << "Content-Length: " << m_body.size()       << "\r\n\r\n";
    ostream << m_body;

    return ostream.str();
}

void HttpResponse::ResetResponse()
{
    m_code = 200;
    m_explain = "OK";

    m_body.clear();
    m_headers.clear();
}

void HttpParser::InitParser(Client *client)
{
    http_parser_init(&m_parser, HTTP_REQUEST);
    
    m_parser.data = client;

    bzero(&m_settings, sizeof(m_settings));
    
    m_settings.on_message_begin    = OnMessageBegin;
    m_settings.on_url              = OnUrl; 
    m_settings.on_header_field     = OnHeaderField;
    m_settings.on_header_value     = OnHeaderValue;
    m_settings.on_headers_complete = OnHeadersComplete;
    m_settings.on_body             = OnBody;
    m_settings.on_message_complete = OnMessageComplete;
}

int HttpParser::HttpParseRequest(const std::string &inbuf)
{
    int drain = http_parser_execute(&m_parser, &m_settings, inbuf.c_str(), inbuf.size());

    if (HttpHasError())
    {
        return -1;
    }

    return drain;
}

bool HttpParser::HttpHasError()
{
    return m_parser.http_errno != HPE_OK;
}

int HttpParser::OnMessageBegin(http_parser *parser)
{
    Client *client = (Client*)parser->data;
    
    client->m_request_building = new HttpRequest();
    
    return 0;
}

int HttpParser::OnUrl(http_parser *parser, const char *at, size_t length)
{
    Client *client = (Client*)parser->data;
    
    client->m_request_building->m_url.assign(at, length);

    return 0;
}

int HttpParser::OnHeaderField(http_parser *parser, const char *at, size_t length)
{
    Client *client = (Client*)parser->data;
    
    client->m_request_building->m_new_field.assign(at, length);

    return 0;
}

int HttpParser::OnHeaderValue(http_parser *parser, const char *at, size_t length)
{
    Client      *client  = (Client*)parser->data;
    HttpRequest *request = client->m_request_building;
    
    request->m_headers[request->m_new_field] = std::string(at, length);

    return 0;
}

int HttpParser::OnHeadersComplete(http_parser *parser)
{
    // This callback seems to have no meanning for us,
    // just reserved for future need.
    return 0;
}

int HttpParser::OnBody(http_parser *parser, const char *at, size_t length)
{
    Client *client = (Client*)parser->data;
    
    // NOTICE:OnBody may be called many times per Reuqest,
    // So not forget to use append but not assign :)
    client->m_request_building->m_body.append(at, length); 
     
    return 0;
}

int HttpParser::OnMessageComplete(http_parser *parser)
{
    Client      *client  = (Client*)parser->data;
    HttpRequest *request = client->m_request_building;

    request->m_method    = http_method_str((http_method)parser->method);
    
    client->m_request_queue.push(request);
    client->m_request_building = NULL;

    std::cerr << __FUNCTION__ << std::endl;
 
    return 0;
}

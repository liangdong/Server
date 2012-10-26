#include "http.h"

#include <string.h>

HttpParser::HttpParser()
{
    http_parser_init(&m_parser, HTTP_REQUEST);
    
    m_parser.data = this;

    bzero(&m_settings, sizeof(m_settings));
    
    m_settings.on_message_begin    = OnMessageBegin;
    m_settings.on_url              = OnUrl; 
    m_settings.on_header_field     = OnHeaderField;
    m_settings.on_header_value     = OnHeaderValue;
    m_settings.on_headers_complete = OnHeadersComplete;
    m_settings.on_body             = OnBody;
    m_settings.on_message_complete = OnMessageComplete;
}

int HttpParser::HttpParseRequest(const std::string &inbuf, HttpRequest *request)
{
    m_parser.data = request;
    
    int drain = http_parser_execute(&m_parser, &m_settings, inbuf.c_str(), inbuf.size());

    if (HttpHasError())
    {
        return -1;
    }

    return drain;
}
void HttpRequest::HttpResetRequest()
{
    if (m_is_complete)
    {
        m_method.clear();
        m_url.clear();
        m_headers.clear();
        m_new_field.clear();
        m_body.clear();
    }
}

bool HttpParser::HttpHasError()
{
    return m_parser.http_errno != HPE_OK;
}

int HttpParser::OnMessageBegin(http_parser *parser)
{
    HttpRequest *request   = (HttpRequest*)parser->data;
    
    // this line has no real effect,
    // but i just want to keep it to help you understanding.
    request->m_is_complete = false;
    
    return 0;
}

int HttpParser::OnUrl(http_parser *parser, const char *at, size_t length)
{
    HttpRequest *request = (HttpRequest*)parser->data;
    
    request->m_url.assign(at, length);

    return 0;
}

int HttpParser::OnHeaderField(http_parser *parser, const char *at, size_t length)
{
    HttpRequest *request = (HttpRequest*)parser->data;
    
    request->m_new_field.assign(at, length);

    return 0;
}

int HttpParser::OnHeaderValue(http_parser *parser, const char *at, size_t length)
{
    HttpRequest *request = (HttpRequest*)parser->data;
    
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
    HttpRequest *request = (HttpRequest*)parser->data;
    
    // NOTICE:OnBody may be called many times per Reuqest,
    // So not forget to use append but not assign :)
    request->m_body.append(at, length); 
     
    return 0;
}

int HttpParser::OnMessageComplete(http_parser *parser)
{
    HttpRequest *request   = (HttpRequest*)parser->data;
    
    request->m_method      = http_method_str((http_method)parser->method);
    request->m_is_complete = true;

    return 0;
}

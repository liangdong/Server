#include <iostream>
#include <string>

#include <string.h>

#include "http_parser.h"

int HttpOnMessageBeginCallback(http_parser *parser)
{
    std::cerr << __FUNCTION__ << std::endl;
    return 0;
}

int HttpOnMessageEndCallback(http_parser *parser)
{
    std::cerr << __FUNCTION__ << std::endl;
    return 0;
}

int main(int argc, char* const argv[])
{
    std::string request = "GET /index.html HTTP/1.1\r\nHost:localhost\r\nContent-Lenght:5\r\n\r\nhello"
                          "GET /index.html HTTP/1.1\r\nHost:localhost\r\nContent-Lenght:5\r\n\r\nhello";

    http_parser          parser;
    http_parser_settings settings;
    
    bzero(&settings, sizeof(settings));
    settings.on_message_begin = HttpOnMessageBeginCallback;
    settings.on_message_complete = HttpOnMessageEndCallback;

    http_parser_init(&parser, HTTP_REQUEST);
    std::cerr << http_parser_execute(&parser, &settings, request.c_str(), 61) << std::endl;
    std::cerr << http_parser_execute(&parser, &settings, request.c_str(), 61) << std::endl;

    return 0;
}

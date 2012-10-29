plugin_cgi.so:plugin_cgi.o ../../src/plugin.o
	g++ -o $@ plugin_cgi.o ../../src/plugin.o -shared -lpthread -levent

plugin_cgi.o:plugin_cgi.cpp ../../include/plugin.h ../../third/http-parser/http_parser.h
	g++ -fPIC -o $@ -c $< -I../../include -I../../third/http-parser

../../src/plugin.o:../../src/plugin.cpp ../../include/plugin.h
	g++ -fPIC -o $@ -c $< -I../../include

clean:
	rm -rf ../../src/plugin.o *.o *.so

plugin_static.so:plugin_static.o ../../src/plugin.o
	g++ -o $@ plugin_static.o ../../src/plugin.o -shared -lpthread -levent

plugin_static.o:plugin_static.cpp ../../include/plugin.h ../../third/http-parser/http_parser.h
	g++ -fPIC -o $@ -c $< -I../../include -I../../third/http-parser

../../src/plugin.o:../../src/plugin.cpp ../../include/plugin.h
	g++ -fPIC -o $@ -c $< -I../../include

clean:
	rm -rf ../../src/plugin.o *.o *.so

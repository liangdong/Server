THIRD_LIBS=-levent
LIBS=-ldl
CFLAGS=-I./include -I./third/http-parser/

server:src/server.o src/listener.o src/client.o src/main.o src/plugin.o src/http.o third/http-parser/libhttp.a
	g++ -o $@ src/server.o src/listener.o src/client.o src/main.o src/plugin.o src/http.o third/http-parser/libhttp.a $(THIRD_LIBS) $(LIBS)

third/http-parser/libhttp.a:third/http-parser/http_parser.o
	ar -r $@ $<

third/http-parser/http_parser.o:third/http-parser/http_parser.c third/http-parser/http_parser.h 
	g++ -o $@ -fPIC -c $< $(CFLAGS)

src/server.o:src/server.cpp include/server.h
	g++ -o $@ -c $< $(CFLAGS)

src/listener.o:src/listener.cpp include/listener.h include/util.h
	g++ -o $@ -c $< $(CFLAGS)

src/client.o:src/client.cpp include/client.h include/util.h ./include/http.h
	g++ -o $@ -c $< $(CFLAGS)

src/main.o:src/main.cpp include/server.h
	g++ -o $@ -c $< $(CFLAGS)

src/plugin.o:src/plugin.cpp include/plugin.h
	g++ -o $@ -c $< $(CFLAGS)

src/http.o:src/http.cpp include/http.h third/http-parser/http_parser.h
	g++ -o $@ -c $< $(CFLAGS)

clean:
	rm -f src/*.o server

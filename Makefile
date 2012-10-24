server:src/server.o src/listener.o src/client.o src/main.o src/plugin.o
	g++ -o $@ src/server.o src/listener.o src/client.o src/main.o src/plugin.o -levent -ldl

src/server.o:src/server.cpp include/server.h
	g++ -o $@ -c $< -I./include

src/listener.o:src/listener.cpp include/listener.h include/util.h
	g++ -o $@ -c $< -I./include

src/client.o:src/client.cpp include/client.h include/util.h
	g++ -o $@ -c $< -I./include

src/main.o:src/main.cpp include/server.h
	g++ -o $@ -c $< -I./include

src/plugin.o:src/plugin.cpp include/plugin.h
	g++ -o $@ -c $< -I./include
clean:
	rm -f src/*.o server

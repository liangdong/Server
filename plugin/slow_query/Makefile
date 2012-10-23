plugin_slow_query.so:slow_query.o ../../src/plugin.o
	g++ -o $@ slow_query.o ../../src/plugin.o -shared

slow_query.o:slow_query.cpp ../../include/plugin.h
	g++ -fPIC -o $@ -c $< -I../../include

../../src/plugin.o:../../src/plugin.cpp ../../include/plugin.h
	g++ -fPIC -o $@ -c $< -I../../include

clean:
	rm -rf ../../src/plugin.o *.o *.so

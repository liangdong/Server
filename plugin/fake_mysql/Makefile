plugin_fake_mysql.so:fake_mysql.o ../../src/plugin.o
	g++ -o $@ fake_mysql.o ../../src/plugin.o -shared -lpthread -levent

fake_mysql.o:fake_mysql.cpp ../../include/plugin.h
	g++ -fPIC -o $@ -c $< -I../../include

../../src/plugin.o:../../src/plugin.cpp ../../include/plugin.h
	g++ -fPIC -o $@ -c $< -I../../include

clean:
	rm -rf ../../src/plugin.o *.o *.so

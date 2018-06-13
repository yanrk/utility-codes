build   :
	g++ -c -std=c++11 -g -Wall -O1 -pipe -fPIC -I./ -o directory_watcher.o directory_watcher.cpp
	g++ -c -std=c++11 -g -Wall -O1 -pipe -fPIC -I./ -o main.o main.cpp
	g++ -o directory_watcher main.o directory_watcher.o

clean   :
	rm *.a *.o *.so directory_watcher

rebuild : clean build

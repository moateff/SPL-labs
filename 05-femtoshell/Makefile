tests: femtoshell.c tests.cpp
	gcc -c femtoshell.c
	g++ -std=c++14 -o tests tests.cpp -lgtest -lgtest_main -pthread  femtoshell.o -g
clean: 
	rm -rf *.o tests

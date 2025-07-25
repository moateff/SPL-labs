tests: picoshell.c tests.cpp
	gcc -c picoshell.c
	g++ -std=c++14 -o tests tests.cpp -lgtest -lgtest_main -pthread  picoshell.o -g
clean: 
	rm -rf *.o tests

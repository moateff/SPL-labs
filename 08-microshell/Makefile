tests: microshell.c tests.cpp
	gcc -c microshell.c
	g++ -std=c++14 -o tests tests.cpp -lgtest -lgtest_main -pthread  microshell.o -g
clean: 
	rm -rf *.o tests

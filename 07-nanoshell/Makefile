tests: nanoshell.c tests.cpp
	gcc -c nanoshell.c
	g++ -std=c++14 -o tests tests.cpp -lgtest -lgtest_main -pthread  nanoshell.o -g
clean: 
	rm -rf *.o tests

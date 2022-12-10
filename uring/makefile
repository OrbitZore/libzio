main : main.o uringtest.o
	g++ main.o uringtest.o -std=c++20  -fsanitize=address -g -luring -o main

%.o : %.cpp
	g++ -c -std=c++20 $< -o $@ ${FLAGS} -g -fsanitize=address
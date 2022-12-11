main : main.o uringtest.o
	g++ main.o uringtest.o -std=c++20  -fsanitize=address -g -luring -o main
	# g++ main.o uringtest.o -std=c++20 -luring -o main -Ofast

%.o : %.cpp
	g++ -c -std=c++20 $< -o $@ ${FLAGS} -g -fsanitize=address
	# g++ -c -std=c++20 $< -o $@ ${FLAGS} -Ofast

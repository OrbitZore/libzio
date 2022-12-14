all : main liburingtest.so

liburingtest.so : uringtest.cpp
	# g++ -shared uringtest.cpp ${FLAGS} -std=c++20  -fsanitize=address -g -luring -o liburingtest.so -fPIC
	g++ -shared uringtest.cpp ${FLAGS} -std=c++20  -Ofast -luring -o liburingtest.so -fPIC

main : main.o uringtest.o
	# g++ main.o uringtest.o -std=c++20  -fsanitize=address -g -luring -o main -fPIC
	g++ main.o uringtest.o -std=c++20 -luring -o main -Ofast

%.o : %.cpp
	# g++ -c -std=c++20 $< -o $@ ${FLAGS} -g -fsanitize=address -Wall -fPIC
	g++ -c -std=c++20 $< -o $@ ${FLAGS} -Ofast

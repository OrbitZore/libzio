
all : zio zio-uring zio-coroutine uringtest

zio : zio.cpp
	g++ zio.cpp -o zio -std=c++20 -fsanitize=address -g

zio-uring : zio-uring.cpp
	g++ zio-uring.cpp -o zio-uring -std=c++20 -fsanitize=address -g -luring 

zio-coroutine : zio-coroutine.cpp
	g++ zio-coroutine.cpp -o zio-coroutine -std=c++20 -fsanitize=address -g

uringtest : uringtest.cpp
	g++ uringtest.cpp -o uringtest -std=c++20 -fsanitize=address -g -luring
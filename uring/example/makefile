SRC = $(wildcard *.cpp)
EXE = $(addsuffix .out,$(basename $(SRC)))

all : $(EXE)

%.out : %.cpp
	g++ -std=c++20 $< -o $@ ${FLAGS} -g -fsanitize=address -L.. -luringtest -I.. -fPIC -luring

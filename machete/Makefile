cxx?=g++
ar?=ar

source=$(wildcard src/*.cpp)
object=$(patsubst src/%, obj/%, $(source:.cpp=.o))

all: lib/libmachete.a lib/libmachete.so mach

lib/libmachete.a: $(object) 
	$(ar) rcvs $@ $^

lib/libmachete.so: $(object)
	$(cxx) -shared -o $@ $^ 

mach: main.cpp $(object)
	$(cxx) -march=native -O3 -fPIC -DNDEBUG $^ -o $@

obj/%.o: src/%.cpp 
	$(cxx) -c -march=native -O3 -fPIC -DNDEBUG $^ -o $@
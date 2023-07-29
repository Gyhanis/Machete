ifeq ($(MODE), DEBUG)
CFLAG=-g -fsanitize=address
else 
CFLAG=-O3 -DNDEBUG
endif

VLQA_SRC=vlq.cpp vlqa.cpp
VLQA_OBJ=$(patsubst %.cpp, %.o, $(VLQA_SRC))

all: libvlqa.a 

.PHONY: clean

libvlqa.a: $(VLQA_OBJ)
	ar -rcs $@ $^

%.o: %.cpp vlqa.h
	g++ -c $(CFLAG) -o $@ $< -lm -I../../inc 

clean:
	rm -f *.o *.a
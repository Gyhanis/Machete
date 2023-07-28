ifeq ($(MODE), DEBUG)
CFLAG=-g -fsanitize=address
# CFLAG=-g 
else 
CFLAG=-O3 -DNDEBUG
endif

HUFF_SRC=huffman.cpp huffman_comtab.cpp huffman_prune.cpp huffman_prune_ct.cpp
HUFF_OBJ=$(patsubst %.cpp, %.o, $(HUFF_SRC))

.PHONY: libvlqa clean 

all: libhuf.a 

libvlqa: 
	cd ../vlq; make 

libhuf.a: $(HUFF_OBJ)
	ar -rcs $@ $^

%.o: %.cpp huffman.h
	g++ -c $(CFLAG) -o $@ $< -lm -I../../inc 

clean:
	rm -f *.o *.a
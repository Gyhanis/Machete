ifeq ($(MODE), DEBUG)
CFLAG=-g -fsanitize=address
# CFLAG=-g 
else 
CFLAG=-DNDEBUG -O3
endif

MACH_SRC=machete.cpp 
MACH_OBJ=$(patsubst %.cpp, %.o, $(MACH_SRC))

MACHF_SRC=machete.cpp \
	huffman/huffman.cpp huffman/huffman_comtab.cpp huffman/huffman_prune.cpp huffman/huffman_prune_ct.cpp \
	vlq/vlq.cpp vlq/vlqa.cpp 
	
MACHF_OBJ=$(patsubst %.cpp, %.o, $(MACHF_SRC))

all: libmach.a 

.PHONY: libhuf.a libvlqa.a clean

libmach0.a: $(MACH_OBJ)
	ar -rcs $@ $^

libmach.a: $(MACHF_OBJ) 
	ar -rcs $@ $^

libhuf.a: 
	cd huffman; make libhuf.a

libvlqa.a:
	cd vlq; make libvlqa.a

%.o: %.cpp 
	g++ -c $(CFLAG) -o $@ $< -lm -I../inc 

clean: 
	rm -f *.o *.a 
	cd huffman; make clean 
	cd vlq; make clean 
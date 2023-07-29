MODE?=RELEASE

ifeq ($(MODE), DEBUG)
CFLAG=-g -fsanitize=address
else 
CFLAG=-O3 -DNDEBUG
endif
CFLAG += -Iinc -fPIC

BITSTREAM := $(shell ls inc/BitStream/*)

all: libmach compression_test

.PHONY: libmach clean

lib\libmach.a: libmach
libmach:
	cd machete; make
	cp machete/libmach.a lib/libmach.a


wrappers.o: wrappers.cpp 
	g++ -c ${CFLAG} -Iinc/zfp -o $@ $< 

compression_test: compression_test.o wrappers.o lib\libmach.a
	g++ -o $@ compression_test.o wrappers.o $(CFLAG) \
		-Llib -lmach -lzfp -lzstd -lz -fopenmp

clean:
	rm -f *.o tmp.cmp compression_test 
	rm -f lib/libmach.a 
	cd machete; make clean 

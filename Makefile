MODE?=RELEASE

ifeq ($(MODE), DEBUG)
CFLAG=-g -fsanitize=address
else 
CFLAG=-O3 -DNDEBUG
endif
CFLAG += -Iinc -fPIC

BITSTREAM := $(shell ls inc/BitStream/*)

LIBS=lib/libmach.a lib/liblfzip.a lib/libgorilla.a lib/libchimp.a

all: libmach compression_test

.PHONY: libmach clean

lib/libmach.a: libmach
libmach:
	cd machete; make
	cp machete/libmach.a lib/libmach.a

lib/liblfzip.a: LFZip/lfzip.cpp LFZip/lfzip_predictor.cpp
	g++ -c -o lfzip.o $< $(CFLAG) -Llib -lbsc -Iinc -fopenmp 
	ar -rcs $@ lfzip.o 
	rm lfzip.o

lib/libgorilla.a: Gorilla/gorilla.cpp $(BITSTREAM)
	g++ -c -o gorilla.o $< $(CFLAG) 
	ar -rcs lib/libgorilla.a gorilla.o 
	rm gorilla.o

lib/libchimp.a: $(CHIMP_SRC)
	g++ -c -o encode.o Chimp/ChimpEncode.cpp $(CFLAG) 
	g++ -c -o decode.o Chimp/ChimpDecode.cpp $(CFLAG) 
	ar -rcs $@ encode.o decode.o 
	rm encode.o decode.o

wrappers.o: wrappers.cpp 
	g++ -c ${CFLAG} -Iinc/zfp -o $@ $< 

compression_test.o: compression_test.cpp 
	g++ -c ${CFLAG} -o $@ $<

compression_test: compression_test.o wrappers.o $(LIBS) 
	g++ -o $@ compression_test.o wrappers.o $(CFLAG) \
		-Llib -lmach -lgorilla -lchimp -llfzip -lbsc \
		-lzfp -lzstd -lz -fopenmp

clean:
	rm -f *.o tmp.cmp compression_test 
	rm -f $(LIBS)
	cd machete; make clean 

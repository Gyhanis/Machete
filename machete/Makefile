ifeq ($(MODE), DEBUG)
CFLAG ?=-g -fsanitize=address
else 
CFLAG ?=-O3 -DNDEBUG
endif

.PHONY: clean

libmach.a: huffman.o ovlq.o hybrid.o predict.o machete.o
	ar -rcs $@ $^

test: test.o huffman.o ovlq.o hybrid.o predict.o machete.o
	$(CXX) $(CFLAG) $^ -o $@

%.o: %.cpp defs.h
	$(CXX) -c $(CFLAG) $< -o $@ -I../inc

clean:
	rm -f *.o libmach.a test
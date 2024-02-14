ifeq ($(MODE), DEBUG)
CFLAG += -g -fsanitize=address
else 
CFLAG += -O3 -DNDEBUG
endif

.PHONY: clean

compression_test: lib/libmach.a lib/libgorilla.a lib/libchimp.a lib/libelf.a lib/liblfzip.a
compression_test: compression_test.o wrapper.o 
	$(CXX) $(CFLAG) $^ -Llib -lmach -lgorilla -lchimp -lelf -llfzip -lSZ3c -lbsc -fopenmp -lzstd -lz -o $@

lib/libmach.a:  
	$(MAKE) -C machete/ CFLAG="$(CFLAG)"
	cp machete/libmach.a lib 

lib/lib%.a:
	$(MAKE) -C $* CFLAG="$(CFLAG)"
	cp $*/lib$*.a lib

%.o: %.cpp
	$(CXX) -c $(CFLAG) $< -o $@ -Iinc

clean:
	cd machete && make clean 
	cd lfzip && make clean
	cd gorilla && make clean 
	cd chimp && make clean 
	cd elf && make clean
	rm tmp* compression_test compression_test.o
	rm lib/libmach.a lib/libgorilla.a lib/libchimp.a lib/libelf.a lib/liblfzip.a

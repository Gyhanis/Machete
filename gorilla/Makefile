.PHONY: clean

LIB=libgorilla.a

SRC=$(wildcard *.cpp)
OBJ=$(patsubst %.cpp,%.o,$(SRC))
HDR=$(wildcard *.h)

$(LIB): $(OBJ)
	ar -rcs $@ $^

%.o: %.cpp $(HDR)
	$(CXX) -c $(CFLAG) $< -o $@ -I../inc

clean:
	rm -f *.o $(LIB)
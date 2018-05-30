CFLAGS = -Wall -g
CXXFLAGS = -Wall -std=c++11 -g -fopenmp -D_GLIBCXX_PARALLEL

.PHONY: all clean

all: sort preload.so

sort: sort.o
	$(CXX) $(CXXFLAGS) $< -o $@ -lrt

preload.so: preload.o
	$(CC) -shared $< -o $@ -lrt -ldl

.cc.o: %.cc
	$(CXX) -c $(CXXFLAGS) $< -o $@

.c.o: %.c
	$(CC) -fPIC -c $(CFLAGS) $< -o $@

clean:
	-rm -f *.o
	-rm -f $(TARGET)

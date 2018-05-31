CXXFLAGS = -Wall -std=c++11 -g -fopenmp -D_GLIBCXX_PARALLEL -O3

.PHONY: all clean

all: sort

sort: sort.o
	$(CXX) $(CXXFLAGS) $< -o $@ -lrt

.cc.o: %.cc
	$(CXX) -c $(CXXFLAGS) $< -o $@


clean:
	-rm -f *.o
	-rm -f sort

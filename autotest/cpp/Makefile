CXX = g++
CXXFLAGS = -g -Wall -Wno-long-long -pedantic
LDFLAGS = `gdal-config --libs`
CFLAGS = `gdal-config --cflags` -I. -Itut $(CPPFLAGS)

PROGS = gdal_unit_test

all: $(PROGS)

test_cpl.o: test_cpl.cpp
	$(CXX) $(CFLAGS) -c test_cpl.cpp 

gdal_unit_test: gdal_unit_test.cpp test_cpl.o
	$(CXX) $(CFLAGS) gdal_unit_test.cpp test_cpl.o $(LDFLAGS) -o gdal_unit_test

clean:
	$(RM) $(PROGS)
	$(RM) *.o
	$(RM) *.a
	$(RM) *.out
	$(RM) *~


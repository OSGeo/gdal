CXX = g++
CXXFLAGS = -g -Wall -Wno-long-long -pedantic
LDFLAGS = `gdal-config --libs`
CFLAGS = `gdal-config --cflags` -I. -Itut $(CPPFLAGS)

PROGS = gdal_unit_test testperfcopywords testcopywords

all: $(PROGS)

test_cpl.o: test_cpl.cpp
	$(CXX) $(CFLAGS) -c $<

gdal_unit_test: gdal_unit_test.cpp test_cpl.o
	$(CXX) $(CFLAGS) $< $(LDFLAGS) -o $@

testperfcopywords: testperfcopywords.cpp
	$(CXX) $(CFLAGS) $< $(LDFLAGS) -o $@
	
testcopywords: testcopywords.cpp
	$(CXX) -O2 $(CFLAGS) $< $(LDFLAGS) -o $@

clean:
	$(RM) $(PROGS)
	$(RM) *.o
	$(RM) *.a
	$(RM) *.out
	$(RM) *~


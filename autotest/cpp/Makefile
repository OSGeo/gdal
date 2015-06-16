CXX = g++
CXXFLAGS =`gdal-config --cflags` -Wall -I. -Itut $(CPPFLAGS)
LDFLAGS = `gdal-config --libs`

PROGS = gdal_unit_test testperfcopywords testcopywords testclosedondestroydm testthreadcond test_virtualmem testblockcache testblockcachewrite

all: $(PROGS)

test:
	make quick_test
	./testperfcopywords

quick_test:
	./gdal_unit_test
	./testcopywords
	./testclosedondestroydm
	./testthreadcond
	./test_virtualmem
	./testblockcache -check -co TILED=YES --debug TEST,LOCK -loops 3 --config GDAL_RB_LOCK_DEBUG_CONTENTION YES
	./testblockcache -check -co TILED=YES --debug TEST,LOCK -loops 3 --config GDAL_RB_LOCK_DEBUG_CONTENTION YES --config GDAL_RB_LOCK_TYPE SPIN
	./testblockcache -check -co TILED=YES -migrate
	./testblockcache -check -memdriver
	./testblockcachewrite --debug ON

OBJ = \
    gdal_unit_test.o \
    test_cpl.o \
    test_gdal_aaigrid.o \
    test_gdal_dted.o \
    test_gdal_gtiff.o \
    test_triangulation.o \
    test_ogr.o \
    test_ogr_geos.o \
    test_ogr_shape.o \
    test_osr.o \
    test_osr_ct.o \
    test_osr_pci.o \
    test_osr_proj4.o \
    tut/tut_gdal.o

gdal_unit_test: $(OBJ)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

testperfcopywords: testperfcopywords.cpp
	$(CXX) -O2 $(CXXFLAGS) $< $(LDFLAGS) -o $@
	
testcopywords: testcopywords.cpp
	$(CXX) -O2 $(CXXFLAGS) $< $(LDFLAGS) -o $@

testclosedondestroydm: testclosedondestroydm.c
	$(CXX) -g $(CXXFLAGS) $< $(LDFLAGS) -o $@

testthreadcond: testthreadcond.c
	$(CXX) -g $(CXXFLAGS) $< $(LDFLAGS) -o $@

test_virtualmem: test_virtualmem.cpp
	$(CXX) -g $(CXXFLAGS) $< $(LDFLAGS) -o $@

testblockcache: testblockcache.cpp
	$(CXX) -g -O2 $(CXXFLAGS) $< $(LDFLAGS) -o $@

testblockcachewrite: testblockcachewrite.cpp
	$(CXX) -g -O2 $(CXXFLAGS) $< $(LDFLAGS) -o $@

vsipreload.so: ../../gdal/port/vsipreload.cpp
	$(CXX) -fPIC -g $(CXXFLAGS) $< $(LDFLAGS) -shared -o $@

clean:
	$(RM) $(PROGS)
	$(RM) *.o
	$(RM) *.a
	$(RM) *.out
	$(RM) *~


# This Makefile requires GNU Make.

include ../../gdal/GDALmake.opt
CXXFLAGS += -I../../gdal/port -I../../gdal/alg -I../../gdal/gcore -I../../gdal/ogr -I../../gdal/ogr/ogrsf_frmts -I../../gdal/apps -I../../gdal/frmts/vrt -DGDAL_COMPILATION
CXXFLAGS += -I. -Itut

CFLAGS += -I../../gdal/port -I../../gdal/alg -I../../gdal/gcore -I../../gdal/ogr -I../../gdal/ogr/ogrsf_frmts -I../../gdal/apps -I../../gdal/frmts/vrt -DGDAL_COMPILATION
CFLAGS += -I. -Itut

PROGS = gdal_unit_test testperfcopywords testcopywords testclosedondestroydm testthreadcond testvirtualmem testblockcache testblockcachewrite testblockcachelimits testdestroy testmultithreadedwriting test_include_from_c_file test_c_include_from_cpp_file

all: $(PROGS)

test:
	make quick_test
	./testperfcopywords

quick_test:
	./gdal_unit_test
	./testcopywords
	./testclosedondestroydm
	./testthreadcond
	./testvirtualmem
	./testblockcache -check -co TILED=YES --debug TEST,LOCK -loops 3 --config GDAL_RB_LOCK_DEBUG_CONTENTION YES
	./testblockcache -check -co TILED=YES --debug TEST,LOCK -loops 3 --config GDAL_RB_LOCK_DEBUG_CONTENTION YES --config GDAL_RB_LOCK_TYPE SPIN
	./testblockcache -check -co TILED=YES -migrate
	./testblockcache -check -memdriver
	./testblockcachewrite --debug ON
	./testblockcache --config GDAL_BAND_BLOCK_CACHE HASHSET -check -co TILED=YES --debug TEST,LOCK -loops 3 --config GDAL_RB_LOCK_DEBUG_CONTENTION YES
	./testblockcache --config GDAL_BAND_BLOCK_CACHE HASHSET -check -co TILED=YES --debug TEST,LOCK -loops 3 --config GDAL_RB_LOCK_DEBUG_CONTENTION YES --config GDAL_RB_LOCK_TYPE SPIN
	./testblockcachelimits --debug ON
	./testmultithreadedwriting
	./testdestroy

test_sse:
	$(CXX) -g -O2  testsse.cpp -o testsse -I../../gdal/port -I../../gdal/gcore
	./testsse
	$(CXX) -g -O2 -mssse3 testsse.cpp -o testsse -I../../gdal/port -I../../gdal/gcore
	./testsse
	$(CXX) -g -O2 -mssse3 -msse4.1 testsse.cpp -o testsse -I../../gdal/port -I../../gdal/gcore
	./testsse
	$(CXX) -g -O2 -mavx2 testsse.cpp -o testsse -I../../gdal/port -I../../gdal/gcore
	./testsse
	$(CXX) -g -O2 -DUSE_SSE2_EMULATION testsse.cpp -o testsse -I../../gdal/port -I../../gdal/gcore
	./testsse

OBJ = \
    gdal_unit_test.o \
    test_alg.o \
    test_cpl.o \
    test_gdal.o \
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
	$(LD) $(LDFLAGS) $^ $(CONFIG_LIBS) -o $@

testperfcopywords.o: testperfcopywords.cpp
	$(CXX) $(CXXFLAGS) -O2 -c $<

testperfcopywords: testperfcopywords.o
	$(LD) $(LDFLAGS) $< $(CONFIG_LIBS) -o $@

testcopywords.o: testcopywords.cpp
	$(CXX) $(CXXFLAGS) -O2 -c $<

testcopywords: testcopywords.o
	$(LD) $(LDFLAGS) $< $(CONFIG_LIBS) -o $@

testclosedondestroydm: testclosedondestroydm.o
	$(LD) $(LDFLAGS) $< $(CONFIG_LIBS) -o $@

testthreadcond: testthreadcond.o
	$(LD) $(LDFLAGS) $< $(CONFIG_LIBS) -o $@

testvirtualmem: testvirtualmem.o
	$(LD) $(LDFLAGS) $< $(CONFIG_LIBS) -o $@

testblockcache.o: testblockcache.cpp
	$(CXX) $(CXXFLAGS) -O2 -c $<

testblockcache: testblockcache.o
	$(LD) $(LDFLAGS) $< $(CONFIG_LIBS) -o $@

testblockcachewrite.o: testblockcachewrite.cpp
	$(CXX) $(CXXFLAGS) -O2 -c $<

testblockcachewrite: testblockcachewrite.o
	$(LD) $(LDFLAGS) $< $(CONFIG_LIBS) -o $@

testmultithreadedwriting.o: testmultithreadedwriting.cpp
	$(CXX) $(CXXFLAGS) -O2 -c $<

testmultithreadedwriting: testmultithreadedwriting.o
	$(LD) $(LDFLAGS) $< $(CONFIG_LIBS) -o $@

testblockcachelimits.o: testblockcachelimits.cpp
	$(CXX) $(CXXFLAGS) -O2 -c $<

testblockcachelimits: testblockcachelimits.o
	$(LD) $(LDFLAGS) $< $(CONFIG_LIBS) -o $@

testdestroy: testdestroy.o
	$(LD) $(LDFLAGS) $< $(CONFIG_LIBS) -o $@

vsipreload.so: ../../gdal/port/vsipreload.o
	$(CXX) -fPIC -g $(CXXFLAGS) $< $(CONFIG_LIBS) $(LDFLAGS) -shared -o $@

test_include_from_c_file.o: test_include_from_c_file.c
	$(CC) $(CFLAGS) -O2 -c $<

test_include_from_c_file: test_include_from_c_file.o
	$(LD) $(LDFLAGS) $< $(CONFIG_LIBS) -o $@

test_c_include_from_cpp_file: test_c_include_from_cpp_file.o
	$(LD) $(LDFLAGS) $< $(CONFIG_LIBS) -o $@

clean:
	$(RM) $(PROGS) testsse
	$(RM) $(OBJ)
	$(RM) *.a
	$(RM) *.out
	$(RM) *~


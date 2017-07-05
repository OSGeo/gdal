# This Makefile requires GNU Make.

CXX = g++
#CXXFLAGS := $(shell gdal-config --cflags)
CXXFLAGS = -I../../gdal/port -I../../gdal/alg -I../../gdal/gcore -I../../gdal/ogr -I../../gdal/ogr/ogrsf_frmts -I../../gdal/apps -I../../gdal/frmts/vrt -DGDAL_COMPILATION
CXXFLAGS += -I. -Itut $(CPPFLAGS)
CXXFLAGS += -Wall
CXXFLAGS += -Wextra -Wno-unused-function
# CXXFLAGS += -Werror
#CXXFLAGS += -fsanitize=undefined
#LDFLAGS := -fsanitize=undefined

ifdef HARDEN
CXXFLAGS += -O2 -g
CXXFLAGS += -D_FORTIFY_SOURCE=2
CXXFLAGS += -fstack-protector-all
CXXFLAGS += -Wformat -Werror=format-security
CXXFLAGS += -ftrapv -fwrapv
# Clashes with current build strategy.
# CXXFLAGS += -fPIE -pie
endif

LDFLAGS = $(shell gdal-config --libs)

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
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

testperfcopywords: testperfcopywords.cpp
	$(CXX) -O2 $(CXXFLAGS) $< $(LDFLAGS) -o $@

testcopywords: testcopywords.cpp
	$(CXX) -O2 $(CXXFLAGS) $< $(LDFLAGS) -o $@

testclosedondestroydm: testclosedondestroydm.cpp
	$(CXX) -g $(CXXFLAGS) $< $(LDFLAGS) -o $@

testthreadcond: testthreadcond.cpp
	$(CXX) -g $(CXXFLAGS) $< $(LDFLAGS) -o $@

testvirtualmem: testvirtualmem.cpp
	$(CXX) -g $(CXXFLAGS) $< $(LDFLAGS) -o $@

testblockcache: testblockcache.cpp
	$(CXX) -g -O2 $(CXXFLAGS) $< $(LDFLAGS) -o $@

testblockcachewrite: testblockcachewrite.cpp
	$(CXX) -g -O2 $(CXXFLAGS) $< $(LDFLAGS) -o $@

testmultithreadedwriting: testmultithreadedwriting.cpp
	$(CXX) -g -O2 $(CXXFLAGS) $< $(LDFLAGS) -o $@

testblockcachelimits: testblockcachelimits.cpp
	$(CXX) -g -O2 $(CXXFLAGS) $< $(LDFLAGS) -o $@

testdestroy: testdestroy.cpp
	$(CXX) -g -O2 $(CXXFLAGS) $< $(LDFLAGS) -o $@

vsipreload.so: ../../gdal/port/vsipreload.cpp
	$(CXX) -fPIC -g $(CXXFLAGS) $< $(LDFLAGS) -shared -o $@

test_include_from_c_file: test_include_from_c_file.c
	$(CC) -g $(CXXFLAGS) $< $(LDFLAGS) -o $@

test_c_include_from_cpp_file: test_c_include_from_cpp_file.cpp
	$(CXX) -g $(CXXFLAGS) $< $(LDFLAGS) -o $@

clean:
	$(RM) $(PROGS)
	$(RM) $(OBJ)
	$(RM) *.a
	$(RM) *.out
	$(RM) *~


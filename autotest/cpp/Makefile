# This Makefile requires GNU Make.

include ../../GDALmake.opt
CXXFLAGS += -I. -Itut $(GDAL_INCLUDE)
CPPFLAGS:=$(filter-out -DGDAL_COMPILATION,$(CPPFLAGS))

CFLAGS += -I. -Itut $(GDAL_INCLUDE)

PROGS = gdal_unit_test testperfcopywords testcopywords testclosedondestroydm testthreadcond testvirtualmem testblockcache testblockcachewrite testblockcachelimits testdestroy testmultithreadedwriting test_include_from_c_file test_include_from_cpp_file test_include_from_cpp_file_with_extern_c test_osr_set_proj_search_paths bug1488 proj_with_fork testlog

all: $(PROGS)

test check: all
	make quick_test
	./testperfcopywords

quick_test: gdal_unit_test testcopywords testclosedondestroydm testthreadcond testvirtualmem testblockcache testblockcachewrite testblockcachelimits testmultithreadedwriting testdestroy test_osr_set_proj_search_paths bug1488 proj_with_fork
	./gdal_unit_test
	./testcopywords
	./testclosedondestroydm
	./testthreadcond
	./testvirtualmem
	./testblockcache -check -co TILED=YES --debug TEST,LOCK -loops 3 --config GDAL_RB_LOCK_DEBUG_CONTENTION YES --config GDAL_CACHEMAX 100
	./testblockcache -check -co TILED=YES --debug TEST,LOCK -loops 3 --config GDAL_RB_LOCK_DEBUG_CONTENTION YES --config GDAL_RB_LOCK_TYPE SPIN  --config GDAL_CACHEMAX 100
	./testblockcache -check -co TILED=YES -migrate --config GDAL_CACHEMAX 100
	./testblockcache -check -memdriver --config GDAL_CACHEMAX 100
	./testblockcachewrite --debug ON
	./testblockcache --config GDAL_BAND_BLOCK_CACHE HASHSET -check -co TILED=YES --debug TEST,LOCK -loops 3 --config GDAL_RB_LOCK_DEBUG_CONTENTION YES  --config GDAL_CACHEMAX 100
	./testblockcache --config GDAL_BAND_BLOCK_CACHE HASHSET -check -co TILED=YES --debug TEST,LOCK,GDAL -loops 3 --config GDAL_RB_LOCK_DEBUG_CONTENTION YES -threads 2 --config GDAL_CACHEMAX 100
	./testblockcache --config GDAL_BAND_BLOCK_CACHE HASHSET -check -co TILED=YES --debug TEST,LOCK -loops 3 --config GDAL_RB_LOCK_DEBUG_CONTENTION YES --config GDAL_RB_LOCK_TYPE SPIN --config GDAL_CACHEMAX 100
	./testblockcachelimits --debug ON
	./testmultithreadedwriting
	./testdestroy
	./test_osr_set_proj_search_paths
	./bug1488
	./proj_with_fork
	./testlog

testsse2:
	$(CXX) -g -O2  testsse.cpp -o testsse -I../../port -I../../gcore
	./testsse
	$(CXX) -g -O2 -DUSE_SSE2_EMULATION -DNO_WARN_USE_SSE2_EMULATION testsse.cpp -o testsse -I../../port -I../../gcore
	./testsse

testavx2: testsse2
	$(CXX) -g -O2 -mssse3 testsse.cpp -o testsse -I../../port -I../../gcore
	./testsse
	$(CXX) -g -O2 -mssse3 -msse4.1 testsse.cpp -o testsse -I../../port -I../../gcore
	./testsse
	$(CXX) -g -O2 -mavx2 testsse.cpp -o testsse -I../../port -I../../gcore
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
    test_marching_squares_square.o \
    test_marching_squares_tile.o \
    test_marching_squares_contour.o \
    test_marching_squares_polygon.o \
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

test_osr_set_proj_search_paths.o: test_osr_set_proj_search_paths.cpp
	$(CXX) $(CXXFLAGS) $(PROJ_INCLUDE) $(PROJ_CFLAGS) -c $<

test_osr_set_proj_search_paths: test_osr_set_proj_search_paths.o
	$(LD) $(LDFLAGS) $< $(CONFIG_LIBS) -o $@

bug1488: bug1488.o
	$(LD) $(LDFLAGS) $< $(CONFIG_LIBS) -o $@

proj_with_fork: proj_with_fork.o
	$(LD) $(LDFLAGS) $< $(CONFIG_LIBS) -o $@

testlog: testlog.o
	$(LD) $(LDFLAGS) $< $(CONFIG_LIBS) -o $@

vsipreload.so: ../../port/vsipreload.o
	$(CXX) -fPIC -g $(CXXFLAGS) $< $(CONFIG_LIBS) $(LDFLAGS) -shared -o $@

test_include_from_c_file:
	for HEADER_FILE in cpl_atomic_ops.h cpl_conv.h cpl_csv.h cpl_error.h cpl_hash_set.h cpl_list.h cpl_minixml.h cpl_port.h cpl_progress.h cpl_quad_tree.h cpl_vsi.h gdal_alg.h gdal_version.h gdal.h gdal_utils.h ogr_api.h ogr_core.h ogr_srs_api.h; do \
		echo "#include \"$$HEADER_FILE\"" > tmp_include_from_c_file.c; \
		$(CC) $(CFLAGS) -c tmp_include_from_c_file.c; \
		rm -f tmp_include_from_c_file.*; \
		rm -f .libs/tmp_include_from_c_file.*; \
	done; \
	$(CC) $(CFLAGS) -c test_include_from_c_file.c

test_include_from_cpp_file:
	for HEADER_FILE in cpl_atomic_ops.h cpl_conv.h cpl_csv.h cpl_error.h cpl_hash_set.h cpl_list.h cpl_minixml.h cpl_port.h cpl_progress.h cpl_quad_tree.h cpl_vsi.h gdal_alg.h gdal_version.h gdal.h gdal_utils.h ogr_api.h ogr_core.h ogr_srs_api.h ogr_feature.h ogr_geometry.h ogrsf_frmts.h ogr_spatialref.h; do \
		echo "#include \"$$HEADER_FILE\"" > tmp_include_from_cpp_file.cpp; \
		$(CXX) $(CXXFLAGS) -c tmp_include_from_cpp_file.cpp; \
		rm -f tmp_include_from_cpp_file.*; \
		rm -f .libs/tmp_include_from_cpp_file.*; \
	done; \
	$(CXX) $(CXXFLAGS) -c test_c_include_from_cpp_file.cpp

test_include_from_cpp_file_with_extern_c:
	for HEADER_FILE in cpl_atomic_ops.h cpl_conv.h cpl_csv.h cpl_error.h cpl_hash_set.h cpl_list.h cpl_minixml.h cpl_port.h cpl_progress.h cpl_quad_tree.h cpl_vsi.h gdal_alg.h gdal_version.h gdal.h gdal_utils.h ogr_api.h ogr_core.h ogr_srs_api.h; do \
		echo "extern \"C\" {" > test_include_from_cpp_file_with_extern_c.cpp; \
		echo "#include \"$$HEADER_FILE\"" >> test_include_from_cpp_file_with_extern_c.cpp; \
		echo "}" >> test_include_from_cpp_file_with_extern_c.cpp; \
		$(CXX) $(CXXFLAGS) -c test_include_from_cpp_file_with_extern_c.cpp; \
		rm -f test_include_from_cpp_file_with_extern_c.*; \
		rm -f .libs/test_include_from_cpp_file_with_extern_c.*; \
	done

clean:
	$(RM) $(PROGS) testsse
	$(RM) $(OBJ)
	$(RM) *.o
	$(RM) *.a
	$(RM) *.out
	$(RM) *~


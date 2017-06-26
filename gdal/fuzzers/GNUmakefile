
include ../GDALmake.opt

OBJ	=	gdal_fuzzer.o \
		ogr_fuzzer.o \
		wkb_import_fuzzer.o \
		wkt_import_fuzzer.o \
		osr_set_from_user_input_fuzzer.o \
		spatialite_geom_import_fuzzer.o \
		gml_geom_import_fuzzer.o \
		get_jpeg2000_structure_fuzzer.o \
		gdal_translate_fuzzer.o \
		fuzzingengine.o

CPPFLAGS	:=	$(CPPFLAGS) -I../ogr/ogrsf_frmts/sqlite

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	$(RM) *.o $(O_OBJ) libFuzzingEngine.a

dummyfuzzers: fuzzingengine.o
	$(AR) r libFuzzingEngine.a fuzzingengine.o
	CXX="${CXX}" CXXFLAGS="${CXXFLAGS} -L." EXTRA_LIBS="${LIBS}" SRC=/tmp OUT=/tmp ./build_google_oss_fuzzers.sh
	OUT=/tmp ./build_seed_corpus.sh

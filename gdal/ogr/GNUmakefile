
include ../GDALmake.opt

OBJ	=	gdalmediancut.o gdaldither.o gdal_crs.o gdaltransformer.o \
		gdalsimplewarp.o gdalwarper.o gdalwarpkernel.o \
		gdalwarpoperation.o gdalchecksum.o gdal_rpc.o gdal_tps.o \
		thinplatespline.o llrasterize.o gdalrasterize.o gdalgeoloc.o \
		gdalgrid.o gdalcutline.o gdalproximity.o rasterfill.o \
		gdalrasterpolygonenumerator.o \
		gdalsievefilter.o gdalwarpkernel_opencl.o polygonize.o \
		contour.o gdaltransformgeolocs.o \
		gdal_octave.o gdal_simplesurf.o gdalmatching.o delaunay.o \
		gdalpansharpen.o

ifeq ($(HAVE_AVX_AT_COMPILE_TIME),yes)
CPPFLAGS 	:=	-DHAVE_AVX_AT_COMPILE_TIME $(CPPFLAGS)
endif

ifeq ($(HAVE_SSE_AT_COMPILE_TIME),yes)
CPPFLAGS 	:=	-DHAVE_SSE_AT_COMPILE_TIME $(CPPFLAGS)
endif

ifeq ($(HAVE_GEOS),yes)
CPPFLAGS 	:=	-DHAVE_GEOS=1 $(GEOS_CFLAGS) $(CPPFLAGS)
endif

ifeq ($(HAVE_SFCGAL),yes)
CPPFLAGS 	:=	-DHAVE_SFCGAL=1 $(SFCGAL_CFLAGS) $(CPPFLAGS)
endif

ifeq ($(HAVE_ARMADILLO),yes)
CPPFLAGS 	:=	-DHAVE_ARMADILLO $(CPPFLAGS)
endif

ifeq ($(QHULL_SETTING),external)
CPPFLAGS 	:=	-DEXTERNAL_QHULL $(CPPFLAGS)
endif
ifeq ($(QHULL_SETTING),internal)
CPPFLAGS 	:=	-DINTERNAL_QHULL -Ilibqhull $(CPPFLAGS)
endif

CPPFLAGS	:=	$(CPPFLAGS) $(OPENCL_FLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT)) gdalgridavx.$(OBJ_EXT) gdalgridsse.$(OBJ_EXT)

# We use CXXFLAGS_NO_LTO_IF_AVX_NONDEFAULT to avoid the whole library to be compiled with -mavx
# if -mavx is not the default
gdalgridavx.$(OBJ_EXT):   gdalgridavx.cpp
	$(CXX) $(GDAL_INCLUDE) $(CXXFLAGS_NO_LTO_IF_AVX_NONDEFAULT) $(AVXFLAGS) $(CPPFLAGS) -c -o $@ $<

gdalgridsse.$(OBJ_EXT):   gdalgridsse.cpp
	$(CXX) $(GDAL_INCLUDE) $(CXXFLAGS) $(SSEFLAGS) $(CPPFLAGS) -c -o $@ $<

clean:
	$(RM) *.o $(O_OBJ)

docs:
	(cd ..; $(MAKE) docs)

install:
	for f in $(INST_H_FILES) ; \
	    do $(INSTALL_DATA) $$f $(DESTDIR)$(INST_INCLUDE) ; \
	done

# The sed substitution below workarounds a bug with gcc 4.1 -O2 (checked on 64bit platforms)
# that produces buggy compiled code.
# Seen on gcc 4.1.2-27ubuntu1 (Ubuntu 10.04) (not the default compiler) and gcc-4.1.2-48.el5 (CentOS 5.5)
# (default compiler...)
# The memset isn't necessary at all with a non-buggy compiler, but I've found
# that it helps gcc 4.1 generating correct code here...
parser:
	bison -p swq -d -oswq_parser.cpp swq_parser.y
	sed "s/yytype_int16 yyssa\[YYINITDEPTH\];/yytype_int16 yyssa[YYINITDEPTH]; \/\* workaround bug with gcc 4.1 -O2 \*\/ memset(yyssa, 0, sizeof(yyssa));/" < swq_parser.cpp | sed "s/\*yyssp = yystate/\*yyssp = (yytype_int16)yystate/" | sed "s/            \/\* Fall through\.  \*\//            CPL_FALLTHROUGH/" > swq_parser.cpp.tmp
	mv swq_parser.cpp.tmp swq_parser.cpp

# The sed hack is to please MSVC
osr_cs_wkt_parser:
	bison --no-lines -p osr_cs_wkt_ -d -oosr_cs_wkt_parser.c osr_cs_wkt_grammar.y
	sed "s/\*yyssp = yystate/\*yyssp = (yytype_int16)yystate/" < osr_cs_wkt_parser.c | sed "s/yyerrorlab:/#if 0\nyyerrorlab:/" | sed "s/yyerrlab1:/#endif\nyyerrlab1:/" | sed "s/for (yylen = 0; yystr\[yylen\]; yylen++)/for (yylen = 0; yystr != YY_NULL \&\& yystr\[yylen\]; yylen++)/"  > osr_cs_wkt_parser.c.tmp
	mv osr_cs_wkt_parser.c.tmp osr_cs_wkt_parser.c

osr_cs_wkt_parser.$(OBJ_EXT):   osr_cs_wkt_parser.c
	$(CC) $(GDAL_INCLUDE) $(CFLAGS) $(CPPFLAGS) $(NO_LOGICAL_OP_FLAG) -c -o $@ $<

swq_parser.$(OBJ_EXT):   swq_parser.cpp
	$(CXX) $(GDAL_INCLUDE) $(CXXFLAGS) $(CPPFLAGS) $(NO_LOGICAL_OP_FLAG) -c -o $@ $<
	for f in *.h ; do $(INSTALL_DATA) $$f $(DESTDIR)$(INST_INCLUDE) ; done

lib:	$(OBJ:.o=.$(OBJ_EXT))
	(cd .. ; $(MAKE) force-lib)

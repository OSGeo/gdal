
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
	for f in *.h ; do $(INSTALL_DATA) $$f $(DESTDIR)$(INST_INCLUDE) ; done

lib:	$(OBJ:.o=.$(OBJ_EXT))
	(cd .. ; $(MAKE) force-lib)

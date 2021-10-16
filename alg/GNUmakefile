
include ../GDALmake.opt

OBJ	=	gdalmediancut.o gdaldither.o gdal_crs.o gdaltransformer.o \
		gdalsimplewarp.o gdalwarper.o gdalwarpkernel.o \
		gdalwarpoperation.o gdalchecksum.o gdal_rpc.o gdal_tps.o \
		thinplatespline.o llrasterize.o gdalrasterize.o gdalgeoloc.o \
		gdalgrid.o gdalcutline.o gdalproximity.o rasterfill.o \
		gdalrasterpolygonenumerator.o \
		gdalsievefilter.o gdalwarpkernel_opencl.o polygonize.o \
		contour.o gdaltransformgeolocs.o gdallinearsystem.o \
		gdal_octave.o gdal_simplesurf.o gdalmatching.o delaunay.o \
		gdalpansharpen.o gdalapplyverticalshiftgrid.o viewshed.o

ifeq ($(HAVE_GEOS),yes)
CPPFLAGS 	:=	-DHAVE_GEOS=1 $(GEOS_CFLAGS) $(CPPFLAGS)
endif

ifeq ($(HAVE_ARMADILLO),yes)
CPPFLAGS 	:=	-DHAVE_ARMADILLO $(CPPFLAGS)
endif

ifeq ($(QHULL_SETTING),external)
CPPFLAGS 	:=	-DEXTERNAL_QHULL -DQHULL_INCLUDE_SUBDIR_IS_LIBQHULL=$(QHULL_INCLUDE_SUBDIR_IS_LIBQHULL) $(CPPFLAGS)
endif
ifeq ($(QHULL_SETTING),internal)
CPPFLAGS 	:=	-DINTERNAL_QHULL $(CPPFLAGS)
endif

CPPFLAGS	:=	-iquote ../frmts/vrt -iquote ../frmts/mem $(CPPFLAGS) $(OPENCL_FLAGS) $(PROJ_FLAGS) $(PROJ_INCLUDE) -iquote marching_squares

CXXFLAGS	:=	$(WARN_EFFCPLUSPLUS) $(WARN_OLD_STYLE_CAST) $(CXXFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT)) gdalgridavx.$(OBJ_EXT) gdalgridsse.$(OBJ_EXT)

# We use CXXFLAGS_NO_LTO_IF_AVX_NONDEFAULT to avoid the whole library to be compiled with -mavx
# if -mavx is not the default
gdalgridavx.$(OBJ_EXT):   gdalgridavx.cpp
	$(CXX) $(GDAL_INCLUDE) $(CXXFLAGS_NO_LTO_IF_AVX_NONDEFAULT) $(WARN_OLD_STYLE_CAST) $(AVXFLAGS) $(CPPFLAGS) -c -o $@ $<

gdalgridsse.$(OBJ_EXT):   gdalgridsse.cpp
	$(CXX) $(GDAL_INCLUDE) $(CXXFLAGS) $(WARN_OLD_STYLE_CAST) $(SSEFLAGS) $(CPPFLAGS) -c -o $@ $<

clean:
	$(RM) *.o $(O_OBJ)

docs:
	(cd ..; $(MAKE) docs)

INST_H_FILES = \
	gdal_alg.h \
	gdal_alg_priv.h \
	gdalgrid.h \
	gdalgrid_priv.h \
	gdalpansharpen.h \
	gdal_simplesurf.h \
	gdalwarper.h

install:
	for f in $(INST_H_FILES) ; do $(INSTALL_DATA) $$f $(DESTDIR)$(INST_INCLUDE) ; done

lib:	$(OBJ:.o=.$(OBJ_EXT))
	(cd .. ; $(MAKE) force-lib)

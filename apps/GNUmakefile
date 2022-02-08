# We get a full absolute path to the tree in this case so that when we
# generate the various paths in the local gdal-config file they will work
# from anywhere in the filesystem.

include ../GDALmake.opt

CPPFLAGS :=	-iquote $(GDAL_ROOT)/frmts/vrt $(CPPFLAGS) $(JSON_INCLUDE) $(PROJ_INCLUDE) -iquote $(GDAL_ROOT)/ogr/ogrsf_frmts/geojson -iquote $(GDAL_ROOT)/ogr/ogrsf_frmts/generic -g
LNK_FLAGS :=	$(LDFLAGS)
DEP_LIBS =	$(EXE_DEP_LIBS) $(XTRAOBJ)

BIN_LIST =	gdalinfo$(EXE) gdal_translate$(EXE) \
		gdaladdo$(EXE) gdalwarp$(EXE) nearblack$(EXE) gdalmanage$(EXE) \
		gdalenhance$(EXE) gdaltransform$(EXE) gdaldem$(EXE) \
		gdallocationinfo$(EXE) gdalsrsinfo$(EXE) gdalmdiminfo$(EXE) \
		gdalmdimtranslate$(EXE) gdal_viewshed$(EXE) gdal_create$(EXE)

OBJ = commonutils.o gdalinfo_lib.o gdal_translate_lib.o gdalwarp_lib.o ogr2ogr_lib.o \
	gdaldem_lib.o nearblack_lib.o gdal_grid_lib.o gdal_rasterize_lib.o gdalbuildvrt_lib.o \
	gdalmdiminfo_lib.o gdalmdimtranslate_lib.o

BIN_LIST += 	gdal_contour$(EXE) \
		gdaltindex$(EXE) \
		gdal_rasterize$(EXE) \
		gdal_grid$(EXE) \
		ogrinfo$(EXE) \
		ogr2ogr$(EXE) \
		ogrtindex$(EXE) \
		ogrlineref$(EXE) \
		gdalbuildvrt$(EXE)

ifeq ($(GNM_ENABLED),yes)
BIN_LIST += 	gnmmanage$(EXE) gnmanalyse$(EXE)
CPPFLAGS +=	-iquote $(GDAL_ROOT)/gnm
endif

ifeq ($(HAVE_GEOS),yes)
CPPFLAGS 	:=	$(CPPFLAGS) -DHAVE_GEOS=1
endif

NON_DEFAULT_LIST = 	multireadtest$(EXE) dumpoverviews$(EXE) \
	gdalwarpsimple$(EXE) gdalflattenmask$(EXE) \
	gdaltorture$(EXE) gdal2ogr$(EXE) \
	gdalasyncread$(EXE) testreprojmulti$(EXE)

default:	gdal-config-inst gdal-config $(BIN_LIST) test_ogrsf$(EXE)

all: default $(NON_DEFAULT_LIST)

lib-depend:
	(cd ../gcore ; $(MAKE) )
	(cd ../port ; $(MAKE) )

appslib: $(OBJ:.o=.$(OBJ_EXT))

gdalinfo_lib.$(OBJ_EXT): gdalinfo_lib.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

gdal_translate_lib.$(OBJ_EXT): gdal_translate_lib.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

gdalwarp_lib.$(OBJ_EXT): gdalwarp_lib.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

ogr2ogr_lib.$(OBJ_EXT): ogr2ogr_lib.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

gdaldem_lib.$(OBJ_EXT): gdaldem_lib.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

nearblack_lib.$(OBJ_EXT): nearblack_lib.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

gdal_grid_lib.$(OBJ_EXT): gdal_grid_lib.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

gdal_rasterize_lib.$(OBJ_EXT): gdal_rasterize_lib.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

gdalbuildvrt_lib.$(OBJ_EXT): gdalbuildvrt_lib.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

gdalmdiminfo_lib.$(OBJ_EXT): gdalmdiminfo_lib.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

gdalmdimtranslate_lib.$(OBJ_EXT): gdalmdimtranslate_lib.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

gdalinfo$(EXE):	gdalinfo_bin.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) $(CONFIG_LIB_UTILS) -o $@

gdalmdiminfo$(EXE):	gdalmdiminfo_bin.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) $(CONFIG_LIB_UTILS) -o $@

gdalmdimtranslate$(EXE):	gdalmdimtranslate_bin.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) $(CONFIG_LIB_UTILS) -o $@

gdal_translate$(EXE):	gdal_translate_bin.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdaladdo$(EXE):	gdaladdo.$(OBJ_EXT)  $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdalwarp$(EXE): gdalwarp_bin.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdal_contour$(EXE):	gdal_contour.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

nearblack$(EXE):	nearblack_bin.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdalmanage$(EXE):	gdalmanage.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdal_rasterize$(EXE):	gdal_rasterize_bin.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdaltindex$(EXE):	gdaltindex.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdalbuildvrt$(EXE):	gdalbuildvrt_bin.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

multireadtest$(EXE):	multireadtest.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

dumpoverviews$(EXE):	dumpoverviews.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdalenhance$(EXE):	gdalenhance.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdaldem$(EXE): gdaldem_bin.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdal_grid$(EXE):	gdal_grid_bin.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdalwarpsimple$(EXE): gdalwarpsimple.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdaltransform$(EXE): gdaltransform.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdallocationinfo$(EXE): gdallocationinfo.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdalsrsinfo$(EXE):	gdalsrsinfo.$(OBJ_EXT)  $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdalflattenmask$(EXE): gdalflattenmask.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdaltorture$(EXE): gdaltorture.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdal2ogr$(EXE):	gdal2ogr.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

ogrinfo$(EXE):	ogrinfo.$(OBJ_EXT)  $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $<  $(XTRAOBJ) $(CONFIG_LIBS) -o $@

ogrlineref$(EXE):	ogrlineref.$(OBJ_EXT)  $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $<  $(XTRAOBJ) $(CONFIG_LIBS) -o $@

ogr2ogr$(EXE):	ogr2ogr_bin.$(OBJ_EXT)  $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

ogrtindex$(EXE):	ogrtindex.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

test_ogrsf$(EXE):	test_ogrsf.$(OBJ_EXT)  $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdalasyncread$(EXE):	gdalasyncread.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

testreprojmulti$(EXE):	testreprojmulti.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gnmmanage$(EXE):	gnmmanage.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gnmanalyse$(EXE):	gnmanalyse.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdal_viewshed$(EXE):	gdal_viewshed.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdal_create$(EXE):	gdal_create.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

clean:
	$(RM) *.o $(BIN_LIST) $(NON_DEFAULT_LIST) core gdal-config gdal-config-inst

$(DEP_LIBS):


gdal-config:	gdal-config.in ../GDALmake.opt ./GNUmakefile ../VERSION
	rm -f gdal-config
	echo '#!/bin/sh' > gdal-config
	echo 'CONFIG_LIBS="$(CONFIG_LIBS)"' >> gdal-config
	echo 'CONFIG_DEP_LIBS="$(LIBS)"' >> gdal-config
	echo 'CONFIG_PREFIX="$(GDAL_ROOT)"' >> gdal-config
	echo 'CONFIG_CFLAGS="-I$(GDAL_ROOT)/port -I$(GDAL_ROOT)/generated_headers -I$(GDAL_ROOT)/gcore -I$(GDAL_ROOT)/alg -I$(GDAL_ROOT)/gnm -I$(GDAL_ROOT)/ogr -I$(GDAL_ROOT)/ogr/ogrsf_frmts -I$(GDAL_ROOT)/frmts/vrt -I$(GDAL_ROOT)/apps"' >> gdal-config
	echo 'CONFIG_DATA="$(GDAL_ROOT)/data"' >> gdal-config
	echo 'CONFIG_VERSION="'`cat ../VERSION`'"' >> gdal-config
	echo 'CONFIG_OGR_ENABLED=yes' >> gdal-config  # Deprecated.  Always true.
	echo 'CONFIG_GNM_ENABLED=$(GNM_ENABLED)' >> gdal-config
	echo 'CONFIG_FORMATS="$(GDAL_FORMATS)"' >> gdal-config
	cat gdal-config.in >> gdal-config
	chmod a+x gdal-config

gdal-config-inst:	gdal-config.in ../GDALmake.opt ./GNUmakefile ../VERSION
	rm -f gdal-config-inst
	echo '#!/bin/sh' > gdal-config-inst
	echo 'CONFIG_LIBS="$(CONFIG_LIBS_INS)"' >> gdal-config-inst
	echo 'CONFIG_DEP_LIBS="$(LIBS)"' >> gdal-config-inst
	echo 'CONFIG_PREFIX="$(INST_PREFIX)"' >> gdal-config-inst
	echo 'CONFIG_CFLAGS="-I$(INST_INCLUDE)"' >> gdal-config-inst
	echo 'CONFIG_DATA="$(INST_DATA)"' >> gdal-config-inst
	echo 'CONFIG_VERSION="'`cat ../VERSION`'"' >> gdal-config-inst
	echo 'CONFIG_OGR_ENABLED=yes' >> gdal-config-inst  # Deprecated.  Always true.
	echo 'CONFIG_GNM_ENABLED=$(GNM_ENABLED)' >> gdal-config-inst
	echo 'CONFIG_FORMATS="$(GDAL_FORMATS)"' >> gdal-config-inst
	cat gdal-config.in >> gdal-config-inst
	chmod a+x gdal-config-inst


install: default
	for f in $(BIN_LIST) ; do $(INSTALL) $$f $(DESTDIR)$(INST_BIN) ; done
	$(INSTALL_DATA) gdal_utils.h $(DESTDIR)$(INST_INCLUDE)
	$(INSTALL) gdal-config-inst $(DESTDIR)$(INST_BIN)/gdal-config


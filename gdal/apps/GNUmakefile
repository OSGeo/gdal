# We get a full absolute path to the tree in this case so that when we
# generate the various paths in the local gdal-config file they will work
# from anywhere in the filesystem. 

include ../GDALmake.opt

CPPFLAGS :=	-I$(GDAL_ROOT)/frmts $(CPPFLAGS)
LNK_FLAGS :=	$(LDFLAGS)
DEP_LIBS =	$(EXE_DEP_LIBS) $(XTRAOBJ)

BIN_LIST =	gdalinfo$(EXE) gdalserver$(EXE) gdal_translate$(EXE) \
		gdaladdo$(EXE) gdalwarp$(EXE) nearblack$(EXE) gdalmanage$(EXE) \
		gdalenhance$(EXE) gdaltransform$(EXE) gdaldem$(EXE) \
		gdallocationinfo$(EXE) gdalsrsinfo$(EXE) 

ifeq ($(OGR_ENABLED),yes)
BIN_LIST += 	gdal_contour$(EXE) \
		gdaltindex$(EXE) \
		gdal_rasterize$(EXE) \
		gdal_grid$(EXE) \
		ogrinfo$(EXE) \
		ogr2ogr$(EXE) \
		ogrtindex$(EXE) \
		ogrlineref$(EXE) \
		testepsg$(EXE) \
		gdalbuildvrt$(EXE)
endif

ifeq ($(HAVE_GEOS),yes)
CPPFLAGS 	:=	$(CPPFLAGS) -DHAVE_GEOS=1 $(GEOS_CFLAGS)
endif

NON_DEFAULT_LIST = 	multireadtest$(EXE) dumpoverviews$(EXE) \
	gdalwarpsimple$(EXE) gdalflattenmask$(EXE) \
	gdaltorture$(EXE) gdal2ogr$(EXE) test_ogrsf$(EXE) \
	gdalasyncread$(EXE) testreprojmulti$(EXE)

default:	gdal-config-inst gdal-config $(BIN_LIST)

all: default $(NON_DEFAULT_LIST)

lib-depend:
	(cd ../gcore ; $(MAKE) )
	(cd ../port ; $(MAKE) )

gdalinfo$(EXE):	gdalinfo.$(OBJ_EXT) commonutils.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< commonutils.$(OBJ_EXT) $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdalserver$(EXE):	gdalserver.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdal_translate$(EXE):	gdal_translate.$(OBJ_EXT) commonutils.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< commonutils.$(OBJ_EXT) $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdaladdo$(EXE):	gdaladdo.$(OBJ_EXT)  $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdalwarp$(EXE): gdalwarp.$(OBJ_EXT) commonutils.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< commonutils.$(OBJ_EXT) $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdal_contour$(EXE):	gdal_contour.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

nearblack$(EXE):	nearblack.$(OBJ_EXT) commonutils.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< commonutils.$(OBJ_EXT) $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdalmanage$(EXE):	gdalmanage.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdal_rasterize$(EXE):	gdal_rasterize.$(OBJ_EXT) commonutils.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< commonutils.$(OBJ_EXT) $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdaltindex$(EXE):	gdaltindex.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdalbuildvrt$(EXE):	gdalbuildvrt.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

multireadtest$(EXE):	multireadtest.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

dumpoverviews$(EXE):	dumpoverviews.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdalenhance$(EXE):	gdalenhance.$(OBJ_EXT) commonutils.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< commonutils.$(OBJ_EXT) $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdaldem$(EXE): gdaldem.$(OBJ_EXT) commonutils.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< commonutils.$(OBJ_EXT) $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdal_grid$(EXE):	gdal_grid.$(OBJ_EXT) commonutils.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< commonutils.$(OBJ_EXT) $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdalwarpsimple$(EXE): gdalwarpsimple.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdaltransform$(EXE): gdaltransform.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdallocationinfo$(EXE): gdallocationinfo.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdalsrsinfo$(EXE):	gdalsrsinfo.$(OBJ_EXT) commonutils.$(OBJ_EXT)  $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< commonutils.$(OBJ_EXT) $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdalflattenmask$(EXE): gdalflattenmask.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdaltorture$(EXE): gdaltorture.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdal2ogr$(EXE):	gdal2ogr.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

ogrinfo$(EXE):	ogrinfo.$(OBJ_EXT) commonutils.$(OBJ_EXT)  $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< commonutils.$(OBJ_EXT)  $(XTRAOBJ) $(CONFIG_LIBS) -o $@

ogrlineref$(EXE):	ogrlineref.$(OBJ_EXT) commonutils.$(OBJ_EXT)  $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< commonutils.$(OBJ_EXT)  $(XTRAOBJ) $(CONFIG_LIBS) -o $@

ogr2ogr$(EXE):	ogr2ogr.$(OBJ_EXT) commonutils.$(OBJ_EXT)  $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< commonutils.$(OBJ_EXT) $(XTRAOBJ) $(CONFIG_LIBS) -o $@

testepsg$(EXE):	testepsg.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

ogrtindex$(EXE):	ogrtindex.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

test_ogrsf$(EXE):	test_ogrsf.$(OBJ_EXT) commonutils.$(OBJ_EXT)  $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< commonutils.$(OBJ_EXT) $(XTRAOBJ) $(CONFIG_LIBS) -o $@

gdalasyncread$(EXE):	gdalasyncread.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

testreprojmulti$(EXE):	testreprojmulti.$(OBJ_EXT) $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) $< $(XTRAOBJ) $(CONFIG_LIBS) -o $@

clean:
	$(RM) *.o $(BIN_LIST) core gdal-config gdal-config-inst

$(DEP_LIBS):


gdal-config:	gdal-config.in ../GDALmake.opt ./GNUmakefile ../VERSION
	rm -f gdal-config
	echo '#!/bin/sh' > gdal-config
	echo 'CONFIG_LIBS="$(CONFIG_LIBS)"' >> gdal-config
	echo 'CONFIG_DEP_LIBS="$(LIBS)"' >> gdal-config
	echo 'CONFIG_PREFIX="$(GDAL_ROOT)"' >> gdal-config
	echo 'CONFIG_CFLAGS="-I$(GDAL_ROOT)/port -I$(GDAL_ROOT)/gcore -I$(GDAL_ROOT)/alg -I$(GDAL_ROOT)/ogr -I$(GDAL_ROOT)/ogr/ogrsf_frmts -I$(GDAL_ROOT)/frmts/vrt"' >> gdal-config
	echo 'CONFIG_DATA="$(GDAL_ROOT)/data"' >> gdal-config
	echo 'CONFIG_VERSION="'`cat ../VERSION`'"' >> gdal-config
	echo 'CONFIG_OGR_ENABLED=$(OGR_ENABLED)' >> gdal-config
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
	echo 'CONFIG_OGR_ENABLED=$(OGR_ENABLED)' >> gdal-config-inst
	echo 'CONFIG_FORMATS="$(GDAL_FORMATS)"' >> gdal-config-inst
	cat gdal-config.in >> gdal-config-inst
	chmod a+x gdal-config-inst


install: default
	for f in $(BIN_LIST) ; do $(INSTALL) $$f $(DESTDIR)$(INST_BIN) ; done
	$(INSTALL) gdal-config-inst $(DESTDIR)$(INST_BIN)/gdal-config


# We get a full absolute path to the tree in this case so that when we
# generate the various paths in the local gdal-config file they will work
# from anywhere in the filesystem. 

include ../GDALmake.opt

CPPFLAGS	:=	$(GDAL_INCLUDE) -I$(GDAL_ROOT)/frmts $(CPPFLAGS)
DEP_LIBS	=	$(EXE_DEP_LIBS) $(XTRAOBJ)
BIN_LIST	=	gdalinfo$(EXE) gdal_translate$(EXE) gdaladdo$(EXE) \
			gdalwarp$(EXE) nearblack$(EXE) gdalmanage$(EXE) \
			gdalenhance$(EXE) gdaltransform$(EXE)

ifeq ($(OGR_ENABLED),yes)
BIN_LIST += 	gdal_contour$(EXE) \
		gdaltindex$(EXE) \
		gdal_rasterize$(EXE) \
		gdal_grid$(EXE) \
		ogrinfo$(EXE) \
		ogr2ogr$(EXE) \
		ogrtindex$(EXE)
endif

default:	gdal-config-inst gdal-config $(BIN_LIST)

lib-depend:
	(cd ../gcore ; $(MAKE) )
	(cd ../port ; $(MAKE) )

gdalinfo$(EXE):	gdalinfo.o  $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) gdalinfo.o $(XTRAOBJ) \
		$(CONFIG_LIBS) -o gdalinfo$(EXE)

gdal_translate$(EXE):	gdal_translate.o  $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) gdal_translate.o $(XTRAOBJ) \
		$(CONFIG_LIBS) -o gdal_translate$(EXE)

gdaladdo$(EXE):	gdaladdo.o  $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) gdaladdo.o $(XTRAOBJ) \
		$(CONFIG_LIBS) -o gdaladdo$(EXE)

gdalwarp$(EXE): gdalwarp.o $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) gdalwarp.o $(XTRAOBJ) \
		$(CONFIG_LIBS) -o gdalwarp$(EXE)

gdal_contour$(EXE):	gdal_contour.o $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) gdal_contour.o $(CONFIG_LIBS) -o gdal_contour$(EXE)

nearblack$(EXE):	nearblack.o $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) nearblack.o $(CONFIG_LIBS) -o nearblack$(EXE)

gdalmanage$(EXE):	gdalmanage.o $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) gdalmanage.o $(CONFIG_LIBS) -o gdalmanage$(EXE)

gdal_rasterize$(EXE):	gdal_rasterize.o $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) gdal_rasterize.o $(CONFIG_LIBS) -o gdal_rasterize$(EXE)

gdaltindex$(EXE):	gdaltindex.o $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) gdaltindex.o $(XTRAOBJ) $(CONFIG_LIBS) \
		-o gdaltindex$(EXE)

gdalbuildvrt$(EXE):	gdalbuildvrt.o $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) gdalbuildvrt.o $(XTRAOBJ) $(CONFIG_LIBS)  \
		-o gdalbuildvrt$(EXE)

multireadtest$(EXE):	multireadtest.o $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) multireadtest.o $(XTRAOBJ) $(CONFIG_LIBS) \
		 -o multireadtest$(EXE)

dumpoverviews$(EXE):	dumpoverviews.o $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) dumpoverviews.o $(XTRAOBJ) $(CONFIG_LIBS) \
		 -o dumpoverviews$(EXE)

gdalenhance$(EXE):	gdalenhance.o $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) gdalenhance.o $(XTRAOBJ) $(CONFIG_LIBS) \
		 -o gdalenhance$(EXE)

gdal_grid$(EXE):	gdal_grid.o $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) gdal_grid.o $(XTRAOBJ) $(CONFIG_LIBS) \
		 -o gdal_grid$(EXE)

gdalwarpsimple$(EXE): gdalwarpsimple.o $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) gdalwarpsimple.o $(XTRAOBJ) \
		$(CONFIG_LIBS) -o gdalwarpsimple$(EXE)

gdaltransform$(EXE): gdaltransform.o $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) gdaltransform.o $(XTRAOBJ) \
		$(CONFIG_LIBS) -o gdaltransform$(EXE)

ogrinfo$(EXE):	ogrinfo.o $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) ogrinfo.o $(XTRAOBJ) $(CONFIG_LIBS) \
		 -o ogrinfo$(EXE)

ogr2ogr$(EXE):	ogr2ogr.o $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) ogr2ogr.o $(XTRAOBJ) $(CONFIG_LIBS) \
		 -o ogr2ogr$(EXE)

testepsg$(EXE):	testepsg.o $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) testepsg.o $(XTRAOBJ) $(CONFIG_LIBS) \
		 -o testepsg$(EXE)

ogrtindex$(EXE):	ogrtindex.o $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) ogrtindex.o $(XTRAOBJ) $(CONFIG_LIBS) \
		 -o ogrtindex$(EXE)

test_ogrsf$(EXE):	test_ogrsf.o $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) test_ogrsf.o $(XTRAOBJ) $(CONFIG_LIBS) \
		 -o test_ogrsf$(EXE)

clean:
	$(RM) *.o $(BIN_LIST) core gdal-config gdal-config-inst

$(DEP_LIBS):


gdal-config:	gdal-config.in ../GDALmake.opt ./GNUmakefile ../VERSION
	rm -f gdal-config
	echo '#!/bin/sh' > gdal-config
	echo 'CONFIG_LIBS="$(CONFIG_LIBS)"' >> gdal-config
	echo 'CONFIG_DEP_LIBS="$(LIBS)"' >> gdal-config
	echo 'CONFIG_PREFIX="$(GDAL_ROOT)"' >> gdal-config
	echo 'CONFIG_CFLAGS="-I$(GDAL_ROOT)/port -I$(GDAL_ROOT)/gcore -I$(GDAL_ROOT)/alg -I$(GDAL_ROOT)/ogr -I$(GDAL_ROOT)/ogr/ogrsf_frmts"' >> gdal-config
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


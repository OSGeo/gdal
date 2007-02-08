# We get a full absolute path to the tree in this case so that when we
# generate the various paths in the local gdal-config file they will work
# from anywhere in the filesystem. 

include ../GDALmake.opt

CPPFLAGS	:=	$(GDAL_INCLUDE) -I$(GDAL_ROOT)/frmts $(CPPFLAGS)
DEP_LIBS	=	$(EXE_DEP_LIBS) $(XTRAOBJ)
BIN_LIST	=	gdalinfo$(EXE) gdal_translate$(EXE) gdaladdo$(EXE) \
			gdalwarp$(EXE) nearblack$(EXE)

ifeq ($(OGR_ENABLED),yes)
BIN_LIST	+= 	gdal_contour$(EXE) gdaltindex$(EXE) gdal_rasterize$(EXE)
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

gdal_rasterize$(EXE):	gdal_rasterize.o $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) gdal_rasterize.o $(CONFIG_LIBS) -o gdal_rasterize$(EXE)

gdaltindex$(EXE):	gdaltindex.o $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) gdaltindex.o $(XTRAOBJ) $(CONFIG_LIBS) \
		-o gdaltindex$(EXE)

multireadtest$(EXE):	multireadtest.o $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) multireadtest.o $(XTRAOBJ) $(CONFIG_LIBS) \
		 -o multireadtest$(EXE)

dumpoverviews$(EXE):	dumpoverviews.o $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) dumpoverviews.o $(XTRAOBJ) $(CONFIG_LIBS) \
		 -o dumpoverviews$(EXE)

test1$(EXE):	test1.o $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) test1.o $(XTRAOBJ) $(CONFIG_LIBS) \
		 -o test1$(EXE)

gdalwarpsimple$(EXE): gdalwarpsimple.o $(DEP_LIBS)
	$(LD) $(LNK_FLAGS) gdalwarpsimple.o $(XTRAOBJ) \
		$(CONFIG_LIBS) -o gdalwarpsimple$(EXE)

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
	echo 'CONFIG_VERSION="'`cat ../VERSION`'"' >> gdal-config-inst
	echo 'CONFIG_OGR_ENABLED=$(OGR_ENABLED)' >> gdal-config-inst
	echo 'CONFIG_FORMATS="$(GDAL_FORMATS)"' >> gdal-config-inst
	cat gdal-config.in >> gdal-config-inst
	chmod a+x gdal-config-inst


install: default
	for f in $(BIN_LIST) ; do $(INSTALL) $$f $(DESTDIR)$(INST_BIN) ; done
ifeq ($(MACOSX_FRAMEWORK),yes)
	for f in $(BIN_LIST) ; do \
		install_name_tool -change $(INST_LIB)/libgdal.$(GDAL_VERSION_MAJOR).dylib ${OSX_VERSION_FRAMEWORK_PREFIX}/GDAL $(DESTDIR)$(INST_BIN)/$$f ; \
	done
endif
	$(INSTALL) gdal-config-inst $(DESTDIR)$(INST_BIN)/gdal-config


GDAL_ROOT	=	$(shell (cd ..; pwd))

include ../GDALmake.opt

INCLUDE		=	$(GDAL_INCLUDE) -I$(GDAL_ROOT)/frmts -I$(GDAL_ROOT)/alg
DEP_LIBS	=	$(GDAL_LIB) $(XTRAOBJ)
BIN_LIST	=	gdalinfo$(EXE) gdal_translate$(EXE) gdaladdo$(EXE) \
			gdalwarp$(EXE)

default:	gdal-config-inst gdal-config $(BIN_LIST)

lib-depend:
	(cd ../gcore ; $(MAKE) )
	(cd ../port ; $(MAKE) )

gdalinfo$(EXE):	gdalinfo.o  $(DEP_LIBS)
	$(CXX) $(LNK_FLAGS) gdalinfo.o $(XTRAOBJ) \
		$(CONFIG_LIBS) -o gdalinfo$(EXE)

gdal_translate$(EXE):	gdal_translate.o  $(DEP_LIBS)
	$(CXX) $(LNK_FLAGS) gdal_translate.o $(XTRAOBJ) \
		$(CONFIG_LIBS) -o gdal_translate$(EXE)

gdaladdo$(EXE):	gdaladdo.o  $(DEP_LIBS)
	$(CXX) $(LNK_FLAGS) gdaladdo.o $(XTRAOBJ) \
		$(CONFIG_LIBS) -o gdaladdo$(EXE)

gdal2raw$(EXE):	gdal2raw.o $(DEP_LIBS)
	$(CXX) $(LNK_FLAGS) gdal2raw.o $(XTRAOBJ) $(CONFIG_LIBS) \
		-o gdal2raw$(EXE)

gdalwarp$(EXE): gdalwarp.o $(DEP_LIBS)
	$(CXX) $(LNK_FLAGS) gdalwarp.o $(XTRAOBJ) \
		$(CONFIG_LIBS) -o gdalwarp$(EXE)

gdal_chksum$(EXE):	gdal_chksum.o $(DEP_LIBS)
	$(CXX) $(LNK_FLAGS) gdal_chksum.o $(CONFIG_LIBS) -o gdal_chksum$(EXE)

gdaltindex$(EXE):	gdaltindex.o $(DEP_LIBS)
	$(CXX) $(LNK_FLAGS) gdaltindex.o $(XTRAOBJ) $(CONFIG_LIBS) \
		-o gdaltindex$(EXE)

test1$(EXE):	test1.o $(DEP_LIBS)
	$(CXX) $(LNK_FLAGS) test1.o $(XTRAOBJ) $(CONFIG_LIBS) \
		 -o test1$(EXE)

gdalwarptest$(EXE): gdalwarptest.o $(DEP_LIBS)
	$(CXX) $(LNK_FLAGS) gdalwarptest.o $(XTRAOBJ) \
		$(CONFIG_LIBS) -o gdalwarptest$(EXE)

clean:
	rm -f *.o $(BIN_LIST) core gdal-config gdal-config-inst

$(DEP_LIBS):

%.o:	%.c
	$(CC) $(INCLUDE) -c $(CFLAGS) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) $(INCLUDE) -c $(CXXFLAGS) $*.cpp -o $*.o


gdal-config:	gdal-config.in ../GDALmake.opt ./GNUmakefile ../VERSION
	rm -f gdal-config
	echo '#!/bin/sh' > gdal-config
	echo 'CONFIG_LIBS="$(CONFIG_LIBS)"' >> gdal-config
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
	echo 'CONFIG_PREFIX="$(INST_PREFIX)"' >> gdal-config-inst
	echo 'CONFIG_CFLAGS="-I$(INST_INCLUDE)"' >> gdal-config-inst
	echo 'CONFIG_VERSION="'`cat ../VERSION`'"' >> gdal-config-inst
	echo 'CONFIG_OGR_ENABLED=$(OGR_ENABLED)' >> gdal-config-inst
	echo 'CONFIG_FORMATS="$(GDAL_FORMATS)"' >> gdal-config-inst
	cat gdal-config.in >> gdal-config-inst
	chmod a+x gdal-config-inst


install: default
	for f in $(BIN_LIST) ; do $(INSTALL) $$f $(INST_BIN) ; done
	$(INSTALL) gdal-config-inst $(INST_BIN)/gdal-config


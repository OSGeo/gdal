GDAL_ROOT	=	$(shell (cd ..; pwd))

include ../GDALmake.opt

INCLUDE		=	$(GDAL_INCLUDE) -I$(GDAL_ROOT)/frmts
DEP_LIBS	=	$(GDAL_LIB) $(CPL_LIB) $(XTRAOBJ)
BIN_LIST	=	gdalinfo gdal_translate gdaladdo

default:	gdal-config-inst gdal-config $(BIN_LIST)

lib-depend:
	(cd ../core ; $(MAKE) )
	(cd ../port ; $(MAKE) )

gdalinfo:	gdalinfo.o $(DEP_LIBS)
	$(CXX) $(LNK_FLAGS) gdalinfo.o $(XTRAOBJ) $(GDAL_LIBS) $(LIBS) \
		 -o gdalinfo

gdal_translate:	gdal_translate.o  $(DEP_LIBS)
	$(CXX) $(LNK_FLAGS) gdal_translate.o $(XTRAOBJ) \
		$(GDAL_SLIB_LINK) $(LIBS) -o gdal_translate

gdaladdo:	gdaladdo.o  $(DEP_LIBS)
	$(CXX) $(LNK_FLAGS) gdaladdo.o $(XTRAOBJ) \
		$(GDAL_SLIB_LINK) $(LIBS) -o gdaladdo

gdal2raw:	gdal2raw.o $(DEP_LIBS)
	$(CXX) $(LNK_FLAGS) gdal2raw.o $(XTRAOBJ) $(GDAL_LIBS) $(LIBS) \
		-o gdal2raw

gdal_chksum:	gdal_chksum.o $(DEP_LIBS)
	$(CXX) $(LNK_FLAGS) gdal_chksum.o $(GDAL_LIBS) $(LIBS) -o gdal_chksum

clean:
	rm -f *.o gdalinfo core

$(DEP_LIBS):

%.o:	%.c
	$(CC) $(INCLUDE) -c $(CFLAGS) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) $(INCLUDE) -c $(CFLAGS) $*.cpp -o $*.o


gdal-config:	gdal-config.in ../GDALmake.opt ./GNUmakefile ../VERSION
	rm -f gdal-config
	echo '#!/bin/sh' > gdal-config
	echo 'CONFIG_LIBS="$(GDAL_SLIB_LINK)"' >> gdal-config
	echo 'CONFIG_PREFIX="$(GDAL_HOME)"' >> gdal-config
	echo 'CONFIG_CFLAGS="-I$(GDAL_HOME)/cpl -I$(GDAL_HOME)/core"' >> gdal-config
	echo 'CONFIG_VERSION="'`cat ../VERSION`'"' >> gdal-config
	echo 'CONFIG_OGR_ENABLED=$(OGR_ENABLED)' >> gdal-config
	echo 'CONFIG_FORMATS="$(GDAL_FORMATS)"' >> gdal-config
	cat gdal-config.in >> gdal-config
	chmod a+x gdal-config

gdal-config-inst:	gdal-config.in ../GDALmake.opt ./GNUmakefile ../VERSION
	rm -f gdal-config-inst
	echo '#!/bin/sh' > gdal-config-inst
	echo 'CONFIG_LIBS="-L$(INST_LIB) -lgdal.$(GDAL_VER)"' >> gdal-config-inst
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


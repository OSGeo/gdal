
GDAL_ROOT	=	.
WEB_DIR		= /web/remotesensing/gdal

include GDALmake.opt

GDAL_OBJ	=	$(GDAL_ROOT)/frmts/o/*.o \
			$(GDAL_ROOT)/gcore/*.o \
			$(GDAL_ROOT)/port/*.o \
			$(GDAL_ROOT)/alg/*.o \
			$(GDAL_ROOT)/ogr/ogrsf_frmts/o/*.o

include ./ogr/file.lst
GDAL_OBJ += $(addprefix ./ogr/,$(OBJ))

default:	lib-target py-target apps-target

lib-target:	check-lib;

force-lib:
	$(AR) r $(GDAL_LIB) $(GDAL_OBJ)
	$(RANLIB) $(GDAL_LIB)
	$(LD_SHARED) $(GDAL_SLIB_SONAME) $(GDAL_OBJ) $(GDAL_LIBS) $(LIBS) \
		-o $(GDAL_SLIB)

$(GDAL_LIB):	$(GDAL_OBJ)
	$(AR) r $(GDAL_LIB) $(GDAL_OBJ)
	$(RANLIB) $(GDAL_LIB)

$(GDAL_SLIB):	$(GDAL_OBJ)
	$(LD_SHARED) $(GDAL_SLIB_SONAME) $(GDAL_OBJ) $(GDAL_LIBS) $(LIBS) \
		-o $(GDAL_SLIB)

check-lib:	port-target core-target frmts-target ogr-target
	$(MAKE) $(GDAL_LIB)
ifeq ($(HAVE_LD_SHARED),yes)
	$(MAKE) $(GDAL_SLIB)
endif

port-target:
	(cd port; $(MAKE))

ogr-target:
	(cd ogr; $(MAKE) lib )

core-target:
	(cd gcore; $(MAKE))
	(cd alg; $(MAKE))

frmts-target:
	(cd frmts; $(MAKE))

ogr-all:
	(cd ogr; $(MAKE) all)

apps-target:	lib-target ogr-apps
	(cd apps; $(MAKE))


ogr-apps:	lib-target
	(cd ogr; $(MAKE) apps)

#
#	We only make python a default target if we think python is installed.
#
ifeq ($(PYTHON),no)
py-target: ;
else
py-target:	py-module;
endif

clean:	lclean
	(cd port; $(MAKE) clean)
	(cd ogr; $(MAKE) clean)
	(cd gcore; $(MAKE) clean)
	(cd frmts; $(MAKE) clean)
	(cd alg; $(MAKE) clean)
	(cd apps; $(MAKE) clean)
	(cd pymod; $(MAKE) clean)

py-module:	lib-target
	(cd pymod; $(MAKE))

lclean:
	rm -f *.a *.so config.log config.cache html/*.*

distclean:	dist-clean

dist-clean:	clean
	rm -f GDALmake.opt port/cpl_config.h config.cache config.status

config:	configure
	./configure

configure:	configure.in aclocal.m4
	autoconf

GDALmake.opt:	GDALmake.opt.in config.status
	./config.status

docs:
	(cd ogr; $(MAKE) docs)
	(cd html; rm -f *.*)
	doxygen
	cp data/gdalicon.png html
	cp frmts/*.html frmts/*/frmt_*.html html

all:	default ogr-all

install-docs:
	(cd ogr; $(MAKE) install-docs)
	$(INSTALL_DIR) $(INST_DOCS)/gdal
	cp html/*.* $(INST_DOCS)/gdal

web-update:	docs
	cp html/*.* $(WEB_DIR)

install:	all install-actions

install-actions:
	$(INSTALL_DIR) $(INST_BIN)
	$(INSTALL_DIR) $(INST_DATA)
	$(INSTALL_DIR) $(INST_LIB)
	$(INSTALL_DIR) $(INST_INCLUDE)
	(cd port; $(MAKE) install)
	(cd gcore; $(MAKE) install)
	(cd frmts; $(MAKE) install)
	(cd alg; $(MAKE) install)
	(cd ogr; $(MAKE) install)
	(cd apps; $(MAKE) install)
ifneq ($(PYTHON),no)
	(cd pymod; $(MAKE) install)
endif
	$(INSTALL_DATA) $(GDAL_LIB) $(INST_LIB)
ifeq ($(HAVE_LD_SHARED),yes)
	$(INSTALL_DATA) $(GDAL_SLIB) $(INST_LIB)
endif
	for f in data/*.* ; do $(INSTALL_DATA) $$f $(INST_DATA) ; done


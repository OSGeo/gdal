
GDAL_ROOT	=	.

include GDALmake.opt

default:	GDALmake.opt lib py-target apps-target

lib:	port-target core-target frmts-target ogr-target force-lib

force-lib:	
	$(AR) r $(GDAL_LIB) $(GDAL_OBJ)
	$(RANLIB) $(GDAL_LIB)
	$(LD_SHARED) $(GDAL_OBJ) $(GDAL_LIBS) $(LIBS) -o $(GDAL_SLIB)

#	If you really want proper SO files that will work in /usr/lib
# 	Try replacing the above command with something like this:
#
#	$(CXX) -shared -Wl,-soname,gdal.so.1 -o $(GDAL_SLIB) \
#		$(GDAL_OBJ) $(GDAL_LIBS) $(LIBS)

port-target:
	(cd port; $(MAKE))

ifeq ($(OGR_ENABLED),yes)

ogr-target:
	(cd ogr; $(MAKE) sublibs lib )

else

ogr-target:
	(cd ogr; $(MAKE) lib )

endif

core-target:
	(cd core; $(MAKE))
	(cd alg; $(MAKE))

frmts-target:
	(cd frmts; $(MAKE))

ogr-all:
	(cd ogr; $(MAKE) all)

apps-target:	ogr-apps
	(cd apps; $(MAKE))


ifeq ($(OGR_ENABLED),yes)

ogr-apps:
	(cd ogr; $(MAKE) apps)

else

ogr-apps:

endif

#
#	We only make python a default target if we think python is installed.
#
ifeq ($(PYTHON),no)
py-target:
else
py-target:	py-module
endif

clean:	lclean
	(cd port; $(MAKE) clean)
	(cd ogr; $(MAKE) clean)
	(cd core; $(MAKE) clean)
	(cd frmts; $(MAKE) clean)
	(cd apps; $(MAKE) clean)
	(cd pymod; $(MAKE) clean)

py-module:
	(cd pymod; $(MAKE))

lclean:
	rm -f *.a *.so config.log config.cache

distclean:	dist-clean

dist-clean:	clean
	rm -f GDALmake.opt port/cpl_config.h config.cache

config:	configure
	./configure

configure:	configure.in aclocal.m4
	autoconf

GDALmake.opt:	GDALmake.opt.in config.status
	./config.status

docs:
	(cd html; rm -f *.*)
	(cd html; cvs update gdal_index.html\
		             formats_list.html frmt_various.html)
	doxygen
	cp frmts/*/frmt_*.html html
	cp html/gdal_index.html html/index.html

all:	default ogr-all

web-update:	docs
	rm -rf /u/www/gdal/html
	mkdir /u/www/gdal/html
	cp html/*.* /u/www/gdal/html

install:	lib install-actions

install-actions:
	$(INSTALL_DIR) $(INST_BIN)
	$(INSTALL_DIR) $(INST_DATA)
	$(INSTALL_DIR) $(INST_LIB)
	$(INSTALL_DIR) $(INST_INCLUDE)
	(cd port; $(MAKE) install)
	(cd core; $(MAKE) install)
	(cd frmts; $(MAKE) install)
	(cd ogr; $(MAKE) install)
	(cd apps; $(MAKE) install)
ifneq ($(PYTHON),no)
	(cd pymod; $(MAKE) install)
endif
	$(INSTALL) $(GDAL_LIB) $(INST_LIB)
ifeq ($(HAVE_LD_SHARED),yes)
	$(INSTALL) $(GDAL_SLIB) $(INST_LIB)
endif
	for f in data/*.csv data/stateplane.txt ; do $(INSTALL) $$f $(INST_DATA) ; done

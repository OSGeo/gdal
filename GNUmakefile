
GDAL_ROOT	=	.

include GDALmake.opt

default:	lib GDALmake.opt
	(cd apps; $(MAKE))

lib:	$(GDAL_LIB)

$(GDAL_LIB):	port-target core-target frmts-target ogr-target force-lib py-target

force-lib:
	ar r $(GDAL_LIB) $(GDAL_OBJ)
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

frmts-target:
	(cd frmts; $(MAKE))

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
	rm configure GDALmake.opt.in port/cpl_config.h

config:	configure
	./configure

configure:	configure.in aclocal.m4
	autoconf

GDALmake.opt:	GDALmake.opt.in config.status
	config.status

docs:
	doxygen
	cp frmts/*/frmt_*.html html
	cp html/gdal_index.html html/index.html

all:	default

web-update:	docs
	rm -rf /u/www/gdal/html
	mkdir /u/www/gdal/html
	cp html/*.* /u/www/gdal/html

install:	$(GDAL_LIB)
	(cd port; $(MAKE) install)
	(cd core; $(MAKE) install)
	(cd frmts; $(MAKE) install)
	(cd ogr; $(MAKE) install)
	(cd apps; $(MAKE) install)
	cp $(GDAL_LIB) $(INST_LIB)
	cp $(GDAL_SLIB) $(INST_LIB)


GDAL_ROOT	=	.

include GDALmake.opt

default:	lib GDALmake.opt
	(cd apps; $(MAKE))

lib:	$(GDAL_LIB)

$(GDAL_LIB):	port-target core-target frmts-target force-lib

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

core-target:
	(cd core; $(MAKE))

frmts-target:
	(cd frmts; $(MAKE))

clean:	lclean
	(cd port; $(MAKE) clean)
	(cd core; $(MAKE) clean)
	(cd frmts; $(MAKE) clean)
	(cd apps; $(MAKE) clean)
	(cd viewer; $(MAKE) clean)

lclean:
	rm -f *.a *.so

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

all:	default


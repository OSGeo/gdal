
GDAL_ROOT	=	.

include GDALmake.opt

default:	lib GDALmake.opt
	(cd apps; $(MAKE))
	(cd viewer; $(MAKE))

lib:	$(GDAL_LIB)

$(GDAL_LIB):	port-target core-target frmts-target
	ar r $(GDAL_LIB) $(GDAL_OBJ)
	ld -share --whole-archive $(GDAL_LIBS) -o $(GDAL_SLIB)

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
	rm *.a *.so

dist-clean:	clean
	rm configure GDALmake.opt.in port/cpl_config.h

config:	configure
	./configure

configure:	configure.in
	autoconf

GDALmake.opt:	GDALmake.opt.in config.status
	config.status

docs:
	doxygen

all:	default


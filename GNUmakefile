
default:	GDALmake.opt
	(cd port; $(MAKE))
	(cd core; $(MAKE))
	(cd frmts; $(MAKE))
	(cd apps; $(MAKE))
	(cd viewer; $(MAKE))

clean:
	(cd port; $(MAKE) clean)
	(cd core; $(MAKE) clean)
	(cd frmts; $(MAKE) clean)
	(cd apps; $(MAKE) clean)
	(cd viewer; $(MAKE) clean)

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


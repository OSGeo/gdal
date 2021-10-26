include ../../GDALmake.opt

all: generate build

BINDING = perl

SWIGDEFINES = -DPERL_CPAN_NAMESPACE

SWIG_INCLUDES = ../include/perl/callback.i ../include/perl/confess.i ../include/perl/destroy.i ../include/perl/init.i ../include/perl/band.i

include ../SWIGmake.base

generate: ${WRAPPERS}
	if [ ! -d lib ]; then mkdir lib; fi
	if [ ! -d "lib/Geo" ]; then mkdir "lib/Geo"; fi
	if [ ! -d "lib/Geo/GDAL" ]; then mkdir "lib/Geo/GDAL"; fi
	if [ -f GDAL.pm ]; then mv GDAL.pm lib/Geo; fi
	if [ -f Const.pm ]; then mv Const.pm lib/Geo/GDAL; fi
	if [ -f OGR.pm ]; then mv OGR.pm lib/Geo; fi
	if [ -f OSR.pm ]; then mv OSR.pm lib/Geo; fi
	if [ -f GNM.pm ]; then mv GNM.pm lib/Geo; fi

build: Makefile_Geo__GDAL
	$(MAKE) -f Makefile_Geo__GDAL
	$(MAKE) -f Makefile_Geo__GDAL__Const
	$(MAKE) -f Makefile_Geo__OSR
	if [ -f Makefile_Geo__OGR ]; then $(MAKE) -f Makefile_Geo__OGR; fi
	if [ -f Makefile_Geo__GNM ]; then $(MAKE) -f Makefile_Geo__GNM; fi

Makefile_Geo__GDAL: ${WRAPPERS} Makefile.PL
	perl Makefile.PL INSTALL_BASE=$(INST_PREFIX)

test: build
	$(MAKE) -f Makefile_Geo__GDAL test

install: build
	$(MAKE) -f Makefile_Geo__GDAL install
	$(MAKE) -f Makefile_Geo__GDAL__Const install
	$(MAKE) -f Makefile_Geo__OSR install
	if [ -f Makefile_Geo__OGR ]; then $(MAKE) -f Makefile_Geo__OGR install; fi
	if [ -f Makefile_Geo__GNM ]; then $(MAKE) -f Makefile_Geo__GNM install; fi

dist: Makefile_Geo__GDAL
	$(MAKE) -f Makefile_Geo__GDAL dist

clean:
	-rm -f gdal.bs gdal_wrap.o
	-rm -f gdalconst.bs gdalconst_wrap.o
	-rm -f ogr.bs ogr_wrap.o
	-rm -f osr.bs osr_wrap.o
	-rm -f gnm.bs gnm_wrap.o
	-rm -rf blib
	-rm -f pm_to_blib
	-rm -f Makefile_Geo__GDAL Makefile_Geo__GDAL__Const Makefile_Geo__OGR Makefile_Geo__OSR Makefile_Geo__GNM

veryclean: clean
	-rm -f ${WRAPPERS} 
	-rm -f *.pm
	-rm -f META.* MYMETA.*
	-rm -f *.tar.gz
	-rm -f *.bs

doc:
	perl parse-for-doxygen.pl > all.pm; doxygen; rm -f all.pm

.PHONY: all generate build test install dist clean veryclean doc

BINDING = perl
SWIGDEFINES = -DPERL_CPAN_NAMESPACE

include ../SWIGmake.base

all:

clean:
	if [ -f Makefile_Geo__GDAL ]; then $(MAKE) -f Makefile_Geo__GDAL clean; fi
	if [ -f Makefile_Geo__GDAL__Const ]; then $(MAKE) -f Makefile_Geo__GDAL__Const clean; fi
	if [ -f Makefile_Geo__OGR ]; then $(MAKE) -f Makefile_Geo__OGR clean; fi
	if [ -f Makefile_Geo__OSR ]; then $(MAKE) -f Makefile_Geo__OSR clean; fi

veryclean: clean
	-rm -f ${WRAPPERS} lib/Geo/*.pm lib/Geo/GDAL/*.pm

generate: ${WRAPPERS}
	if [ ! -d lib ]; then mkdir lib; fi
	if [ ! -d "lib/Geo" ]; then mkdir "lib/Geo"; fi
	if [ ! -d "lib/Geo/GDAL" ]; then mkdir "lib/Geo/GDAL"; fi
	if [ -f GDAL.pm ]; then mv GDAL.pm lib/Geo; fi
	if [ -f Const.pm ]; then mv Const.pm lib/Geo/GDAL; fi
	if [ -f OGR.pm ]; then mv OGR.pm lib/Geo; fi
	if [ -f OSR.pm ]; then mv OSR.pm lib/Geo; fi

Makefile_Geo__GDAL:
	perl Makefile.PL

build: Makefile_Geo__GDAL
	$(MAKE) -f Makefile_Geo__GDAL
	$(MAKE) -f Makefile_Geo__GDAL__Const
	$(MAKE) -f Makefile_Geo__OGR
	$(MAKE) -f Makefile_Geo__OSR

install: build
	$(MAKE) -f Makefile_Geo__GDAL install
	$(MAKE) -f Makefile_Geo__GDAL__Const install
	$(MAKE) -f Makefile_Geo__OGR install
	$(MAKE) -f Makefile_Geo__OSR install

test: build
	$(MAKE) -f Makefile_Geo__GDAL test

dist: build
	$(MAKE) -f Makefile_Geo__GDAL dist

doc:

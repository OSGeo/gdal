BINDING = perl
SWIGDEFINES = -DPERL_CPAN_NAMESPACE

include ../SWIGmake.base

all:

clean:
	$(MAKE) -f Makefile clean

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

Makefile:
	perl Makefile.PL

build: Makefile
	$(MAKE) -f Makefile

install: build
	$(MAKE) -f Makefile install

test: build
	$(MAKE) -f Makefile test

dist: build
	$(MAKE) -f Makefile dist

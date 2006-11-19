BINDING = perl
SWIGDEFINES = -DPERL_CPAN_NAMESPACE

include ../../GDALmake.opt 
include ../SWIGmake.base

all:

clean:
	-rm -f gdal.bs gdal_wrap.o
	-rm -f gdalconst.bs gdalconst_wrap.o
	-rm -f ogr.bs ogr_wrap.o
	-rm -f osr.bs osr_wrap.o
	-rm -rf blib
	-rm -f pm_to_blib
	-rm -f Makefile_Geo__GDAL Makefile_Geo__GDAL__Const Makefile_Geo__OGR Makefile_Geo__OSR

veryclean: clean
	-rm -f ${WRAPPERS} 
	-rm -f gdal.pm gdalconst.pm osr.pm ogr.pm

generate: ${WRAPPERS}
	if [ -f GDAL.pm ]; then mv GDAL.pm lib/Geo; fi
	if [ -f Const.pm ]; then mv Const.pm lib/Geo/GDAL; fi
	if [ -f OGR.pm ]; then mv OGR.pm lib/Geo; fi
	if [ -f OSR.pm ]; then mv OSR.pm lib/Geo; fi
	echo "Please edit lib/Geo/GDAL.pm to contain 'use Geo::GDAL::Const; use Geo::OGR; use Geo::OSR; our (dollar)VERSION = '0.20';'"

Makefile_Geo__GDAL:
	PREFIX=${INST_PREFIX} perl Makefile.PL

build: Makefile_Geo__GDAL
	make -f Makefile_Geo__GDAL
	make -f Makefile_Geo__GDAL__Const
	make -f Makefile_Geo__OGR
	make -f Makefile_Geo__OSR

install: build
	make -f Makefile_Geo__GDAL install
	make -f Makefile_Geo__GDAL__Const install
	make -f Makefile_Geo__OGR install
	make -f Makefile_Geo__OSR install

test: build
	make -f Makefile_Geo__GDAL test

dist: build
	make -f Makefile_Geo__GDAL dist

Geo-GDAL.html: lib/Geo/GDAL.pod
	pod2html lib/Geo/GDAL.pod > Geo-GDAL.html

Geo-GDAL-Const.html: lib/Geo/GDAL/Const.pod
	pod2html lib/Geo/GDAL/Const.pod > Geo-GDAL-Const.html

Geo-OGR.html: lib/Geo/OGR.pod
	pod2html lib/Geo/OGR.pod > Geo-OGR.html

Geo-OSR.html: lib/Geo/OSR.pod
	pod2html lib/Geo/OSR.pod > Geo-OSR.html

doc: Geo-GDAL.html Geo-GDAL-Const.html Geo-OGR.html Geo-OSR.html

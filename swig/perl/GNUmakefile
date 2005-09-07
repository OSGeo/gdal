
GDAL_ROOT = ../..

include ../../GDALmake.opt

all: build

BINDING = perl
include ../SWIGmake.base

clean:
	-rm -f gdal.bs gdal_wrap.o
	-rm -f gdalconst.bs gdalconst_wrap.o
	-rm -f ogr.bs ogr_wrap.o
	-rm -f osr.bs osr_wrap.o
	-rm -rf blib
	-rm -f pm_to_blib

veryclean: clean
	-rm -f ${WRAPPERS} 
	-rm -f gdal.pm gdalconst.pm osr.pm ogr.pm
	-rm -f Makefile_gdal Makefile_gdalconst Makefile_ogr Makefile_osr

generate: ${WRAPPERS}

Makefile_gdal: Makefile.PL
	perl Makefile.PL

build: generate Makefile_gdal
	make -f Makefile_gdal
	make -f Makefile_gdalconst
	make -f Makefile_ogr
	make -f Makefile_osr

install: generate
	make -f Makefile install

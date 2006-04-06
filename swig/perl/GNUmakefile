

include ../../GDALmake.opt

all: 

BINDING = perl
include ../SWIGmake.base

clean:
	-rm -f gdal.bs gdal_wrap.o
	-rm -f gdalconst.bs gdalconst_wrap.o
	-rm -f ogr.bs ogr_wrap.o
	-rm -f osr.bs osr_wrap.o
	-rm -rf blib
	-rm -f pm_to_blib
	-rm -f Makefile_gdal Makefile_gdalconst Makefile_ogr Makefile_osr

veryclean: clean
	-rm -f ${WRAPPERS} 
	-rm -f gdal.pm gdalconst.pm osr.pm ogr.pm

generate: ${WRAPPERS}

Makefile_gdal: Makefile.PL ${GDAL_ROOT}/GDALmake.opt
	PREFIX=${INST_PREFIX} perl Makefile.PL

build:  Makefile_gdal
	make -f Makefile_gdal
	make -f Makefile_gdalconst
	make -f Makefile_ogr
	make -f Makefile_osr

install: Makefile_gdal build
	make -f Makefile_gdal install

test: Makefile_gdal
	make -f Makefile_gdal test

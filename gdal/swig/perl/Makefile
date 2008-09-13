
all: build

clean:
	-rm -f gdal.bs gdal_wrap.o
	-rm -f gdalconst.bs gdalconst_wrap.o
	-rm -f ogr.bs ogr_wrap.o
	-rm -f osr.bs osr_wrap.o
	-rm -rf blib
	-rm -f pm_to_blib
	-rm -f Makefile_Geo__GDAL Makefile_Geo__GDAL__Const Makefile_Geo__OGR Makefile_Geo__OSR

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

dist: Makefile_Geo__GDAL
	$(MAKE) -f Makefile_Geo__GDAL dist

Makefile_Geo__GDAL:
	perl Makefile.PL



include ../../GDALmake.opt

all:

BINDING = python
include ../SWIGmake.base

PY_COMMANDS     =       epsg_tr.py gdalchksum.py gdal2xyz.py gcps2wld.py \
                        gdalimport.py gdal_merge.py pct2rgb.py rgb2pct.py \
                        gcps2vec.py
PY_MODULES      =       gdal.py ogr.py osr.py gdalconst.py

clean:
	-rm -rf build

veryclean: clean
	-rm -f ${WRAPPERS} ${PY_MODULES}
	-rm -f *.pyc

gdal_wrap.cpp: ../include/python/gdal_python.i

ogr_wrap.cpp: ../include/python/ogr_python.i

osr_wrap.cpp: ../include/python/osr_python.i

generate: ${WRAPPERS}

build:
	python setup.py build

install:
	python setup.py install --prefix=$(DESTDIR)$(prefix)

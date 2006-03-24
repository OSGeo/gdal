

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

generate: ${WRAPPERS}

build:
	python setup.py build

install:
	python setup.py install --prefix=$(prefix)

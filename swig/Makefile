
SWIG_TARGETS = gdal_wrap.cpp gdalconst_wrap.c ogr_wrap.cpp osr_wrap.cpp

default: ${SWIG_TARGETS}
	python setup.py build

install: ${SWIG_TARGETS}
	python setup.py install

gdalconst_wrap.c: gdalconst.i
	swig -python -I../.. -o $@ $<

gdal_wrap.cpp:	gdal.i gdal_typemaps.i Driver.i Dataset.i Band.i ColorTable.i cpl.i
	swig -python -c++ -I../.. -o $@ $<

ogr_wrap.cpp:  ogr.i gdal_typemaps.i
	swig -python -c++ -I../.. -o $@ $<

osr_wrap.cpp:  osr.i gdal_typemaps.i
	swig -python -c++ -I../.. -o $@ $<

clean:
	-rm *~ *.pyc
	-rm gdal_wrap.cpp gdal.py
	-rm gdalconst_wrap.c gdalconst.py
	-rm ogr_wrap.cpp ogr.py
	-rm osr_wrap.cpp osr.py
	-rm -rf build



SWIG_TARGETS = gdal_wrap.cpp gdalconst_wrap.c ogr_wrap.cpp osr_wrap.cpp

SWIGARGS = -I../.. -python -modern -new_repr

TYPEMAPDEPS = typemaps.i typemaps_csharp.i typemaps_python.i

default: ${SWIG_TARGETS}
	python setup.py build

install: ${SWIG_TARGETS}
	python setup.py install

gdalconst_wrap.c: gdalconst.i
	swig ${SWIGARGS} -o $@ $<

gdal_wrap.cpp:	gdal.i ${TYPEMAPDEPS} Driver.i Dataset.i Band.i ColorTable.i cpl.i
	swig -c++ ${SWIGARGS} -o $@ $<

ogr_wrap.cpp:  ogr.i ${TYPEMAPDEPS}
	swig -c++ ${SWIGARGS} -o $@ $<

osr_wrap.cpp:  osr.i ${TYPEMAPDEPS}
	swig -c++ ${SWIGARGS} -o $@ $<

clean:
	-rm *~ *.pyc
	-rm gdal_wrap.cpp gdal.py
	-rm gdalconst_wrap.c gdalconst.py
	-rm ogr_wrap.cpp ogr.py
	-rm osr_wrap.cpp osr.py
	-rm -rf build


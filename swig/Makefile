
default: gdal_wrap.cpp
	python setup.py build

gdal_wrap.cpp:	gdal.i gdal_typemaps.i Driver.i Dataset.i Band.i
	swig -python -c++ -I../.. -o $@ $<

clean:
	-rm *~ *.pyc
	-rm gdal_wrap.cpp gdal.py
	-rm -rf build

install: default
	python setup.py install


default: gdal_wrap.cpp
	./setup.py build

gdal_wrap.cpp:	gdal.i
	swig -python -c++ -I.. -o $@ $<

clean:
	-rm *~ *.pyc
	-rm gdal_wrap.cpp gdal.py
	-rm -rf build
	-rm _gdal.so

TOPDIR?=$(shell pwd)
VERSION=$(shell cat gdal/VERSION)

.PHONY: install

all: hdf5r hdf5r_subds

dist:
	tar zcf "gdal-$(VERSION).tar.gz" --ignore-failed-read --exclude=.git --exclude=rpmbuild --exclude="gdal-$(VERSION).tar.gz" \
	--transform='s,^gdal,gdal-$(VERSION),' *

rpm:: dist
	mkdir -p rpmbuild/SOURCES
	cp 'gdal-$(VERSION).tar.gz' rpmbuild/SOURCES/
	rpmbuild -ba --define '_topdir $(TOPDIR)/rpmbuild' -vv rpmbuild/SPECS/gdal.spec

hdf5r:
	c++ -std=c++11 -fPIC -Wl,-z,defs -shared gdal/frmts/hdf5r/HDF5RDataSet.cpp gdal/frmts/hdf5r/HDF5RRasterBand.cpp \
	-Igdal/frmts/hdf5r/ -I/usr/include/gdal -lhdf5 -lgdal -o gdal_HDF5R.so

hdf5r_subds: hdf5r
	c++ -std=c++11 -fPIC -Wl,-z,defs -shared gdal/frmts/hdf5r/HDF5RSubDataSet.cpp \
	gdal/frmts/hdf5r/HDF5RRasterBand.cpp gdal/frmts/hdf5r/HDF5RDataSet.cpp -Igdal/frmts/hdf5r/ \
	-I/usr/include/gdal -lhdf5 -lgdal -o gdal_HDF5Rsubds.so

install:
	mkdir -p $(DESTDIR)$(PREFIX)/lib
	install gdal_HDF5R.so $(DESTDIR)$(PREFIX)/lib
	install gdal_HDF5Rsubds.so $(DESTDIR)$(PREFIX)/lib

clean:
	rm gdal*.tgz
	rm -rf rpmbuild/BUILD
	rm -rf rpmbuild/BUILDROOT
	rm -rf rpmbuild/SOURCES
	rm -rf rpmbuild/RPMS
	rm -rf rpmbuild/SRPMS

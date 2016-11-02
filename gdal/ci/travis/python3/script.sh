#!/bin/sh

set -e

cd gdal

# Perl unit tests
#cd swig/perl
#make test
#cd ../..
# Java unit tests
#cd swig/java
#make test
#cd ../..
# CPP unit tests
cd ../autotest
#cd cpp
#GDAL_SKIP=JP2ECW make quick_test
# Compile and test vsipreload
#make vsipreload.so
#LD_PRELOAD=./vsipreload.so gdalinfo /vsicurl/http://download.osgeo.org/gdal/data/ecw/spif83.ecw
#LD_PRELOAD=./vsipreload.so sqlite3  /vsicurl/http://download.osgeo.org/gdal/data/sqlite3/polygon.db "select * from polygon limit 10"
#cd ..
# Download a sample file
mkdir -p ogr/tmp/cache/
cd ogr/tmp/cache/
wget http://download.osgeo.org/gdal/data/pgeo/PGeoTest.zip
unzip PGeoTest.zip
cd ../../..
# Run ogr_fgdb.py in isolation from the rest
cd ogr
python3 ogr_fgdb.py
mkdir disabled
mv ogr_fgdb.* disabled
cd ..
# Run ogr_pgeo.py in isolation from the rest
cd ogr
python3 ogr_pgeo.py
mv ogr_pgeo.* disabled
cd ..
# Run all the Python autotests
GDAL_SKIP="JP2ECW ECW" python3 run_all.py
# A bit messy, but force testing with libspatialite 4.0dev (that has been patched a bit to remove any hard-coded SRS definition so it is very small)
cd ogr
wget http://s3.amazonaws.com/etc-data.koordinates.com/gdal-travisci/libspatialite4.0dev_ubuntu12.04-64bit_srs_stripped.tar.gz
tar xzf libspatialite4.0dev_ubuntu12.04-64bit_srs_stripped.tar.gz
ln -s install-libspatialite-4.0dev/lib/libspatialite.so.5.0.1 libspatialite.so.3
LD_LIBRARY_PATH=$PWD python3 ogr_sqlite.py
cd ..

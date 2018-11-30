#!/bin/bash

set -e

export PATH=$PWD/install-gcc-5.2.0/bin:$PATH
export LD_LIBRARY_PATH=$PWD/install-gcc-5.2.0/lib64
export PRELOAD=$PWD/install-gcc-5.2.0/lib64/libasan.so.2.0.0:$PWD/install-gcc-5.2.0/lib64/libubsan.so.0.0.0
#export PRELOAD=$PWD/install-gcc-5.2.0/lib64/libubsan.so.0.0.0
export PYTEST="pytest -vv -p no:sugar --color=no"

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
#mkdir -p ogr/tmp/cache/
#cd ogr/tmp/cache/
#wget http://download.osgeo.org/gdal/data/pgeo/PGeoTest.zip
#unzip PGeoTest.zip
#cd ../../..
# Don't run these
rm ogr/ogr_fgdb.py ogr/ogr_pgeo.py

# Too old spatialite version
rm ogr/ogr_sqlite.py gdrivers/rasterlite.py

# install test dependencies
# note: pip 9 is installed on the box, but it hits a strange error after upgrading setuptools.
# so we install a newer pip first.
sudo -H pip install -U pip
sudo -H pip install -U -r ./requirements.txt


# Run each module in its own pytest process.
# This makes sure the output from the address sanitizer is relevant
# and it doesn't blow out RAM too much.
# Unfortunately it's also a reasonably large slowdown since we have to wait
# for a python interpreter and all modules to load between each module.
# (and add a grep to get rid of the extra pytest header headers/etc)
# 
# NOTE: `find ... -exec` always exits with 0 even when the tests failed.
# That turns out to be what we want here though, since we want
# to not fail when the address sanitizer finds errors.
# So we tee the output to a file and grep it to discover if the tests failed.
export SKIP_MEM_INTENSIVE_TEST=YES SKIP_VIRTUALMEM=YES LD_PRELOAD=$PRELOAD \
    ASAN_OPTIONS=detect_leaks=1,print_suppressions=0,suppressions=$PWD/asan_suppressions.txt
find \
    ogr gcore gdrivers osr alg gnm utilities pyscripts \
    -name '*.py' ! -name netcdf_cfchecks.py ! -name "__init__.py" \
    -print \
    -exec $PYTEST -o console_output_style=classic {} \; \
    | tee ./test-output.txt

# Check if the tests failed and error out.
if grep -P '===.*\d+ failed' ./test-output.txt > /dev/null ; then
    echo 'Tests failed'
    exit 1
else
    echo 'Tests passed'
fi

# A bit messy, but force testing with libspatialite 4.0dev (that has been patched a bit to remove any hard-coded SRS definition so it is very small)
#cd ogr
#wget http://s3.amazonaws.com/etc-data.koordinates.com/gdal-travisci/libspatialite4.0dev_ubuntu12.04-64bit_srs_stripped.tar.gz
#tar xzf libspatialite4.0dev_ubuntu12.04-64bit_srs_stripped.tar.gz
#ln -s install-libspatialite-4.0dev/lib/libspatialite.so.5.0.1 libspatialite.so.3
#LD_PRELOAD=$PRELOAD LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PWD $PYTEST ogr_sqlite.py
#cd ..

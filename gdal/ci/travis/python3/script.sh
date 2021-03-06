#!/bin/sh

set -e

# When run in the same process after ogr_pgeo.py (with the MDB driver), FileGDB tests fail.
# Run it in isolation
export JAVA_HOME=/usr/lib/jvm/java-8-openjdk-amd64
export PATH=$JAVA_HOME/jre/bin:$PATH

cd gdal
# Perl unit tests
cd swig/perl
make test
cd ../..
# Java unit tests
cd swig/java
make test
cd ../..
# CPP unit tests
cd ../autotest
cd cpp
make quick_test
# Compile and test vsipreload
make vsipreload.so
LD_PRELOAD=./vsipreload.so gdalinfo /vsicurl/http://download.osgeo.org/gdal/data/ecw/spif83.ecw
LD_PRELOAD=./vsipreload.so sqlite3  /vsicurl/http://download.osgeo.org/gdal/data/sqlite3/polygon.db "select * from polygon limit 10"
cd ..
# Download a sample file
mkdir -p ogr/tmp/cache/
cd ogr/tmp/cache/
wget http://download.osgeo.org/gdal/data/pgeo/PGeoTest.zip
unzip PGeoTest.zip
cd ../../..

# install test dependencies
curl -sSL 'https://bootstrap.pypa.io/get-pip.py' | sudo python3
sudo -H pip3 install -U -r ./requirements.txt

PYTEST="python3 $(command -v pytest) -vv -p no:sugar --color=no"

# Run ogr_fgdb.py in isolation from the rest
$PYTEST ogr/ogr_fgdb.py
PYTESTARGS="--ignore ogr/ogr_fgdb.py"

# Run ogr_pgeo.py in isolation from the rest
# This crashes on Trusty since travis-ci upgraded their Trusty workers
#python ogr_pgeo.py
PYTESTARGS="$PYTESTARGS --ignore ogr/ogr_pgeo.py"

# Fails on test_validate_jp2_2 (errors not in expected order)
PYTESTARGS="$PYTESTARGS --ignore gdrivers/test_validate_jp2.py"

# Run all the Python autotests

GDAL_SKIP="JP2ECW ECW" $PYTEST $PYTESTARGS

#!/bin/bash
set -ex

mkdir -p build-rtd
cd build-rtd

PREFIX=${READTHEDOCS_VIRTUALENV_PATH}

cmake \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_INSTALL_PREFIX=${PREFIX} \
    -DCMAKE_INSTALL_RPATH=${PREFIX}/lib \
    -DGDAL_PYTHON_INSTALL_PREFIX=${PREFIX} \
    -DGDAL_BUILD_OPTIONAL_DRIVERS=OFF \
    -DOGR_BUILD_OPTIONAL_DRIVERS=OFF \
    -DGDAL_ENABLE_DRIVER_GTI=ON \
    -DOGR_ENABLE_DRIVER_CSV=ON \
    -DOGR_ENABLE_DRIVER_GPKG=ON \
    -DOGR_ENABLE_DRIVER_OPENFILEGDB=ON \
    -DBUILD_APPS=ON \
    -DBUILD_PYTHON_BINDINGS=ON \
    -DBUILD_JAVA_BINDINGS=ON \
    -DBUILD_TESTING=OFF \
    -DGDAL_JAVA_GENERATE_JAVADOC=ON \
    -DBASH_COMPLETIONS_DIR="" \
    ..

cmake --build . -j$(nproc)
cmake --build . --target doxygen_xml doxygen_html
cmake --install .

# set rpath for python libraries
find ${READTHEDOCS_VIRTUALENV_PATH} -wholename "*/osgeo/*.so" -exec patchelf --set-rpath ${READTHEDOCS_VIRTUALENV_PATH}/lib {} \;

# check python import (needed for doc build)
python3 -c "from osgeo import gdal; print(gdal.__version__)"

# unpack javadoc created during cmake --build into correct location
mkdir -p ../doc/build/html_extra
unzip -d ../doc/build/html_extra swig/java/javadoc.zip

# copy doxygen outputs into source tree
cp -r doc/build/xml ../doc/build
cp -r doc/build/html_extra/doxygen ../doc/build/html_extra

# copy gdalicon.png into html_extra (used by test suite)
cp ../data/gdalicon.png ../doc/build/html_extra/

# copy proj_list documentation
# see https://github.com/OSGeo/gdal/issues/8221
git clone https://github.com/OSGeo/libgeotiff
cp -r libgeotiff/geotiff/html/proj_list ../doc/build/html_extra

# copy resources directory for gdal2tiles usage
# see https://github.com/OSGeo/gdal/pull/6276
cp -r ../resources ../doc/build/html_extra/resources # Do not change this without changing swig/python/gdal-utils/osgeo_utils/gdal2tiles.py

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
    -DBUILD_APPS=OFF \
    -DBUILD_PYTHON_BINDINGS=ON \
    -DBUILD_JAVA_BINDINGS=ON \
    -DBUILD_TESTING=OFF \
    -DGDAL_JAVA_GENERATE_JAVADOC=ON \
    -DBASH_COMPLETIONS_DIR="" \
    ..

cmake --build . -j$(nproc)
cmake --install .

# set rpath for python libraries
find ${READTHEDOCS_VIRTUALENV_PATH} -wholename "*/osgeo/*.so" -exec patchelf --set-rpath ${READTHEDOCS_VIRTUALENV_PATH}/lib {} \;

# unpack javadoc created during cmake --build into correct location
mkdir -p ../doc/build/html_extra
unzip -d ../doc/build/html_extra swig/java/javadoc.zip

# check python import
python3 -c "from osgeo import gdal; print(gdal.__version__)"

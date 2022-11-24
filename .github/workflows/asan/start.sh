#!/bin/sh

set -e

apt-get update -y
DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    sudo wget tzdata

# Emulate 'sanitize' Travis-CI target for the purpose of test skipping
TRAVIS=yes
export TRAVIS
TRAVIS_BRANCH=sanitize
export TRAVIS_BRANCH

LANG=en_US.UTF-8
export LANG
apt-get install -y locales && \
    echo "$LANG UTF-8" > /etc/locale.gen && \
    dpkg-reconfigure --frontend=noninteractive locales && \
    update-locale LANG=$LANG

USER=root
export USER

cd "$WORK_DIR"

if test -f "$WORK_DIR/ccache.tar.gz"; then
    echo "Restoring ccache..."
    (cd $HOME && tar xzf "$WORK_DIR/ccache.tar.gz")
fi


sudo apt-get update
sudo apt-get install -y --allow-unauthenticated libpng-dev libjpeg-dev libgif-dev liblzma-dev libgeos-dev libcurl4-gnutls-dev libproj-dev libxml2-dev libexpat1-dev libxerces-c-dev libnetcdf-dev netcdf-bin libpoppler-dev libpoppler-private-dev libsqlite3-dev gpsbabel swig libhdf4-alt-dev libhdf5-dev libpodofo-dev poppler-utils libfreexl-dev unixodbc-dev libwebp-dev libepsilon-dev liblcms2-2 libpcre3-dev libcrypto++-dev libfyba-dev libmysqlclient-dev libogdi-dev libcfitsio-dev openjdk-8-jdk libzstd-dev ccache curl autoconf automake sqlite3 libspatialite-dev make g++ libssl-dev libsfcgal-dev libgeotiff-dev libopenjp2-7-dev libcairo2-dev python3-dev python3-setuptools python3-numpy python3-pip clang git cmake

# Workaround bug in ogdi packaging
sudo ln -s /usr/lib/ogdi/libvrf.so /usr/lib

# Build odbc-cpp library for HANA
(wget https://github.com/SAP/odbc-cpp-wrapper/archive/refs/tags/v1.1.tar.gz -O odbc-cpp-wrapper.tar.gz && mkdir odbc-cpp-wrapper && tar -xvf odbc-cpp-wrapper.tar.gz -C odbc-cpp-wrapper --strip-components=1 && mkdir odbc-cpp-wrapper/build && cd odbc-cpp-wrapper/build && cmake .. && make -j 2 && make install && cd ../.. && rm -rf odbc-cpp-wrapper)

wget https://github.com/Esri/file-geodatabase-api/raw/master/FileGDB_API_1.5/FileGDB_API_1_5_64gcc51.tar.gz
tar xzf FileGDB_API_1_5_64gcc51.tar.gz
sudo cp FileGDB_API-64gcc51/lib/* /usr/lib
sudo ldconfig

SCRIPT_DIR=$(dirname "$0")
case $SCRIPT_DIR in
    "/"*)
        ;;
    ".")
        SCRIPT_DIR=$(pwd)
        ;;
    *)
        SCRIPT_DIR=$(pwd)/$(dirname "$0")
        ;;
esac

export CCACHE_CPP2=yes
export CC="ccache clang"
export CXX="ccache clang++"

if [ "$NPROC" = "" ]; then
  NPROC=3
fi

ccache -M 1G
ccache -s

echo "#!/bin/sh" > mycc.sh
chmod +x mycc.sh
echo "$CC -fsanitize=undefined -fsanitize=address -shared-libasan \$*" >> mycc.sh

echo "#!/bin/sh" > mycxx.sh
chmod +x mycxx.sh
echo "$CXX -fsanitize=undefined -fsanitize=address -shared-libasan \$*" >> mycxx.sh

export CC=$PWD/mycc.sh
export CXX=$PWD/mycxx.sh

SANITIZE_FLAGS="-DMAKE_SANITIZE_HAPPY -fsanitize=unsigned-integer-overflow -fno-sanitize-recover=unsigned-integer-overflow"

mkdir build
cd build
export LDFLAGS="-fsanitize=undefined -fsanitize=address -shared-libasan -lstdc++"
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS="${SANITIZE_FLAGS} -Werror" -DCMAKE_CXX_FLAGS="${SANITIZE_FLAGS} -Werror" -DCMAKE_INSTALL_PREFIX=/usr -DGDAL_USE_GEOTIFF_INTERNAL=ON -DGDAL_USE_TIFF_INTERNAL=ON -DFileGDB_ROOT:PATH=$PWD/../FileGDB_API-64gcc51 -DFileGDB_LIBRARY=/usr/lib/libFileGDBAPI.so
make -j$NPROC

sudo rm -f /usr/lib/libgdal.so*
sudo make install
cd ..

sudo ldconfig

ccache -s

echo "Saving ccache..."
rm -f "$WORK_DIR/ccache.tar.gz"
(cd $HOME && tar czf "$WORK_DIR/ccache.tar.gz" .ccache)


export PRELOAD=$(clang -print-file-name=libclang_rt.asan-x86_64.so)

# install test dependencies
sudo python3 -m pip install -U -r autotest/requirements.txt
sudo python3 -m pip install -U hdbcli

cd build/autotest

# Don't run these
rm -f ogr/ogr_fgdb.py ogr/ogr_pgeo.py

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
export SKIP_MEM_INTENSIVE_TEST=YES
export SKIP_VIRTUALMEM=YES
export LD_PRELOAD=$PRELOAD
export ASAN_OPTIONS=allocator_may_return_null=1:symbolize=1:suppressions=$PWD/../../autotest/asan_suppressions.txt
export LSAN_OPTIONS=detect_leaks=1,print_suppressions=0,suppressions=$PWD/../../autotest/lsan_suppressions.txt

gdalinfo gcore/data/byte.tif
python3 -c "from osgeo import gdal; print('yes')"

echo "#!/bin/sh" > pytest_wrapper.sh
echo 'ARGS="$*"' >> pytest_wrapper.sh
echo "python3 -m pytest --capture=no -ra -vv -p no:sugar --color=no -o console_output_style=classic \${ARGS} 2>&1" >> pytest_wrapper.sh
cat pytest_wrapper.sh
chmod +x pytest_wrapper.sh

# Error on ogdi_5 test
rm ogr/ogr_ogdi.py

# new-delete-type-mismatch error in gpsbabel binary that we can't suppress
rm ogr/ogr_gpsbabel.py

find \
    ogr gcore gdrivers osr alg gnm utilities pyscripts \
    -name '*.py' ! -name netcdf_cfchecks.py ! -name "__init__.py" ! -path 'ogr/data/*' \
    -print \
    -exec ./pytest_wrapper.sh {} \; \
    | tee ./test-output.txt

# Check if the tests failed and error out.
if grep -P '===.*\d+ failed' ./test-output.txt > /dev/null ; then
    echo 'Tests failed'
    exit 1
elif grep '==ABORTING' ./test-output.txt; then
    echo 'Tests crashed'
    exit 1
else
    echo 'Tests passed'
fi

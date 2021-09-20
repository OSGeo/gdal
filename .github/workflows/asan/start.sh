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


sudo apt-get install -y software-properties-common python-software-properties
sudo add-apt-repository -y ppa:ubuntugis/ubuntugis-unstable
sudo add-apt-repository -y ppa:deadsnakes/ppa
sudo apt-get update
sudo apt-get install -y python3.6 python3.6-dev
sudo apt-get install -y --allow-unauthenticated libpng12-dev libjpeg-dev libgif-dev liblzma-dev libgeos-dev libcurl4-gnutls-dev libproj-dev libxml2-dev libexpat-dev libxerces-c-dev libnetcdf-dev netcdf-bin libpoppler-dev libpoppler-private-dev libsqlite3-dev gpsbabel swig libhdf4-alt-dev libhdf5-serial-dev libpodofo-dev poppler-utils libfreexl-dev unixodbc-dev libwebp-dev libepsilon-dev liblcms2-2 libpcre3-dev libcrypto++-dev libdap-dev libfyba-dev libmysqlclient-dev libogdi3.2-dev libcfitsio-dev openjdk-8-jdk couchdb libzstd1-dev ccache curl autoconf automake sqlite3 libspatialite-dev make g++ libssl-dev libsfcgal-dev libgeotiff-dev libcharls-dev libopenjp2-7-dev libcairo2-dev

# get-pip.py will install setuptools (the python3.6 package from the deadsnakes/ppa doesn't include pip or setuptools)
curl -sSL https://bootstrap.pypa.io/get-pip.py -o get-pip.py
python3.6 get-pip.py

wget https://github.com/Esri/file-geodatabase-api/raw/master/FileGDB_API_1.5/FileGDB_API_1_5_64gcc51.tar.gz
tar xzf FileGDB_API_1_5_64gcc51.tar.gz
sudo cp FileGDB_API-64gcc51/lib/* /usr/lib
sudo ldconfig

FILE=clang+llvm-9.0.0-x86_64-linux-gnu-ubuntu-16.04.tar.xz
URL_ROOT=https://github.com/rouault/gdal_ci_tools/raw/master/${FILE}
curl -Ls ${URL_ROOT}aa ${URL_ROOT}ab ${URL_ROOT}ac ${URL_ROOT}ad ${URL_ROOT}ae | tar xJf -


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
$SCRIPT_DIR/../common_install.sh

export ASAN_OPTIONS=allocator_may_return_null=1

export CCACHE_CPP2=yes
export CC="ccache $PWD/clang+llvm-9.0.0-x86_64-linux-gnu-ubuntu-16.04/bin/clang"
export CXX="ccache $PWD/clang+llvm-9.0.0-x86_64-linux-gnu-ubuntu-16.04/bin/clang++"

ccache -M 1G
ccache -s

# Build proj
(cd proj;  ./autogen.sh && CFLAGS='-DPROJ_RENAME_SYMBOLS' CXXFLAGS='-DPROJ_RENAME_SYMBOLS' ./configure --disable-static --prefix=/usr/local && make -j3)
(cd proj; sudo make -j3 install)
sudo sh -c "apt-get remove -y libproj-dev"

export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

CURRENT_DIR=$PWD
cd gdal

./autogen.sh
SANITIZE_FLAGS="-DMAKE_SANITIZE_HAPPY -fsanitize=undefined -fsanitize=address -fsanitize=unsigned-integer-overflow"
CFLAGS=$SANITIZE_FLAGS CXXFLAGS=$SANITIZE_FLAGS LDFLAGS="-fsanitize=undefined -fsanitize=address -lstdc++" ./configure --prefix=/usr --without-libtool --enable-debug --with-jpeg12 --with-poppler --without-podofo --with-spatialite --with-mysql --with-liblzma --with-webp --with-epsilon --with-libtiff=internal --with-rename-internal-libtiff-symbols --with-hide-internal-symbols --with-gnm --with-proj=/usr/local --with-fgdb=$PWD/../FileGDB_API-64gcc51
sed -i "s/-fsanitize=address/-fsanitize=address -shared-libasan/g" GDALmake.opt
sed -i "s/-fsanitize=unsigned-integer-overflow/-fsanitize=unsigned-integer-overflow -fno-sanitize-recover=unsigned-integer-overflow/g" GDALmake.opt
make USER_DEFS="-Werror" -j3
(cd apps && make USER_DEFS="-Werror" test_ogrsf)
(cd swig/python && \
    echo "#!/bin/sh" > mycc.sh && \
    echo "$CC -fsanitize=undefined -fsanitize=address -shared-libasan \$*" >> mycc.sh && \
    cat mycc.sh && \
    chmod +x mycc.sh && \
    PATH=$PWD:$PATH CC=mycc.sh python3.6 setup.py build
)

sudo rm -f /usr/lib/libgdal.so*
sudo make install
curl -sSL 'https://bootstrap.pypa.io/get-pip.py' | sudo python3.6
sudo python3.6 -m pip install numpy
(cd swig/python && sudo python3.6 setup.py install)

sudo ldconfig

ccache -s

cd "$CURRENT_DIR"

echo "Saving ccache..."
rm -f "$WORK_DIR/ccache.tar.gz"
(cd $HOME && tar czf "$WORK_DIR/ccache.tar.gz" .ccache)


export PRELOAD=$PWD/clang+llvm-9.0.0-x86_64-linux-gnu-ubuntu-16.04/lib/clang/9.0.0/lib/linux/libclang_rt.asan-x86_64.so

cd autotest

# Don't run these
rm -f ogr/ogr_fgdb.py ogr/ogr_pgeo.py

# Too old spatialite version
rm -f ogr/ogr_sqlite.py gdrivers/rasterlite.py

# install test dependencies
sudo python3.6 -m pip install -U -r ./requirements.txt

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
    LSAN_OPTIONS=detect_leaks=1,print_suppressions=0,suppressions=$PWD/asan_suppressions.txt

gdalinfo gcore/data/byte.tif
python3.6 -c "from osgeo import gdal; print('yes')"

echo "#!/bin/sh" > pytest_wrapper.sh
echo 'ARGS="$*"' >> pytest_wrapper.sh
echo "python3.6 -m pytest --capture=no -ra -vv -p no:sugar --color=no -o console_output_style=classic \${ARGS} 2>&1" >> pytest_wrapper.sh
cat pytest_wrapper.sh
chmod +x pytest_wrapper.sh

# Error on ogdi_5 test
rm ogr/ogr_ogdi.py

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

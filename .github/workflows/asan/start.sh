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
sudo apt-get install -y --allow-unauthenticated libpng-dev libjpeg-dev libgif-dev liblzma-dev libgeos-dev libcurl4-gnutls-dev libproj-dev libxml2-dev libexpat1-dev libxerces-c-dev libnetcdf-dev netcdf-bin libpoppler-dev libpoppler-private-dev libsqlite3-dev gpsbabel swig libhdf4-alt-dev libhdf5-dev libpodofo-dev poppler-utils libfreexl-dev unixodbc-dev libwebp-dev libepsilon-dev liblcms2-2 libpcre3-dev libcrypto++-dev libdap-dev libfyba-dev libmysqlclient-dev libogdi-dev libcfitsio-dev openjdk-8-jdk libzstd-dev ccache curl autoconf automake sqlite3 libspatialite-dev make g++ libssl-dev libsfcgal-dev libgeotiff-dev libcharls-dev libopenjp2-7-dev libcairo2-dev python3-dev python3-setuptools python3-numpy python3-pip clang

# Workaround bug in ogdi packaging
sudo ln -s /usr/lib/ogdi/libvrf.so /usr/lib

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

./autogen.sh
SANITIZE_FLAGS="-DMAKE_SANITIZE_HAPPY -fsanitize=undefined -fsanitize=address -fsanitize=unsigned-integer-overflow"
CFLAGS=$SANITIZE_FLAGS CXXFLAGS=$SANITIZE_FLAGS LDFLAGS="-fsanitize=undefined -fsanitize=address -lstdc++" ./configure --prefix=/usr --without-libtool --enable-debug --with-jpeg12 --with-poppler --without-podofo --with-spatialite --with-mysql --with-liblzma --with-webp --with-epsilon --with-libtiff=internal --with-rename-internal-libtiff-symbols --with-hide-internal-symbols --with-gnm --with-fgdb=$PWD/FileGDB_API-64gcc51
sed -i "s/-fsanitize=address/-fsanitize=address -shared-libasan/g" GDALmake.opt
sed -i "s/-fsanitize=unsigned-integer-overflow/-fsanitize=unsigned-integer-overflow -fno-sanitize-recover=unsigned-integer-overflow/g" GDALmake.opt
make USER_DEFS="-Werror" -j$NPROC
(cd apps && make USER_DEFS="-Werror" test_ogrsf)
(cd swig/python && \
    echo "#!/bin/sh" > mycc.sh && \
    echo "$CC -fsanitize=undefined -fsanitize=address -shared-libasan \$*" >> mycc.sh && \
    cat mycc.sh && \
    chmod +x mycc.sh && \
    PATH=$PWD:$PATH CC=mycc.sh python3 setup.py build
)

sudo rm -f /usr/lib/libgdal.so*
sudo make install
(cd swig/python && sudo python3 setup.py install)

sudo ldconfig

ccache -s

echo "Saving ccache..."
rm -f "$WORK_DIR/ccache.tar.gz"
(cd $HOME && tar czf "$WORK_DIR/ccache.tar.gz" .ccache)


export PRELOAD=$(clang -print-file-name=libclang_rt.asan-x86_64.so)

cd autotest

# Don't run these
rm -f ogr/ogr_fgdb.py ogr/ogr_pgeo.py

# install test dependencies
sudo python3 -m pip install -U -r ./requirements.txt

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
export ASAN_OPTIONS=allocator_may_return_null=1:symbolize=1:suppressions=$PWD/asan_suppressions.txt
export LSAN_OPTIONS=detect_leaks=1,print_suppressions=0,suppressions=$PWD/lsan_suppressions.txt

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

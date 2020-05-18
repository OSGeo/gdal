#!/bin/bash

set -e

export PRELOAD=$PWD/clang+llvm-9.0.0-x86_64-linux-gnu-ubuntu-16.04/lib/clang/9.0.0/lib/linux/libclang_rt.asan-x86_64.so

cd gdal

cd ../autotest

# Don't run these
rm -f ogr/ogr_fgdb.py ogr/ogr_pgeo.py

# Too old spatialite version
rm -f ogr/ogr_sqlite.py gdrivers/rasterlite.py

# install test dependencies
sudo -H pip install -U pip
sudo -H pip install -U -r ./requirements.txt
sudo apt-get remove -y python-numpy
sudo -H pip install -U numpy

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
python -c "from osgeo import gdal; print('yes')"

echo "#!/bin/sh" > pytest_wrapper.sh
echo 'ARGS="$*"' >> pytest_wrapper.sh
echo "pytest --capture=no -ra -vv -p no:sugar --color=no -o console_output_style=classic \${ARGS} 2>&1" >> pytest_wrapper.sh
cat pytest_wrapper.sh
chmod +x pytest_wrapper.sh

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

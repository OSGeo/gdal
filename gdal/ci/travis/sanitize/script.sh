#!/bin/bash

set -e

export PYTEST="pytest -vv -p no:sugar --color=no"
export PRELOAD=/usr/lib/gcc/x86_64-linux-gnu/5/libasan.so:/usr/lib/gcc/x86_64-linux-gnu/5/libubsan.so

cd gdal

cd ../autotest

# Don't run these
rm ogr/ogr_fgdb.py ogr/ogr_pgeo.py

# Too old spatialite version
rm ogr/ogr_sqlite.py gdrivers/rasterlite.py

# install test dependencies
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

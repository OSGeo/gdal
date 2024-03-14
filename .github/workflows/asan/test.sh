#!/bin/bash

set -ex

. ../scripts/setdevenv.sh

export LD_LIBRARY_PATH=/usr/lib/llvm-14/lib/clang/14.0.0/lib/linux:${LD_LIBRARY_PATH}
export PATH=/usr/lib/llvm-14/bin:${PATH}
export SKIP_MEM_INTENSIVE_TEST=YES
export SKIP_VIRTUALMEM=YES
export LD_PRELOAD=$(clang -print-file-name=libclang_rt.asan-x86_64.so)
export ASAN_OPTIONS=allocator_may_return_null=1:symbolize=1:suppressions=$PWD/../autotest/asan_suppressions.txt
export LSAN_OPTIONS=detect_leaks=1,print_suppressions=0,suppressions=$PWD/../autotest/lsan_suppressions.txt
export PYTHONMALLOC=malloc

gdalinfo autotest/gcore/data/byte.tif
python3 -c "from osgeo import gdal; print('yes')"

cd autotest

# Run each module in its own pytest process.
# This makes sure the output from the address sanitizer is relevant
# and it doesn't blow out RAM too much.
# Unfortunately it's also a reasonably large slowdown since we have to wait
# for a python interpreter and all modules to load between each module.
# (and add a grep to get rid of the extra pytest header headers/etc)

echo "#!/bin/sh" > pytest_wrapper.sh
echo 'ARGS="$*"' >> pytest_wrapper.sh
echo "python3 -m pytest --capture=no -ra -vv -p no:sugar --color=no -o console_output_style=classic \${ARGS} 2>&1" >> pytest_wrapper.sh
cat pytest_wrapper.sh
chmod +x pytest_wrapper.sh

# NOTE: `find ... -exec` always exits with 0 even when the tests failed.
# That turns out to be what we want here though, since we want
# to not fail when the address sanitizer finds errors.
# So we tee the output to a file and grep it to discover if the tests failed.
find -L \
    ogr gcore gdrivers osr alg gnm utilities pyscripts \
    -name '*.py' \
        ! -name netcdf_cfchecks.py \
        ! -name ogr_fgdb.py `# Don't run these` \
        ! -name ogr_pgeo.py `# Don't run these` \
        ! -name ogr_ogdi.py `# Error on ogdi_5 test` \
        ! -name ogr_gpsbabel.py `# new-delete-type-mismatch error in gpsbabel binary that we can't suppress` \
        ! -name "__init__.py" \
        ! -path 'ogr/data/*' \
        ! -name test_gdal_merge.py \
        ! -name test_gdal_retile.py \
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

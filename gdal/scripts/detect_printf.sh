#!/bin/bash

set -eu

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
GDAL_ROOT=$SCRIPT_DIR/..
cd "$GDAL_ROOT"


ret_code=0

echo "Checking for printf() statements..."
# apps is voluntarily missing in the list of directories, due to lots of legitimate uses of such statements in programs
if grep -r --include="*.c*" " printf" alg gnm port ogr gcore frmts | grep -v -e "/*ok" -e "printf()" -e "printf-like" -e "printf style" -e "with printf"  -e "/\\* printf" -e "//" -e degrib -e aitest -e dted_test -e giflib -e 8211view -e 8211dump -e timetest -e 8211createfromxml -e hfatest -e zlib -e libtiff -e envisat_dump -e dumpgeo -e nitfdump -e ceostest -e libpng -e generate_encoding -e capi_test -e ntfdump -e test_load_virtual_ogr -e ocitest -e fastload -e s57dump -e dgndump -e test_geo_utils -e testparser -e internal_libqhull ; then
    echo "FAIL: suspicious printf found. Remove or tag it with /*ok*/"
    ret_code=1
else
    echo "OK: no suspicious printf found."
fi


echo "Checking for fprintf(stderr,) statements..."
# apps is voluntarily missing in the list of directories, due to lots of legitimate uses of such statements in programs
if grep fprintf -r alg gnm port ogr gcore frmts --include="*.cpp" | grep stderr | grep -v -G -e "/[/|*][ ]*fprintf" -e "/*ok" -e sdts2shp -e degrib  -e 8211view -e 8211createfromxml -e 8211dump -e pcidskexception -e vsipreload -e fprintfstderr -e cpl_multiproc -e "truncation occurred" -e xmlreformat -e cpl_error ; then
    echo "FAIL: suspicious fprintf(stder,...) found. Remove or tag it with /*ok*/"
    ret_code=1
else
    echo "OK: no suspicious fprintf(stder,...) found."
fi

exit $ret_code

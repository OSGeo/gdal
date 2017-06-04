#!/bin/bash

ret_code=0

echo "Checking for printf() statements..."
# apps is voluntarily missing in the list of directories, due to lots of legitimate uses of such statements in programs
grep -r --include="*.c*" " printf" alg gnm port ogr gcore frmts | grep -v "/*ok" | grep -v "printf()" | grep -v "printf-like" | grep -v "printf style" | grep -v "with printf"  | grep -v "/\* printf" |  grep -v "//" | grep -v degrib | grep -v aitest | grep -v dted_test | grep -v giflib | grep -v 8211view | grep -v 8211dump | grep -v timetest | grep -v 8211createfromxml | grep -v hfatest | grep -v zlib | grep -v libtiff | grep -v envisat_dump | grep -v dumpgeo | grep -v nitfdump | grep -v ceostest| grep -v libpng | grep -v generate_encoding | grep -v capi_test | grep -v ntfdump | grep -v test_load_virtual_ogr | grep -v ocitest | grep -v fastload | grep -v s57dump | grep -v dgndump | grep -v test_geo_utils | grep -v testparser | grep -v internal_libqhull

if [[ $? -eq 0 ]] ; then
    echo "FAIL: suspicious printf found. Remove or tag it with /*ok*/"
    ret_code=1
else
    echo "OK: no suspicious printf found."
fi


echo "Checking for fprintf(stderr,) statements..."
# apps is voluntarily missing in the list of directories, due to lots of legitimate uses of such statements in programs
grep fprintf -r  alg gnm port ogr gcore frmts --include="*.cpp" | grep stderr | grep -v -G "/[/|*][ ]*fprintf" | grep -v "/*ok" | grep -v sdts2shp | grep -v degrib18  | grep -v 8211view | grep -v 8211createfromxml | grep -v 8211dump | grep -v pcidskexception | grep -v vsipreload | grep -v fprintfstderr | grep -v cpl_multiproc | grep -v "truncation occurred" | grep -v xmlreformat | grep -v cpl_error

if [[ $? -eq 0 ]] ; then
    echo "FAIL: suspicious fprintf(stder,...) found. Remove or tag it with /*ok*/"
    ret_code=1
else
    echo "OK: no suspicious fprintf(stder,...) found."
fi

exit $ret_code

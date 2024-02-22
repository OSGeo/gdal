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

find alg port gcore apps ogr frmts gnm autotest/cpp \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) > /tmp/gdal_list_files.txt

echo "Checking for missing #include <algorithm> statements..."
rm -f /tmp/missing_include.txt
while read -r i; do
   grep -e std::min -e std::max $i >/dev/null && (grep "#include <algorithm>" $i >/dev/null || echo $i) | grep -v ogr/ogrsf_frmts/flatgeobuf/flatbuffers/ | tee -a /tmp/missing_include.txt;
done < /tmp/gdal_list_files.txt

if test -s /tmp/missing_include.txt; then
    echo "FAIL: missing #include <algorithm> in above listed files"
    ret_code=1
else
    echo "OK."
fi


echo "Checking for missing #include <limits> statements..."
rm -f /tmp/missing_include.txt
while read -r i; do
   grep -e std::numeric_limits $i >/dev/null && (grep "#include <limits>" $i >/dev/null || echo $i) | grep -v ogr/ogrsf_frmts/flatgeobuf/flatbuffers/ | tee -a /tmp/missing_include.txt;
done < /tmp/gdal_list_files.txt


if test -s /tmp/missing_include.txt; then
    echo "FAIL: missing #include <limits> in above listed files"
    ret_code=1
else
    echo "OK."
fi


echo "Checking for missing #include <cctype> statements..."
rm -f /tmp/missing_include.txt
while read -r i; do
   grep -e std::isalpha $i >/dev/null && (grep "#include <cctype>" $i >/dev/null || echo $i) | tee -a /tmp/missing_include.txt;
done < /tmp/gdal_list_files.txt


if test -s /tmp/missing_include.txt; then
    echo "FAIL: missing #include <cctype> in above listed files"
    ret_code=1
else
    echo "OK."
fi

rm -f /tmp/missing_include.txt
rm -f /tmp/gdal_list_files.txt

exit $ret_code

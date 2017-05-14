#!/bin/bash

if [ "$SRC" == "" ]; then
    echo "SRC env var not defined"
    exit 1
fi

if [ "$OUT" == "" ]; then
    echo "OUT env var not defined"
    exit 1
fi

if [ "$CXX" == "" ]; then
    echo "CXX env var not defined"
    exit 1
fi

SRC_DIR=$(dirname $0)/..

formats="GTiff HFA"
for format in $formats; do
    format_lc=$(echo $format | tr '[:upper:]' '[:lower:]')
    fuzzerName="${format_lc}_fuzzer"
    echo "Building fuzzer $fuzzerName"
    $CXX $CXXFLAGS -std=c++11 -I$SRC_DIR/port -I$SRC_DIR/gcore -I$SRC_DIR/alg -I$SRC_DIR/ogr -I$SRC_DIR/ogr/ogrsf_frmts \
        $(dirname $0)/gdal_fuzzer.cpp -DREGISTER_FUNC=GDALRegister_$format -o $OUT/$fuzzerName \
        -lFuzzingEngine $SRC_DIR/libgdal.a $SRC/install/lib/*.a
done

fuzzerFiles=$(find $(dirname $0) -name "*.cpp")
for F in $fuzzerFiles; do
    fuzzerName=$(basename $F .cpp)
    echo "Building fuzzer $fuzzerName"
    if test -d $SRC/install/lib; then
        $CXX $CXXFLAGS -std=c++11 -I$SRC_DIR/port -I$SRC_DIR/gcore -I$SRC_DIR/alg -I$SRC_DIR/ogr -I$SRC_DIR/ogr/ogrsf_frmts \
            $F -o $OUT/$fuzzerName \
            -lFuzzingEngine $SRC_DIR/libgdal.a $SRC/install/lib/*.a
    else
        $CXX $CXXFLAGS -std=c++11 -I$SRC_DIR/port -I$SRC_DIR/gcore -I$SRC_DIR/alg -I$SRC_DIR/ogr -I$SRC_DIR/ogr/ogrsf_frmts \
            $F -o $OUT/$fuzzerName \
            -lFuzzingEngine $SRC_DIR/libgdal.a
    fi
done

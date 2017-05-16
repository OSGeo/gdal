#!/bin/bash

set -e

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

build_fuzzer()
{
    fuzzerName=$1
    sourceFilename=$2
    shift
    shift
    echo "Building fuzzer $fuzzerName"
    if test -d $SRC/install/lib; then
        $CXX $CXXFLAGS -std=c++11 -I$SRC_DIR/port -I$SRC_DIR/gcore -I$SRC_DIR/alg -I$SRC_DIR/ogr -I$SRC_DIR/ogr/ogrsf_frmts \
            $sourceFilename $* -o $OUT/$fuzzerName \
            -lFuzzingEngine $SRC_DIR/libgdal.a $EXTRA_LIBS $SRC/install/lib/*.a
    else
        $CXX $CXXFLAGS -std=c++11 -I$SRC_DIR/port -I$SRC_DIR/gcore -I$SRC_DIR/alg -I$SRC_DIR/ogr -I$SRC_DIR/ogr/ogrsf_frmts \
            $sourceFilename $* -o $OUT/$fuzzerName \
            -lFuzzingEngine $SRC_DIR/libgdal.a $EXTRA_LIBS
    fi
}

build_ogr_specialized_fuzzer()
{
    format=$1
    registerFunc=$2
    memFilename=$3
    gdalFilename=$4
    fuzzerName="${format}_fuzzer"
    build_fuzzer $fuzzerName $(dirname $0)/ogr_fuzzer.cpp -DREGISTER_FUNC=$registerFunc -DMEM_FILENAME="\"$memFilename\"" -DGDAL_FILENAME="\"$gdalFilename\""
}

build_gdal_specialized_fuzzer()
{
    format=$1
    registerFunc=$2
    memFilename=$3
    gdalFilename=$4
    fuzzerName="${format}_fuzzer"
    build_fuzzer $fuzzerName $(dirname $0)/gdal_fuzzer.cpp -DREGISTER_FUNC=$registerFunc -DMEM_FILENAME="\"$memFilename\"" -DGDAL_FILENAME="\"$gdalFilename\""
}

build_ogr_specialized_fuzzer openfilegdb RegisterOGROpenFileGDB "/vsimem/test.gdb.tar" "/vsimem/test.gdb.tar"
build_ogr_specialized_fuzzer shape OGRRegisterAll "/vsimem/test.tar" "/vsitar//vsimem/test.tar/my.shp"
build_ogr_specialized_fuzzer mitab_mif OGRRegisterAll "/vsimem/test.tar" "/vsitar//vsimem/test.tar/my.mif"
build_ogr_specialized_fuzzer mitab_tab OGRRegisterAll "/vsimem/test.tar" "/vsitar//vsimem/test.tar/my.tab"

formats="GTiff HFA"
for format in $formats; do
    format_lc=$(echo $format | tr '[:upper:]' '[:lower:]')
    fuzzerName="${format_lc}_fuzzer"
    build_gdal_specialized_fuzzer $fuzzerName "GDALRegister_$format" "/vsimem/test" "/vsimem/test"
done

fuzzerFiles=$(dirname $0)/*.cpp
for F in $fuzzerFiles; do
    fuzzerName=$(basename $F .cpp)
    build_fuzzer $fuzzerName $F
done

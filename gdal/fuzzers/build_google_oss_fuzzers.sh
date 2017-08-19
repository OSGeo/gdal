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
        $CXX $CXXFLAGS -std=c++11 -I$SRC_DIR/port -I$SRC_DIR/gcore -I$SRC_DIR/alg -I$SRC_DIR/apps -I$SRC_DIR/ogr -I$SRC_DIR/ogr/ogrsf_frmts -I$SRC_DIR/ogr/ogrsf_frmts/sqlite \
            $sourceFilename $* -o $OUT/$fuzzerName \
            -lFuzzingEngine $SRC_DIR/libgdal.a $EXTRA_LIBS $SRC/install/lib/*.a
    else
        $CXX $CXXFLAGS -std=c++11 -I$SRC_DIR/port -I$SRC_DIR/gcore -I$SRC_DIR/alg -I$SRC_DIR/apps -I$SRC_DIR/ogr -I$SRC_DIR/ogr/ogrsf_frmts -I$SRC_DIR/ogr/ogrsf_frmts/sqlite \
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
    build_fuzzer $fuzzerName $(dirname $0)/ogr_fuzzer.cpp -DREGISTER_FUNC=$registerFunc -DMEM_FILENAME="\"$memFilename\"" -DGDAL_FILENAME="\"$gdalFilename\"" -DOGR_SKIP="\"CAD\""
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

build_fuzzer gtiff_mmap $(dirname $0)/gdal_fuzzer.cpp -DREGISTER_FUNC=GDALRegister_GTiff -DGTIFF_USE_MMAP

fuzzerFiles=$(dirname $0)/*.cpp
for F in $fuzzerFiles; do
    fuzzerName=$(basename $F .cpp)
    build_fuzzer $fuzzerName $F
done

build_ogr_specialized_fuzzer ogr_sdts RegisterOGRSDTS "/vsimem/test.tar" "/vsitar//vsimem/test.tar/TR01CATD.DDF"
build_ogr_specialized_fuzzer openfilegdb RegisterOGROpenFileGDB "/vsimem/test.gdb.tar" "/vsimem/test.gdb.tar"
build_ogr_specialized_fuzzer shape OGRRegisterAll "/vsimem/test.tar" "/vsitar//vsimem/test.tar/my.shp"
build_ogr_specialized_fuzzer mitab_mif OGRRegisterAll "/vsimem/test.tar" "/vsitar//vsimem/test.tar/my.mif"
build_ogr_specialized_fuzzer mitab_tab OGRRegisterAll "/vsimem/test.tar" "/vsitar//vsimem/test.tar/my.tab"
build_ogr_specialized_fuzzer csv RegisterOGRCSV "/vsimem/test.csv" "/vsimem/test.csv"
build_ogr_specialized_fuzzer bna RegisterOGRBNA "/vsimem/test.bna" "/vsimem/test.bna"
build_ogr_specialized_fuzzer wasp RegisterOGRWAsP "/vsimem/test.map" "/vsimem/test.map"
build_ogr_specialized_fuzzer xlsx RegisterOGRXLSX "/vsimem/test.xlsx" "/vsitar/{/vsimem/test.xlsx}"
build_ogr_specialized_fuzzer ods RegisterOGRODS "/vsimem/test.ods" "/vsitar/{/vsimem/test.ods}"
build_fuzzer cad_fuzzer $(dirname $0)/ogr_fuzzer.cpp -DREGISTER_FUNC=RegisterOGRCAD
build_fuzzer rec_fuzzer $(dirname $0)/ogr_fuzzer.cpp -DREGISTER_FUNC=RegisterOGRREC -DUSE_FILESYSTEM -DEXTENSION="\"rec\""

formats="GTiff HFA"
for format in $formats; do
    fuzzerName=$(echo $format | tr '[:upper:]' '[:lower:]')
    build_gdal_specialized_fuzzer $fuzzerName "GDALRegister_$format" "/vsimem/test" "/vsimem/test"
done
build_gdal_specialized_fuzzer adrg GDALRegister_ADRG  "/vsimem/test.tar" "/vsitar//vsimem/test.tar/ABCDEF01.GEN"
build_gdal_specialized_fuzzer srp GDALRegister_SRP "/vsimem/test.tar" "/vsitar//vsimem/test.tar/FKUSRP01.IMG"
build_gdal_specialized_fuzzer envi GDALRegister_ENVI "/vsimem/test.tar" "/vsitar//vsimem/test.tar/my.dat"
build_gdal_specialized_fuzzer aig GDALRegister_AIGrid "/vsimem/test.tar" "/vsitar//vsimem/test.tar/hdr.adf"
# mrf can use indirectly the GTiff driver
build_gdal_specialized_fuzzer mrf "GDALRegister_mrf();GDALRegister_GTiff" "/vsimem/test.tar" "/vsitar//vsimem/test.tar/byte.mrf"
build_gdal_specialized_fuzzer gdal_sdts GDALRegister_SDTS "/vsimem/test.tar" "/vsitar//vsimem/test.tar/1107CATD.DDF"

build_fuzzer gdal_filesystem_fuzzer $(dirname $0)/gdal_fuzzer.cpp -DUSE_FILESYSTEM
build_fuzzer ogr_filesystem_fuzzer $(dirname $0)/ogr_fuzzer.cpp -DUSE_FILESYSTEM

echo "[libfuzzer]" > $OUT/wkb_import_fuzzer.options
echo "max_len = 100000" >> $OUT/wkb_import_fuzzer.options

echo "[libfuzzer]" > $OUT/wkt_import_fuzzer.options
echo "max_len = 100000" >> $OUT/wkt_import_fuzzer.options

echo "[libfuzzer]" > $OUT/gml_geom_import_fuzzer.options
echo "max_len = 100000" >> $OUT/gml_geom_import_fuzzer.options

echo "[libfuzzer]" > $OUT/spatialite_geom_import_fuzzer.options
echo "max_len = 100000" >> $OUT/spatialite_geom_import_fuzzer.options

echo "[libfuzzer]" > $OUT/osr_set_from_user_input_fuzzer.options
echo "max_len = 10000" >> $OUT/osr_set_from_user_input_fuzzer.options


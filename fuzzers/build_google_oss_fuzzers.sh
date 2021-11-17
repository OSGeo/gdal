#!/bin/bash
# WARNING: this script is used by https://github.com/google/oss-fuzz/blob/master/projects/gdal/build.sh
# and should not be renamed or moved without updating the above

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

if [ "$LIB_FUZZING_ENGINE" = "" ]; then
    export LIB_FUZZING_ENGINE=-lFuzzingEngine
fi

SRC_DIR=$(dirname $0)/..

if [ "$LIBGDAL" = "" ]; then
  LIBGDAL="$SRC_DIR/libgdal.a"
fi

build_fuzzer()
{
    fuzzerName=$1
    sourceFilename=$2
    shift
    shift
    echo "Building fuzzer $fuzzerName"
    if test -d $SRC/install/lib; then
        $CXX $CXXFLAGS -I$SRC_DIR/port -I$SRC_DIR/generated_headers -I$SRC_DIR/gcore -I$SRC_DIR/alg -I$SRC_DIR/apps -I$SRC_DIR/ogr -I$SRC_DIR/ogr/ogrsf_frmts -I$SRC_DIR/ogr/ogrsf_frmts/sqlite \
            $sourceFilename "$@" -o $OUT/$fuzzerName \
            $LIB_FUZZING_ENGINE $LIBGDAL $EXTRA_LIBS $SRC/install/lib/*.a
    else
        $CXX $CXXFLAGS -I$SRC_DIR/port -I$SRC_DIR/generated_headers -I$SRC_DIR/gcore -I$SRC_DIR/alg -I$SRC_DIR/apps -I$SRC_DIR/ogr -I$SRC_DIR/ogr/ogrsf_frmts -I$SRC_DIR/ogr/ogrsf_frmts/sqlite \
            $sourceFilename "$@" -o $OUT/$fuzzerName \
            $LIB_FUZZING_ENGINE $LIBGDAL $EXTRA_LIBS
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

fuzzerFiles="$(dirname $0)/*.cpp"
for F in $fuzzerFiles; do
    if test $F != "$(dirname $0)/fuzzingengine.cpp"; then
        fuzzerName=$(basename $F .cpp)
        build_fuzzer $fuzzerName $F
    fi
done

build_ogr_specialized_fuzzer dxf RegisterOGRDXF "/vsimem/test" "/vsimem/test"
build_ogr_specialized_fuzzer ogr_sdts RegisterOGRSDTS "/vsimem/test.tar" "/vsitar//vsimem/test.tar/TR01CATD.DDF"
build_ogr_specialized_fuzzer openfilegdb RegisterOGROpenFileGDB "/vsimem/test.gdb.tar" "/vsimem/test.gdb.tar"
build_ogr_specialized_fuzzer shape OGRRegisterAll "/vsimem/test.tar" "/vsitar//vsimem/test.tar/my.shp"
build_ogr_specialized_fuzzer mitab_mif OGRRegisterAll "/vsimem/test.tar" "/vsitar//vsimem/test.tar/my.mif"
build_ogr_specialized_fuzzer mitab_tab OGRRegisterAll "/vsimem/test.tar" "/vsitar//vsimem/test.tar/my.tab"
build_ogr_specialized_fuzzer csv RegisterOGRCSV "/vsimem/test.csv" "/vsimem/test.csv"
build_ogr_specialized_fuzzer wasp RegisterOGRWAsP "/vsimem/test.map" "/vsimem/test.map"
build_ogr_specialized_fuzzer xlsx RegisterOGRXLSX "/vsimem/test.xlsx" "/vsitar/{/vsimem/test.xlsx}"
build_ogr_specialized_fuzzer ods RegisterOGRODS "/vsimem/test.ods" "/vsitar/{/vsimem/test.ods}"
build_ogr_specialized_fuzzer lvbag RegisterOGRLVBAG "/vsimem/test.xml" "/vsimem/test.xml"
build_ogr_specialized_fuzzer avce00 RegisterOGRAVCE00 "/vsimem/test.e00" "/vsimem/test.e00"
build_ogr_specialized_fuzzer avcbin RegisterOGRAVCBin "/vsimem/test.tar" "/vsitar/{/vsimem/test.tar}/testavc"
build_ogr_specialized_fuzzer gml RegisterOGRGML "/vsimem/test.tar" "/vsitar//vsimem/test.tar/test.gml"
build_ogr_specialized_fuzzer gmlas RegisterOGRGMLAS "/vsimem/test.tar" "GMLAS:/vsitar//vsimem/test.tar/test.gml"
build_ogr_specialized_fuzzer fgb RegisterOGRFlatGeobuf "/vsimem/test.fgb" "/vsimem/test.fgb"
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
build_gdal_specialized_fuzzer ehdr GDALRegister_EHdr "/vsimem/test.tar" "/vsitar//vsimem/test.tar/my.dat"
build_gdal_specialized_fuzzer genbin GDALRegister_GenBin "/vsimem/test.tar" "/vsitar//vsimem/test.tar/my.bil"
build_gdal_specialized_fuzzer isce GDALRegister_ISCE "/vsimem/test.tar" "/vsitar//vsimem/test.tar/isce.slc"
build_gdal_specialized_fuzzer roipac GDALRegister_ROIPAC "/vsimem/test.tar" "/vsitar//vsimem/test.tar/srtm.dem"
build_gdal_specialized_fuzzer rraster GDALRegister_RRASTER "/vsimem/test.tar" "/vsitar//vsimem/test.tar/my.grd"
build_gdal_specialized_fuzzer aig GDALRegister_AIGrid "/vsimem/test.tar" "/vsitar//vsimem/test.tar/hdr.adf"
# mrf can use indirectly the GTiff driver
build_gdal_specialized_fuzzer mrf "GDALRegister_mrf();GDALRegister_GTiff" "/vsimem/test.tar" "/vsitar//vsimem/test.tar/byte.mrf"
build_gdal_specialized_fuzzer gdal_sdts GDALRegister_SDTS "/vsimem/test.tar" "/vsitar//vsimem/test.tar/1107CATD.DDF"
build_gdal_specialized_fuzzer gdal_vrt GDALAllRegister "/vsimem/test.tar" "/vsitar//vsimem/test.tar/test.vrt"
build_gdal_specialized_fuzzer ers GDALRegister_ERS "/vsimem/test.tar" "/vsitar//vsimem/test.tar/test.ers"
build_gdal_specialized_fuzzer zarr GDALRegister_Zarr "/vsimem/test.tar" "/vsitar//vsimem/test.tar"
build_gdal_specialized_fuzzer dimap "GDALRegister_DIMAP();GDALRegister_GTiff" "/vsimem/test.tar" "/vsitar//vsimem/test.tar"

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

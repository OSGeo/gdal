#!/bin/bash
# Note: tested with cppcheck 1.72 as shipped with Ubuntu 16.04
# as well as with cppcheck 1.76.1

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


LOG_FILE=/tmp/cppcheck_gdal.txt

CPPCHECK_VERSION="$(cppcheck --version | awk '{print $2}')"
if test $(expr $CPPCHECK_VERSION \>= 1.84) = 1; then
    OVERRIDE=
else
    OVERRIDE="-Doverride="
fi

echo "" > ${LOG_FILE}
for dirname in alg port gcore ogr frmts gnm apps fuzzers; do
    printf "Running cppcheck on %s (can be long): " "$dirname"
    cppcheck --inline-suppr --template='{file}:{line},{severity},{id},{message}' \
        --enable=all --inconclusive --std=posix -UAFL_FRIENDLY -UANDROID \
        -UCOMPAT_WITH_ICC_CONVERSION_CHECK -DDEBUG -UDEBUG_BOOL -DHAVE_CXX11=1 \
        -D__linux \
        -DGBool=int -DCPL_HAS_GINT64=1 -DHAVE_GEOS -DHAVE_EXPAT -DHAVE_XERCES -DCOMPILATION_ALLOWED \
        -DHAVE_SFCGAL -DHAVE_SPATIALITE -DSPATIALITE_412_OR_LATER \
        -DHAVE_SQLITE -DSQLITE_VERSION_NUMBER=3006000 -DHAVE_SQLITE_VFS \
        -DHAVE_RASTERLITE2 \
        -DHAVE_CURL -DLIBCURL_VERSION_NUM=0x073800 \
        -DPTHREAD_MUTEX_RECURSIVE -DCPU_LITTLE_ENDIAN -DCPL_IS_LSB=1 \
        -DKDU_MAJOR_VERSION=7 -DKDU_MINOR_VERSION=5 \
        -DHAVE_JASPER_UUID \
        -D__GNUC__==5 -DGDAL_COMPILATION \
        -DODBCVER=0x0300 \
        -DNETCDF_HAS_NC4 \
        -DJPEG_SUPPORTED \
        -DJPEG_DUAL_MODE_8_12 \
        -D_TOOLKIT_IN_DLL_ \
        -UGDAL_NO_AUTOLOAD \
        -DHAVE_MITAB \
        -Dva_copy=va_start \
        -D__cplusplus=201103 \
        -DVSIRealloc=realloc \
        -DCPPCHECK \
        -DDEBUG_MUTEX \
        -DDEBUG_PROXY_POOL \
        ${OVERRIDE} \
        -DOCAD_EXTERN= \
        -DTIFFLIB_VERSION=99999999 \
        -DHAVE_SSE_AT_COMPILE_TIME \
        -DHAVE_LIBXML2 \
        -DCPL_INTERNAL= \
        -DCHAR_BIT=8 \
        -DUCHAR_MAX=255 \
        -DSHRT_MIN=-32768 \
        -DSHRT_MAX=32767 \
        -DUSHRT_MAX=65535 \
        -DINT_MIN=-2147483648 \
        -DINT_MAX=2147483647 \
        -DUINT_MAX=4294967295U \
        --include=port/cpl_config.h \
        --include=port/cpl_port.h \
        -I port -I gcore -I ogr -I ogr/ogrsf_frmts -I ogr/ogrsf_frmts/geojson \
        -I ogr/ogrsf_frmts/geojson/libjson \
        -i cpl_mem_cache.h \
        -i ogrdissolve.cpp \
        -i gdalasyncread.cpp \
        -i gdaltorture.cpp \
        $dirname \
        -j "$(nproc)" >>${LOG_FILE} 2>&1 &
    # Display some progress to avoid Travis-CI killing the job after 10 minutes
    PID=$!
    while kill -0 $PID 2>/dev/null; do
        printf "."
        sleep 1
    done
    echo " done"
    if ! wait $PID; then
        echo "cppcheck failed"
        exit 1
    fi
done

ret_code=0

grep -v "unmatchedSuppression" ${LOG_FILE} | grep -v -e " yacc.c" -e PublicDecompWT -e "kdu_cache_wrapper.h" > ${LOG_FILE}.tmp
mv ${LOG_FILE}.tmp ${LOG_FILE}

# I don't want to care about SDE
grep -v -e "frmts/sde" -e  "ogr/ogrsf_frmts/sde" ${LOG_FILE} > ${LOG_FILE}.tmp
mv ${LOG_FILE}.tmp ${LOG_FILE}

# I don't want to care about flatbuffers
grep -v -e "ogr/ogrsf_frmts/flatgeobuf/flatbuffers" ${LOG_FILE} > ${LOG_FILE}.tmp
mv ${LOG_FILE}.tmp ${LOG_FILE}

# False positive deallocuse
grep -v -e "frmts/png/libpng/png.c" ${LOG_FILE} > ${LOG_FILE}.tmp
mv ${LOG_FILE}.tmp ${LOG_FILE}

if grep "null pointer" ${LOG_FILE} ; then
    echo "Null pointer check failed"
    ret_code=1
fi

if grep "duplicateBreak" ${LOG_FILE} ; then
    echo "duplicateBreak check failed"
    ret_code=1
fi

if grep "duplicateBranch" ${LOG_FILE} ; then
    echo "duplicateBranch check failed"
    ret_code=1
fi

if grep "uninitMemberVar" ${LOG_FILE} ; then
    echo "uninitMemberVar check failed"
    ret_code=1
fi

if grep "useInitializationList" ${LOG_FILE} ; then
    echo "uninitMemberVar check failed"
    ret_code=1
fi

if grep "clarifyCalculation" ${LOG_FILE} ; then
    echo "clarifyCalculation check failed"
    ret_code=1
fi

if grep "invalidPrintfArgType_uint" ${LOG_FILE} ; then
    echo "invalidPrintfArgType_uint check failed"
    ret_code=1
fi

if grep "catchExceptionByValue" ${LOG_FILE} ; then
    echo "catchExceptionByValue check failed"
    ret_code=1
fi

grep "memleakOnRealloc" ${LOG_FILE} | grep frmts/hdf4/hdf-eos > /dev/null && echo "memleakOnRealloc issues in frmts/hdf4/hdf-eos ignored"
grep "memleakOnRealloc" ${LOG_FILE} | grep frmts/grib/degrib > /dev/null && echo "memleakOnRealloc issues in frmts/grib/degrib ignored"

if grep "memleakOnRealloc" ${LOG_FILE} | grep -v -e frmts/hdf4/hdf-eos -e frmts/grib/degrib ; then
    echo "memleakOnRealloc check failed"
    ret_code=1
fi

# Those warnings in libjpeg seems to be false positives
#grep "arrayIndexOutOfBoundsCond" ${LOG_FILE} | grep frmts/jpeg/libjpeg > /dev/null && echo "arrayIndexOutOfBoundsCond issues in frmts/jpeg/libjpeg ignored"
if grep "arrayIndexOutOfBoundsCond" ${LOG_FILE} | grep -v frmts/jpeg/libjpeg ; then
    echo "arrayIndexOutOfBoundsCond check failed"
    ret_code=1
fi

grep "arrayIndexOutOfBounds," ${LOG_FILE} | grep frmts/hdf4/hdf-eos > /dev/null && echo "arrayIndexOutOfBounds issues in frmts/hdf4/hdf-eos ignored"
if grep "arrayIndexOutOfBounds," ${LOG_FILE} | grep -v frmts/hdf4/hdf-eos ; then
    echo "arrayIndexOutOfBounds check failed"
    ret_code=1
fi

if grep "syntaxError" ${LOG_FILE} | grep -v "is invalid C code" ; then
    echo "syntaxError check failed"
    ret_code=1
fi

grep "memleak," ${LOG_FILE} | grep frmts/hdf4/hdf-eos > /dev/null && echo "memleak issues in frmts/hdf4/hdf-eos ignored"
grep "memleak," ${LOG_FILE} | grep frmts/grib/degrib > /dev/null && echo "memleak issues in frmts/grib/degrib ignored"
if grep "memleak," ${LOG_FILE} | grep -v -e frmts/hdf4/hdf-eos -e frmts/grib/degrib ; then
    echo "memleak check failed"
    ret_code=1
fi

if grep "eraseDereference" ${LOG_FILE} ; then
    echo "eraseDereference check failed"
    ret_code=1
fi

if grep "memsetClass," ${LOG_FILE} ; then
    echo "memsetClass check failed"
    ret_code=1
fi

# Most if not all of them are false positives
grep "uninitvar," ${LOG_FILE} | grep frmts/hdf4/hdf-eos > /dev/null && echo "(potential) uninitvar issues in frmts/hdf4/hdf-eos ignored"
grep "uninitvar," ${LOG_FILE} | grep frmts/grib/degrib > /dev/null && echo "(potential) uninitvar issues in frmts/grib/degrib ignored"
grep "uninitvar," ${LOG_FILE} | grep frmts/gtiff/libtiff > /dev/null && echo "(potential) uninitvar issues in frmts/gtiff/libtiff ignored"
grep "uninitvar," ${LOG_FILE} | grep frmts/gtiff/libgeotiff > /dev/null && echo "(potential) uninitvar issues in frmts/gtiff/libgeotiff ignored"
grep "uninitvar," ${LOG_FILE} | grep frmts/jpeg/libjpeg > /dev/null && echo "(potential) uninitvar issues in frmts/jpeg/libjpeg ignored"
grep "uninitvar," ${LOG_FILE} | grep frmts/gif/giflib > /dev/null && echo "(potential) uninitvar issues in frmts/gif/giflib ignored"
grep "uninitvar," ${LOG_FILE} | grep frmts/png/libpng > /dev/null && echo "(potential) uninitvar issues in frmts/png/libpng ignored"
grep "uninitvar," ${LOG_FILE} | grep frmts/zlib > /dev/null && echo "(potential) uninitvar issues in frmts/zlib ignored"
grep "uninitvar," ${LOG_FILE} | grep ogr/ogrsf_frmts/geojson/libjson > /dev/null && echo "(potential) uninitvar issues in ogr/ogrsf_frmts/geojson/libjson ignored"

if grep "uninitvar," ${LOG_FILE} | grep -v -e frmts/hdf4/hdf-eos \
        -e frmts/grib/degrib -e frmts/gtiff/libtiff -e frmts/gtiff/libgeotiff \
        -e frmts/jpeg/libjpeg -e frmts/gif/giflib -e frmts/png/libpng \
        -e frmts/zlib -e ogr/ogrsf_frmts/geojson/libjson \
        -e osr_cs_wkt_parser.c ; then
    echo "uninitvar check failed"
    ret_code=1
fi

grep "uninitdata," ${LOG_FILE} | grep "frmts/grib/degrib/g2clib" > /dev/null && echo "(potential) uninitdata issues in frmts/grib/degrib/g2clib ignored"

if grep "uninitdata," ${LOG_FILE} | grep -v "frmts/grib/degrib/g2clib" ; then
    echo "uninitdata check failed"
    ret_code=1
fi

if grep "va_list_usedBeforeStarted" ${LOG_FILE} ; then
    echo "va_list_usedBeforeStarted check failed"
    ret_code=1
fi

if grep "duplInheritedMember" ${LOG_FILE} ; then
    echo "duplInheritedMember check failed"
    ret_code=1
fi

if grep "terminateStrncpy" ${LOG_FILE} ; then
    echo "terminateStrncpy check failed"
    ret_code=1
fi

if grep "operatorEqVarError" ${LOG_FILE} ; then
    echo "operatorEqVarError check failed"
    ret_code=1
fi

if grep "uselessAssignmentPtrArg" ${LOG_FILE} | grep -v -e swq_parser.cpp -e osr_cs_wkt_parser.c -e ods_formula_parser.cpp ; then
    echo "uselessAssignmentPtrArg check failed"
    ret_code=1
fi

if grep "bufferNotZeroTerminated" ${LOG_FILE} ; then
    echo "bufferNotZeroTerminated check failed"
    ret_code=1
fi

if grep "sizeofDivisionMemfunc" ${LOG_FILE} ; then
    echo "sizeofDivisionMemfunc check failed"
    ret_code=1
fi

if grep "selfAssignment" ${LOG_FILE} ; then
    echo "selfAssignment check failed"
    ret_code=1
fi

if grep "invalidPrintfArgType_sint" ${LOG_FILE} ; then
    echo "invalidPrintfArgType_sint check failed"
    ret_code=1
fi

if grep "redundantAssignInSwitch" ${LOG_FILE} ; then
    echo "redundantAssignInSwitch check failed"
    ret_code=1
fi

if grep "publicAllocationError" ${LOG_FILE} ; then
    echo "publicAllocationError check failed"
    ret_code=1
fi

if grep "invalidScanfArgType_int" ${LOG_FILE} ; then
    echo "invalidScanfArgType_int check failed"
    ret_code=1
fi

if grep "invalidscanf," ${LOG_FILE} ; then
    echo "invalidscanf check failed"
    ret_code=1
fi

if grep "moduloAlwaysTrueFalse" ${LOG_FILE} ; then
    echo "moduloAlwaysTrueFalse check failed"
    ret_code=1
fi

if grep "charLiteralWithCharPtrCompare" ${LOG_FILE} ; then
    echo "charLiteralWithCharPtrCompare check failed"
    ret_code=1
fi

if grep "noConstructor" ${LOG_FILE} ; then
    echo "noConstructor check failed"
    ret_code=1
fi

if grep "noExplicitConstructor" ${LOG_FILE} ; then
    echo "noExplicitConstructor check failed"
    ret_code=1
fi

if grep "noCopyConstructor" ${LOG_FILE} ; then
    echo "noCopyConstructor check failed"
    ret_code=1
fi

if grep "passedByValue" ${LOG_FILE} ; then
    echo "passedByValue check failed"
    ret_code=1
fi

if grep "postfixOperator" ${LOG_FILE} ; then
    echo "postfixOperator check failed"
    ret_code=1
fi

if grep "redundantCopy" ${LOG_FILE} ; then
    echo "redundantCopy check failed"
    ret_code=1
fi

if grep "stlIfStrFind" ${LOG_FILE} ; then
    echo "stlIfStrFind check failed"
    ret_code=1
fi

if grep "functionStatic" ${LOG_FILE} | grep -v -e OGRSQLiteDataSource::OpenRaster \
                                            -e OGRSQLiteDataSource::OpenRasterSubDataset \
                                            -e cpl_mem_cache ; then
    echo "functionStatic check failed"
    ret_code=1
fi

if grep "knownConditionTrueFalse" ${LOG_FILE} ; then
    echo "knownConditionTrueFalse check failed"
    ret_code=1
fi

if grep "arrayIndexThenCheck" ${LOG_FILE} ; then
    echo "arrayIndexThenCheck check failed"
    ret_code=1
fi

if grep "unusedPrivateFunction" ${LOG_FILE} ; then
    echo "unusedPrivateFunction check failed"
    ret_code=1
fi

if grep "redundantCondition" ${LOG_FILE} ; then
    echo "redundantCondition check failed"
    ret_code=1
fi

if grep "unusedStructMember" ${LOG_FILE} | grep -v -e frmts/jpeg/libjpeg -e frmts/gtiff/libtiff -e frmts/zlib ; then
    echo "unusedStructMember check failed"
    ret_code=1
fi

if grep "multiCondition" ${LOG_FILE} ; then
    echo "multiCondition check failed"
    ret_code=1
fi

if grep "duplicateExpression" ${LOG_FILE} ; then
    echo "duplicateExpression check failed"
    ret_code=1
fi

if grep "operatorEq" ${LOG_FILE} ; then
    echo "operatorEq check failed"
    ret_code=1
fi

if grep "truncLongCastAssignment" ${LOG_FILE} ; then
    echo "truncLongCastAssignment check failed"
    ret_code=1
fi

if grep "exceptRethrowCopy" ${LOG_FILE} ; then
    echo "exceptRethrowCopy check failed"
    ret_code=1
fi

if grep "unusedVariable" ${LOG_FILE} ; then
    echo "unusedVariable check failed"
    ret_code=1
fi

if grep "unsafeClassCanLeak" ${LOG_FILE} ; then
    echo "unsafeClassCanLeak check failed"
    ret_code=1
fi

if grep "unsignedLessThanZero" ${LOG_FILE} | grep -v frmts/jpeg/libjpeg ; then
    echo "unsignedLessThanZero check failed"
    ret_code=1
fi

if grep "unpreciseMathCall" ${LOG_FILE} ; then
    echo "unpreciseMathCall check failed"
    ret_code=1
fi

if grep "unreachableCode" ${LOG_FILE} ; then
    echo "unreachableCode check failed"
    ret_code=1
fi

if grep "clarifyCondition" ${LOG_FILE} ; then
    echo "clarifyCondition check failed"
    ret_code=1
fi

if grep "redundantIfRemove" ${LOG_FILE} ; then
    echo "redundantIfRemove check failed"
    ret_code=1
fi

if grep "unassignedVariable" ${LOG_FILE} | grep -v frmts/png/libpng ; then
    echo "unassignedVariable check failed"
    ret_code=1
fi

if grep "redundantAssignment" ${LOG_FILE} | grep -v -e frmts/grib/degrib/g2clib -e frmts/hdf4/hdf-eos -e frmts/png/libpng ; then
    echo "redundantAssignment check failed"
    ret_code=1
fi

if grep "unreadVariable" ${LOG_FILE} | grep -v alg/internal_libqhull | \
                                    grep -v frmts/gtiff/libtiff  | \
                                    grep -v frmts/jpeg/libjpeg | \
                                    grep -v frmts/png/libpng | \
                                    grep -v frmts/grib/degrib/degrib | \
                                    grep -v frmts/hdf4/hdf-eos | \
                                    grep -v frmts/zlib ; then
    echo "unreadVariable check failed"
    ret_code=1
fi


for whitelisted_dir in alg/ port/ gcore/; do
    if grep "cstyleCast" ${LOG_FILE} | grep $whitelisted_dir ; then
        echo "cstyleCast check failed"
        ret_code=1
    fi
done

if grep "AssignmentAddressToInteger" ${LOG_FILE} ; then
    echo "AssignmentAddressToInteger check failed"
    ret_code=1
fi


# Check any remaining errors
if grep "error," ${LOG_FILE} | grep -v "uninitvar" | \
    grep -v "memleak," | grep -v "memleakOnRealloc" | \
    grep -v "frmts/jpeg/libjpeg/jdphuff.c:493,error,shiftNegative,Shifting a negative value is undefined behaviour" | \
    grep -v "ogr/ogrsf_frmts/avc/avc_bin.c:1866,error,bufferAccessOutOfBounds,Buffer is accessed out of bounds" | \
    grep -v "frmts/hdf4/hdf-eos/EHapi.c:1890,error,bufferAccessOutOfBounds,Buffer is accessed out of bounds." | \
    grep -v "frmts/hdf4/hdf-eos/EHapi.c:1920,error,bufferAccessOutOfBounds,Buffer is accessed out of bounds." | \
    grep -v "frmts/hdf4/hdf-eos/EHapi.c:1958,error,bufferAccessOutOfBounds,Buffer is accessed out of bounds." | \
    grep -v "frmts/hdf4/hdf-eos/EHapi.c:1994,error,bufferAccessOutOfBounds,Buffer is accessed out of bounds." | \
    grep -v "frmts/hdf4/hdf-eos/EHapi.c:2056,error,bufferAccessOutOfBounds,Buffer is accessed out of bounds." | \
    grep -v "frmts/hdf4/hdf-eos/EHapi.c:2118,error,bufferAccessOutOfBounds,Buffer is accessed out of bounds." | \
    grep -v "frmts/hdf4/hdf-eos/EHapi.c:2159,error,bufferAccessOutOfBounds,Buffer is accessed out of bounds." | \
    grep -v "frmts/hdf4/hdf-eos/EHapi.c:2208,error,bufferAccessOutOfBounds,Buffer is accessed out of bounds." | \
    grep -v "frmts/hdf4/hdf-eos/EHapi.c:2227,error,bufferAccessOutOfBounds,Buffer is accessed out of bounds." | \
    grep -v "frmts/grib/degrib/g2clib" | \
    grep -v "is invalid C code" ; then

    echo "Errors check failed"
    ret_code=1
fi

# Check any remaining warnings
if grep "warning," ${LOG_FILE} | grep -v "ods_formula_parser" | \
    grep -v "osr_cs_wkt_parser" | grep -v "swq_parser" | \
    grep -v "frmts/jpeg/libjpeg" ; then
    echo "Warnings check failed"
    ret_code=1
fi

if [ ${ret_code} = 0 ]; then
    echo "cppcheck succeeded"
fi

exit ${ret_code}

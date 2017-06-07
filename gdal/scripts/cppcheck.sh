#!/bin/bash
# Note: tested with cppcheck 1.72 as shipped with Ubuntu 16.04
# as well as with cppcheck 1.76.1

LOG_FILE=/tmp/cppcheck_gdal.txt

echo "" > ${LOG_FILE}
for dirname in alg port gcore ogr frmts gnm apps; do
    echo "Running cppcheck on $dirname... (can be long)"
    cppcheck --inline-suppr --template='{file}:{line},{severity},{id},{message}' \
        --enable=all --inconclusive --std=posix -UAFL_FRIENDLY -UANDROID \
        -UCOMPAT_WITH_ICC_CONVERSION_CHECK -DDEBUG -UDEBUG_BOOL -DHAVE_CXX11=1 \
        -DGBool=int -DCPL_HAS_GINT64=1 -DHAVE_GEOS -DHAVE_EXPAT -DHAVE_XERCES -DCOMPILATION_ALLOWED \
        -DHAVE_SFCGAL -DHAVE_SPATIALITE -DSPATIALITE_412_OR_LATER \
        -DHAVE_SQLITE -DSQLITE_VERSION_NUMBER=3006000 -DHAVE_SQLITE_VFS \
        -DPTHREAD_MUTEX_RECURSIVE -DCPU_LITTLE_ENDIAN -DCPL_IS_LSB=1 \
        -DKDU_MAJOR_VERSION=7 -DKDU_MINOR_VERSION=5 \
        -DHAVE_JASPER_UUID \
        -D__GNUC__==5 -DGDAL_COMPILATION \
        -DODBCVER=0x0300 \
        -DNETCDF_HAS_NC4 \
        -UGDAL_NO_AUTOLOAD \
        -DHAVE_MITAB \
        -Dva_copy=va_start \
        -D__cplusplus \
        -DVSIRealloc=realloc \
        -DCPPCHECK \
        -DDEBUG_MUTEX \
        -DDEBUG_PROXY_POOL \
        -Doverride= \
        --include=port/cpl_config.h \
        --include=port/cpl_port.h \
        -I port -I gcore -I ogr -I ogr/ogrsf_frmts \
        -i ogrdissolve.cpp \
        -i gdalasyncread.cpp \
        -i gdaltorture.cpp \
        $dirname \
        -j 8 >>${LOG_FILE} 2>&1
    if [[ $? -ne 0 ]] ; then
        echo "cppcheck failed"
        exit 1
    fi
done

ret_code=0

cat ${LOG_FILE} | grep -v "unmatchedSuppression" | grep -v " yacc.c" | grep -v PublicDecompWT | grep -v "kdu_cache_wrapper.h"  > ${LOG_FILE}.tmp
mv ${LOG_FILE}.tmp ${LOG_FILE}

grep "null pointer" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "Null pointer check failed"
    ret_code=1
fi

grep "duplicateBreak" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "duplicateBreak check failed"
    ret_code=1
fi

grep "duplicateBranch" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "duplicateBranch check failed"
    ret_code=1
fi

grep "uninitMemberVar" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "uninitMemberVar check failed"
    ret_code=1
fi

grep "useInitializationList" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "uninitMemberVar check failed"
    ret_code=1
fi

grep "clarifyCalculation" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "clarifyCalculation check failed"
    ret_code=1
fi

grep "invalidPrintfArgType_uint" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "invalidPrintfArgType_uint check failed"
    ret_code=1
fi

grep "catchExceptionByValue" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "catchExceptionByValue check failed"
    ret_code=1
fi

grep "memleakOnRealloc" ${LOG_FILE} | grep frmts/hdf4/hdf-eos > /dev/null && echo "memleakOnRealloc issues in frmts/hdf4/hdf-eos ignored"
grep "memleakOnRealloc" ${LOG_FILE} | grep frmts/grib/degrib18 > /dev/null && echo "memleakOnRealloc issues in frmts/grib/degrib18 ignored"
grep "memleakOnRealloc" ${LOG_FILE} | grep -v frmts/hdf4/hdf-eos | grep -v frmts/grib/degrib18
if [[ $? -eq 0 ]] ; then
    echo "memleakOnRealloc check failed"
    ret_code=1
fi

# Those warnings in libjpeg seems to be false positives
#grep "arrayIndexOutOfBoundsCond" ${LOG_FILE} | grep frmts/jpeg/libjpeg > /dev/null && echo "arrayIndexOutOfBoundsCond issues in frmts/jpeg/libjpeg ignored"
grep "arrayIndexOutOfBoundsCond" ${LOG_FILE} | grep -v frmts/jpeg/libjpeg
if [[ $? -eq 0 ]] ; then
    echo "arrayIndexOutOfBoundsCond check failed"
    ret_code=1
fi

grep "arrayIndexOutOfBounds," ${LOG_FILE} | grep frmts/hdf4/hdf-eos > /dev/null && echo "arrayIndexOutOfBounds issues in frmts/hdf4/hdf-eos ignored"
grep "arrayIndexOutOfBounds," ${LOG_FILE} | grep -v frmts/hdf4/hdf-eos
if [[ $? -eq 0 ]] ; then
    echo "arrayIndexOutOfBounds check failed"
    ret_code=1
fi

grep "syntaxError" ${LOG_FILE} | grep -v "is invalid C code"
if [[ $? -eq 0 ]] ; then
    echo "syntaxError check failed"
    ret_code=1
fi

grep "memleak," ${LOG_FILE} | grep frmts/hdf4/hdf-eos > /dev/null && echo "memleak issues in frmts/hdf4/hdf-eos ignored"
grep "memleak," ${LOG_FILE} | grep frmts/grib/degrib18 > /dev/null && echo "memleak issues in frmts/grib/degrib18 ignored"
grep "memleak," ${LOG_FILE} | grep -v frmts/hdf4/hdf-eos | grep -v frmts/grib/degrib18
if [[ $? -eq 0 ]] ; then
    echo "memleak check failed"
    ret_code=1
fi

grep "eraseDereference" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "eraseDereference check failed"
    ret_code=1
fi

grep "memsetClass," ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "memsetClass check failed"
    ret_code=1
fi

# Most if not all of them are false positives
grep "uninitvar," ${LOG_FILE} | grep frmts/hdf4/hdf-eos > /dev/null && echo "(potential) uninitvar issues in frmts/hdf4/hdf-eos ignored"
grep "uninitvar," ${LOG_FILE} | grep frmts/grib/degrib18 > /dev/null && echo "(potential) uninitvar issues in frmts/grib/degrib18 ignored"
grep "uninitvar," ${LOG_FILE} | grep frmts/gtiff/libtiff > /dev/null && echo "(potential) uninitvar issues in frmts/gtiff/libtiff ignored"
grep "uninitvar," ${LOG_FILE} | grep frmts/gtiff/libgeotiff > /dev/null && echo "(potential) uninitvar issues in frmts/gtiff/libgeotiff ignored"
grep "uninitvar," ${LOG_FILE} | grep frmts/jpeg/libjpeg > /dev/null && echo "(potential) uninitvar issues in frmts/jpeg/libjpeg ignored"
grep "uninitvar," ${LOG_FILE} | grep frmts/gif/giflib > /dev/null && echo "(potential) uninitvar issues in frmts/gif/giflib ignored"
grep "uninitvar," ${LOG_FILE} | grep frmts/png/libpng > /dev/null && echo "(potential) uninitvar issues in frmts/png/libpng ignored"
grep "uninitvar," ${LOG_FILE} | grep frmts/zlib > /dev/null && echo "(potential) uninitvar issues in frmts/zlib ignored"
grep "uninitvar," ${LOG_FILE} | grep ogr/ogrsf_frmts/geojson/libjson > /dev/null && echo "(potential) uninitvar issues in ogr/ogrsf_frmts/geojson/libjson ignored"

grep "uninitvar," ${LOG_FILE} | grep -v frmts/hdf4/hdf-eos | \
                                grep -v frmts/grib/degrib18 | \
                                grep -v frmts/gtiff/libtiff | \
                                grep -v frmts/gtiff/libgeotiff | \
                                grep -v frmts/jpeg/libjpeg | \
                                grep -v frmts/gif/giflib | \
                                grep -v frmts/png/libpng | \
                                grep -v frmts/zlib | \
                                grep -v ogr/ogrsf_frmts/geojson/libjson | \
                                grep -v osr_cs_wkt_parser.c
if [[ $? -eq 0 ]] ; then
    echo "uninitvar check failed"
    ret_code=1
fi

grep "uninitdata," ${LOG_FILE} | grep "frmts/grib/degrib18/g2clib-1.0.4" > /dev/null && echo "(potential) uninitdata issues in frmts/grib/degrib18/g2clib-1.0.4 ignored"

grep "uninitdata," ${LOG_FILE} | grep -v "frmts/grib/degrib18/g2clib-1.0.4"
if [[ $? -eq 0 ]] ; then
    echo "uninitdata check failed"
    ret_code=1
fi

grep "va_list_usedBeforeStarted" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "va_list_usedBeforeStarted check failed"
    ret_code=1
fi

grep "duplInheritedMember" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "duplInheritedMember check failed"
    ret_code=1
fi

grep "terminateStrncpy" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "terminateStrncpy check failed"
    ret_code=1
fi

grep "operatorEqVarError" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "operatorEqVarError check failed"
    ret_code=1
fi

grep "uselessAssignmentPtrArg" ${LOG_FILE} | grep -v swq_parser.cpp | grep -v osr_cs_wkt_parser.c | grep -v ods_formula_parser.cpp
if [[ $? -eq 0 ]] ; then
    echo "uselessAssignmentPtrArg check failed"
    ret_code=1
fi

grep "bufferNotZeroTerminated" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "bufferNotZeroTerminated check failed"
    ret_code=1
fi

grep "sizeofDivisionMemfunc" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "sizeofDivisionMemfunc check failed"
    ret_code=1
fi

grep "selfAssignment" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "selfAssignment check failed"
    ret_code=1
fi

grep "invalidPrintfArgType_sint" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "invalidPrintfArgType_sint check failed"
    ret_code=1
fi

grep "redundantAssignInSwitch" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "redundantAssignInSwitch check failed"
    ret_code=1
fi

grep "publicAllocationError" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "publicAllocationError check failed"
    ret_code=1
fi

grep "invalidScanfArgType_int" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "invalidScanfArgType_int check failed"
    ret_code=1
fi

grep "invalidscanf," ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "invalidscanf check failed"
    ret_code=1
fi

grep "moduloAlwaysTrueFalse" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "moduloAlwaysTrueFalse check failed"
    ret_code=1
fi

grep "charLiteralWithCharPtrCompare" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "charLiteralWithCharPtrCompare check failed"
    ret_code=1
fi

grep "noConstructor" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "noConstructor check failed"
    ret_code=1
fi

grep "noExplicitConstructor" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "noExplicitConstructor check failed"
    ret_code=1
fi

grep "noCopyConstructor" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "noCopyConstructor check failed"
    ret_code=1
fi

grep "passedByValue" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "passedByValue check failed"
    ret_code=1
fi

grep "postfixOperator" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "postfixOperator check failed"
    ret_code=1
fi

grep "redundantCopy" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "redundantCopy check failed"
    ret_code=1
fi

grep "stlIfStrFind" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "stlIfStrFind check failed"
    ret_code=1
fi

grep "functionStatic" ${LOG_FILE} | grep -v "OGRSQLiteDataSource::OpenRaster" | grep -v "OGRSQLiteDataSource::OpenRasterSubDataset"
if [[ $? -eq 0 ]] ; then
    echo "functionStatic check failed"
    ret_code=1
fi

grep "knownConditionTrueFalse" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "knownConditionTrueFalse check failed"
    ret_code=1
fi

grep "arrayIndexThenCheck" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "arrayIndexThenCheck check failed"
    ret_code=1
fi

grep "unusedPrivateFunction" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "unusedPrivateFunction check failed"
    ret_code=1
fi

grep "redundantCondition" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "redundantCondition check failed"
    ret_code=1
fi

grep "unusedStructMember" ${LOG_FILE} | grep -v frmts/jpeg/libjpeg | grep -v frmts/gtiff/libtiff | grep -v frmts/zlib
if [[ $? -eq 0 ]] ; then
    echo "unusedStructMember check failed"
    ret_code=1
fi

grep "multiCondition" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "multiCondition check failed"
    ret_code=1
fi

grep "duplicateExpression" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "duplicateExpression check failed"
    ret_code=1
fi

grep "operatorEq" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "operatorEq check failed"
    ret_code=1
fi

grep "truncLongCastAssignment" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "truncLongCastAssignment check failed"
    ret_code=1
fi

grep "exceptRethrowCopy" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "exceptRethrowCopy check failed"
    ret_code=1
fi

grep "unusedVariable" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "unusedVariable check failed"
    ret_code=1
fi

grep "unsafeClassCanLeak" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "unsafeClassCanLeak check failed"
    ret_code=1
fi

grep "unsignedLessThanZero" ${LOG_FILE} | grep -v frmts/jpeg/libjpeg
if [[ $? -eq 0 ]] ; then
    echo "unsignedLessThanZero check failed"
    ret_code=1
fi

grep "unpreciseMathCall" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "unpreciseMathCall check failed"
    ret_code=1
fi

grep "unreachableCode" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "unreachableCode check failed"
    ret_code=1
fi

grep "clarifyCondition" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "clarifyCondition check failed"
    ret_code=1
fi

grep "redundantIfRemove" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "redundantIfRemove check failed"
    ret_code=1
fi

grep "unassignedVariable" ${LOG_FILE} | grep -v frmts/png/libpng
if [[ $? -eq 0 ]] ; then
    echo "unassignedVariable check failed"
    ret_code=1
fi

grep "redundantAssignment" ${LOG_FILE} | grep -v frmts/grib/degrib18/g2clib-1.0.4 | grep -v frmts/hdf4/hdf-eos | grep -v frmts/png/libpng
if [[ $? -eq 0 ]] ; then
    echo "redundantAssignment check failed"
    ret_code=1
fi

grep "unreadVariable" ${LOG_FILE} | grep -v alg/internal_libqhull | \
                                    grep -v frmts/gtiff/libtiff  | \
                                    grep -v frmts/jpeg/libjpeg | \
                                    grep -v frmts/png/libpng | \
                                    grep -v frmts/grib/degrib18/degrib | \
                                    grep -v frmts/hdf4/hdf-eos | \
                                    grep -v frmts/zlib
if [[ $? -eq 0 ]] ; then
    echo "unreadVariable check failed"
    ret_code=1
fi


for whitelisted_dir in alg/ port/; do
    grep "cstyleCast" ${LOG_FILE} | grep $whitelisted_dir
    if [[ $? -eq 0 ]] ; then
        echo "cstyleCast check failed"
        ret_code=1
    fi
done

# Check any remaining errors
grep "error," ${LOG_FILE} | grep -v "uninitvar" | \
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
    grep -v "frmts/grib/degrib18/g2clib-1.0.4" | \
    grep -v "is invalid C code"

if [[ $? -eq 0 ]] ; then
    echo "Errors check failed"
    ret_code=1
fi

# Check any remaining warnings
grep "warning," ${LOG_FILE} | grep -v "ods_formula_parser" | \
    grep -v "osr_cs_wkt_parser" | grep -v "swq_parser" | \
    grep -v "frmts/jpeg/libjpeg"
if [[ $? -eq 0 ]] ; then
    echo "Warnings check failed"
    ret_code=1
fi

if [ ${ret_code} = 0 ]; then
    echo "cppcheck succeeded"
fi

exit ${ret_code}


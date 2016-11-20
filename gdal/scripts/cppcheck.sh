LOG_FILE=/tmp/cppcheck_gdal.txt
cppcheck --inline-suppr --template='{file}:{line},{severity},{id},{message}' \
    --enable=all --inconclusive --std=posix -UAFL_FRIENDLY -UANDROID \
    -UCOMPAT_WITH_ICC_CONVERSION_CHECK -DDEBUG -UDEBUG_BOOL -DHAVE_CXX11=1 \
    -DGBool=int -DHAVE_GEOS -DHAVE_XERCES \
    -DVSIRealloc=realloc \
    alg port gcore ogr frmts gnm \
    -j 8 >${LOG_FILE} 2>&1
if [[ $? -ne 0 ]] ; then
    echo "cppcheck failed"
    exit 1
fi

grep "null pointer" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "Null pointer check failed"
    exit 1
fi

grep "duplicateBreak" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "duplicateBreak check failed"
    exit 1
fi

grep "duplicateBranch" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "duplicateBranch check failed"
    exit 1
fi

grep "uninitMemberVar" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "uninitMemberVar check failed"
    exit 1
fi

grep "useInitializationList" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "uninitMemberVar check failed"
    exit 1
fi

grep "clarifyCalculation" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "clarifyCalculation check failed"
    exit 1
fi

grep "invalidPrintfArgType_uint" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "invalidPrintfArgType_uint check failed"
    exit 1
fi

grep "catchExceptionByValue" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "catchExceptionByValue check failed"
    exit 1
fi

grep "memleakOnRealloc" ${LOG_FILE} | grep frmts/hdf4/hdf-eos > /dev/null && echo "memleakOnRealloc issues in frmts/hdf4/hdf-eos ignored"
grep "memleakOnRealloc" ${LOG_FILE} | grep frmts/grib/degrib18 > /dev/null && echo "memleakOnRealloc issues in frmts/grib/degrib18 ignored"
grep "memleakOnRealloc" ${LOG_FILE} | grep -v frmts/hdf4/hdf-eos | grep -v frmts/grib/degrib18
if [[ $? -eq 0 ]] ; then
    echo "memleakOnRealloc check failed"
    exit 1
fi

# Those warnings in libjpeg seems to be false positives
#grep "arrayIndexOutOfBoundsCond" ${LOG_FILE} | grep frmts/jpeg/libjpeg > /dev/null && echo "arrayIndexOutOfBoundsCond issues in frmts/jpeg/libjpeg ignored"
grep "arrayIndexOutOfBoundsCond" ${LOG_FILE} | grep -v frmts/jpeg/libjpeg
if [[ $? -eq 0 ]] ; then
    echo "arrayIndexOutOfBoundsCond check failed"
    exit 1
fi

grep "arrayIndexOutOfBounds," ${LOG_FILE} | grep frmts/hdf4/hdf-eos > /dev/null && echo "arrayIndexOutOfBounds issues in frmts/hdf4/hdf-eos ignored"
grep "arrayIndexOutOfBounds," ${LOG_FILE} | grep -v frmts/hdf4/hdf-eos
if [[ $? -eq 0 ]] ; then
    echo "arrayIndexOutOfBounds check failed"
    exit 1
fi

grep "syntaxError" ${LOG_FILE}
if [[ $? -eq 0 ]] ; then
    echo "syntaxError check failed"
    exit 1
fi

echo "cppcheck succeeded"



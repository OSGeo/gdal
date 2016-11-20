LOG_FILE=/tmp/cppcheck_gdal.txt
cppcheck --inline-suppr --template='{file}:{line},{severity},{id},{message}' \
    --enable=all --inconclusive --std=posix -UAFL_FRIENDLY -UANDROID \
    -UCOMPAT_WITH_ICC_CONVERSION_CHECK -DDEBUG -UDEBUG_BOOL -DHAVE_CXX11=1 \
    -DGBool=int alg port gcore ogr frmts gnm -j 8 >${LOG_FILE} 2>&1
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

echo "cppcheck succeeded"



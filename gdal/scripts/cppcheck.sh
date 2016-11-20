LOG_FILE=/tmp/cppcheck_gdal.txt
cppcheck --inline-suppr --template='{file}:{line},{severity},{id},{message}' \
    --enable=all --inconclusive --std=posix -UAFL_FRIENDLY -UANDROID \
    -UCOMPAT_WITH_ICC_CONVERSION_CHECK -DDEBUG -UDEBUG_BOOL \
    alg port gcore ogr frmts gnm -j 8 >${LOG_FILE} 2>&1
grep "null pointer" ${LOG_FILE} && exit 1
grep "duplicateBreak" ${LOG_FILE} && exit 1
grep "duplicateBranch" ${LOG_FILE} && exit 1


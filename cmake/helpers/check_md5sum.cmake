file(READ "${IN_FILE}" CONTENTS)

string(MD5 MD5SUM "${CONTENTS}")

if(NOT("${MD5SUM}" STREQUAL "${EXPECTED_MD5SUM}"))
    message(FATAL_ERROR "File ${IN_FILE} has been modified. target ${TARGET} should be manually run. And ${FILENAME_CMAKE} should be updated with \"${MD5SUM}\"")
endif()

#!/bin/sh

# Post-install tests for a macOSX framework build.
#
# First required argument is the installed prefix.

echo "Running post-install tests with macOS Framework"

prefix=$1
if [ -z "$prefix" ]; then
  echo "First positional argument (installed prefix) is required"
  exit 1
fi

FRAMEWORK_PARENT_DIR="$prefix/Library/Frameworks"
if [ ! -d "$FRAMEWORK_PARENT_DIR/gdal.framework" ]; then
  echo "Error: gdal.framework not found in $FRAMEWORK_PARENT_DIR"
  exit 1
fi

# Run tests from shell, count any errors.
ERRORS=0
NTESTS=0

check_otool(){
  printf "Testing expected otool output ... "
  NTESTS=$(($NTESTS + 1))
  OTOOL_OUTPUT=$(otool -L ./$1 | grep -i gdal)
  OTOOL_SUBSTR="gdal.framework"

  case "$OTOOL_OUTPUT" in
    *$OTOOL_SUBSTR*)
      echo "passed" ;;
    *)
      ERRORS=$(($ERRORS + 1))
      echo "failed: otool output '$OTOOL_OUTPUT' does not contain '$OTOOL_SUBSTR'" ;;
  esac
}

CXX="${CXX:-c++}"
CC="${CC:-cc}"

# We add the Header path explicitly so existing GDAL test files don't need to be
# rewritten to use <gdal/gdal.h> instead of <gdal.h>.
# This is usually done automatically by the Apple toolchain.
INCLUDES="-I${FRAMEWORK_PARENT_DIR}/gdal.framework/Headers"
FRAMEWORK_FLAGS="-F${FRAMEWORK_PARENT_DIR} -framework gdal"

cd $(dirname $0)

echo "Testing C app"
cd test_c
${CC} -Wall test_c.c -o test_c_app ${INCLUDES} ${FRAMEWORK_FLAGS}
if [ $? -ne 0 ]; then
  echo "failed: C compilation error"
  ERRORS=$(($ERRORS + 1))
else
  check_otool test_c_app
  # Ensure it runs without dyld library missing errors
  DYLD_FRAMEWORK_PATH=${FRAMEWORK_PARENT_DIR} ./test_c_app >/dev/null
  if [ $? -ne 0 ]; then
    echo "failed: Runtime execution error"
    ERRORS=$(($ERRORS + 1))
  fi
fi
cd ..

echo "Testing C++ app"
cd test_cpp
${CXX} -Wall -std=c++11 test_cpp.cpp -o test_cpp_app ${INCLUDES} ${FRAMEWORK_FLAGS}
if [ $? -ne 0 ]; then
  echo "failed: C++ compilation error"
  ERRORS=$(($ERRORS + 1))
else
  check_otool test_cpp_app
  # Ensure it runs without dyld library missing errors
  DYLD_FRAMEWORK_PATH=${FRAMEWORK_PARENT_DIR} ./test_cpp_app >/dev/null
  if [ $? -ne 0 ]; then
    echo "failed: Runtime execution error"
    ERRORS=$(($ERRORS + 1))
  fi
fi
cd ..

echo "$ERRORS tests failed out of $NTESTS"
exit $ERRORS

#!/bin/sh

# Post-install tests with CMake
#
# First required argument is the installed prefix, which
# is used to set PKG_CONFIG_PATH and LD_LIBRARY_PATH/DYLD_LIBRARY_PATH
#
# Second, optional argument can be '--static', to skip the ldd check.

echo "Running post-install tests with CMake"

prefix=$1
if [ -z "$prefix" ]; then
    echo "First positional argument to the the installed prefix is required"
    exit 1
fi

export PKG_CONFIG_PATH=$prefix/lib/pkgconfig

# Run tests from shell, count any errors
ERRORS=0
NTESTS=0

UNAME=$(uname)
case $UNAME in
  Darwin*)
    alias ldd="otool -L"
    export DYLD_LIBRARY_PATH=$prefix/lib
    ;;
  Linux*)
    export LD_LIBRARY_PATH=$prefix/lib
    ;;
  *)
    echo "no ldd equivalent found for UNAME=$UNAME"
    exit 1 ;;
esac

check_ldd(){
  printf "Testing expected ldd output ... "
  NTESTS=$(($NTESTS + 1))
  LDD_OUTPUT=$(ldd ./$1 | grep libgdal)
  LDD_SUBSTR=$LD_LIBRARY_PATH/libgdal.
  case "$LDD_OUTPUT" in
    *$LDD_SUBSTR*)
      echo "passed" ;;
    *)
      ERRORS=$(($ERRORS + 1))
      echo "failed: ldd output '$LDD_OUTPUT' does not contain '$LDD_SUBSTR'" ;;
  esac
}

PKG_CONFIG_MODVERSION=$(pkg-config gdal --modversion)

check_version(){
  printf "Testing expected version ... "
  NTESTS=$(($NTESTS + 1))
  VERSION_OUTPUT=$(./$1)
  case "$VERSION_OUTPUT" in
    $PKG_CONFIG_MODVERSION*)
      echo "passed" ;;
    *)
      ERRORS=$(($ERRORS + 1))
      echo "failed: '$VERSION_OUTPUT' != '$PKG_CONFIG_MODVERSION'" ;;
  esac
}

cd $(dirname $0)

echo Testing C app
mkdir -p test_c/build
cd test_c/build
cmake .. -DCMAKE_BUILD_TYPE=Release -DGDAL_DIR=$prefix/lib/cmake/gdal
cmake --build .

if test "$2" != "--static"; then
  check_ldd test_c
fi
check_version test_c

cd ../..
rm -Rf test_c/build

echo Testing C++ app
mkdir -p test_cpp/build
cd test_cpp/build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DGDAL_DIR=$prefix/lib/cmake/gdal
cmake --build .

if test "$2" != "--static"; then
  check_ldd test_cpp
fi
check_version test_cpp

cd ../..
rm -Rf test_cpp/build

echo "$ERRORS tests failed out of $NTESTS"
exit $ERRORS

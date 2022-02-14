#!/bin/sh

# Post-install tests with gdal-config
#
# First required argument is the installed prefix, which
# is used to set LD_LIBRARY_PATH/DYLD_LIBRARY_PATH
#
# Second, optional argument can be '--static', to skip the ldd check.

echo "Running post-install tests with gdal-config"

prefix=$1
if [ -z "$prefix" ]; then
    echo "First positional argument to the the installed prefix is required"
    exit 1
fi

GDAL_CONFIG="${GDAL_CONFIG:-$prefix/bin/gdal-config}"

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

GDAL_CONFIG_VERSION=$(${GDAL_CONFIG} --version)

check_version(){
  printf "Testing expected version ... "
  NTESTS=$(($NTESTS + 1))
  VERSION_OUTPUT=$(./$1)
  case "$VERSION_OUTPUT" in
    $GDAL_CONFIG_VERSION*)
      echo "passed" ;;
    *)
      ERRORS=$(($ERRORS + 1))
      echo "failed: '$VERSION_OUTPUT' != '$GDAL_CONFIG_VERSION'" ;;
  esac
}

cd $(dirname $0)

echo Testing C app
cd test_c
make clean
make "GDAL_CONFIG=${GDAL_CONFIG}"

if test "$2" != "--static"; then
  check_ldd test_c
fi
check_version test_c

make clean
cd ..

echo Testing C++ app
cd test_cpp
make clean
make "GDAL_CONFIG=${GDAL_CONFIG}"

if test "$2" != "--static"; then
  check_ldd test_cpp
fi
check_version test_cpp

make clean
cd ..

echo "$ERRORS tests failed out of $NTESTS"
exit $ERRORS

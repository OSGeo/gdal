#!/bin/sh
set -x

if test -f CMakeCache.txt; then
  echo "ERROR: mixing autoconf builds and in-tree CMake builds is not supported"
  exit 1
fi

#libtoolize --force --copy
aclocal -I ./m4
# We deliberately do not use autoheader, since it introduces PACKAGE junk
# that conflicts with other packages in cpl_config.h.in (FrankW)
#autoheader
autoconf

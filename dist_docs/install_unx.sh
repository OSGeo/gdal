#!/bin/sh

if test $# -eq 0; then
    echo "Usage: install_unx.sh install-path"
    echo
    echo "This script will attempt to install GDAL binaries, and shared"
    echo "library in the named location."
    exit 1
fi

PREFIX=$1

if test ! -d $PREFIX ; then
  echo "Directory $PREFIX does not exist.  Please create or correct."
  exit 1
fi

if test ! -f bin/gdalinfo ; then
  echo "This script must be run from within the unpacked binary distribution"
  echo "directory."
  exit 1
fi

###############################################################################
# Ensure required subdirectories exist.
#

if test ! -d $PREFIX/lib ; then
  mkdir $PREFIX/lib
fi
if test ! -d $PREFIX/bin ; then
  mkdir $PREFIX/bin
fi
if test ! -d $PREFIX/share ; then
  mkdir $PREFIX/share
fi
if test ! -d $PREFIX/share/gdal ; then
  mkdir $PREFIX/share/gdal
fi

###############################################################################
# The following is intended to "burn" an updated INST_DATA location
# into the given file over the preformatted message embedded in the so.
# Look at gcore/gdaldrivermanager.cpp for a clue as to what is going on there.
#

SHARED_LIB=libgdal.1.1.so

for SHARED_LIB in lib/* ; do
  cp $SHARED_LIB $PREFIX/lib
  bin/burnpath $PREFIX/$SHARED_LIB __INST_DATA_TARGET: $PREFIX/share/gdal
done

###############################################################################
# Copy the rest of the files.
#

cp share/gdal/* $PREFIX/share/gdal

for EXECUTABLE in bin/* ; do
  if test "$EXECUTABLE" == "bin/gdal-config" -o "$EXECUTABLE" == "bin/burnpath" ; then
    /bin/true
  else
    cp $EXECUTABLE $PREFIX/bin
    bin/burnpath $PREFIX/$EXECUTABLE __INST_DATA_TARGET: $PREFIX/share/gdal
  fi
done

echo "Installation of GDAL to $PREFIX complete."


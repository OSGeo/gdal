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
# The following is inded to use sed to "burn" an updated INST_DATA location
# into the given file over the preformatted message embedded in the so.
# Look at core/gdaldrivermanager.cpp for a clue as to what is going on there.
#

NEW_INST_DATA=$PREFIX/share/gdal
EMBEDDED='__INST_DATA_TARGET:                                                                                                                                      '
ORIG=${EMBEDDED:0:${#NEW_INST_DATA}+19}

sed -e "s#$ORIG#__INST_DATA_TARGET:$NEW_INST_DATA#" < lib/libgdal.1.1.so > $PREFIX/lib/libgdal.1.1.so

###############################################################################
# Copy the rest of the files.
#
 
cp share/gdal/* $PREFIX/share/gdal
cp bin/* $PREFIX/bin

echo "Installation of GDAL to $PREFIX complete."


#!/bin/sh

# This is a script to build the python extensions with Python 2.7 and
# a mingw32 cross-compiler under Linux/Unix. It needs wine to run native python.

# You may need to customize the following versions to match your cross-compiler
# name and your native python installation
if test "x${CXX}" = "x"; then
  CXX=i586-mingw32msvc-g++
else
  CXX="${CXX}"
fi
PYTHONHOME=$HOME/.wine/drive_c/Python27
PYTHONLIB=python27

if test -d "${PYTHONHOME}/Lib/site-packages/numpy/core/include"; then
    echo "NumPy found !"
    HAS_NUMPY=yes
else
    HAS_NUMPY=no
fi

INCFLAGS="-I${PYTHONHOME}/include -I../../port -I../../gcore -I../../alg -I../../ogr/ -I../../apps/"
LINKFLAGS="-L../../.libs -lgdal -L${PYTHONHOME}/libs -l${PYTHONLIB}"
CFLAGS="-O2 -D__MSVCRT_VERSION__=0x0601"

# Run native python
wine "$PYTHONHOME/python" setup.py build

# Determine OUTDIR
if test -d build/lib.win32-2.7; then
  OUTDIR=build/lib.win32-2.7/osgeo
elif test -d build/lib.win-amd64-2.7; then
  OUTDIR=build/lib.win-amd64-2.7/osgeo
  CFLAGS="-DMS_WIN64 $CFLAGS"
else
  echo "Cannot determine OUTDIR"
  exit 1
fi

# Build extensions

# Horrible hack because if <cmath> is included after Python.h we get weird errors
echo '#include <cmath>' > extensions/gdal_wrap.temp.cpp
cat extensions/gdal_wrap.cpp >> extensions/gdal_wrap.temp.cpp
${CXX} ${CFLAGS} -std=c++11 extensions/gdal_wrap.temp.cpp -shared -o ${OUTDIR}/_gdal.pyd ${INCFLAGS} ${LINKFLAGS}
${CXX} ${CFLAGS} -std=c++11 extensions/ogr_wrap.cpp -shared -o ${OUTDIR}/_ogr.pyd ${INCFLAGS} ${LINKFLAGS}
${CXX} ${CFLAGS} -std=c++11 extensions/osr_wrap.cpp -shared -o ${OUTDIR}/_osr.pyd ${INCFLAGS} ${LINKFLAGS}
${CXX} ${CFLAGS} -std=c++11 extensions/gdalconst_wrap.c -shared -o ${OUTDIR}/_gdalconst.pyd ${INCFLAGS} ${LINKFLAGS}

if test x${HAS_NUMPY} = "xyes"; then
    ${CXX} ${CFLAGS} extensions/gdal_array_wrap.cpp -shared -o ${OUTDIR}/_gdal_array.pyd ${INCFLAGS} -I${PYTHONHOME}/Lib/site-packages/numpy/core/include ${LINKFLAGS}
fi

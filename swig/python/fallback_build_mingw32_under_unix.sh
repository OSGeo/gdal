#!/bin/sh

# This is a script to build the python extensions with Python 2.7 and
# a mingw32 cross-compiler under Linux/Unix. It needs wine to run native python.

# You may need to customize the following versions to match your cross-compiler
# name and your native python installation
CXX=i586-mingw32msvc-g++
PYTHONHOME=$HOME/.wine/drive_c/Python27
OUTDIR=build/lib.win32-2.7/osgeo
PYTHONLIB=python27

if test -d ${PYTHONHOME}/Lib/site-packages/numpy/core/include; then
    echo "NumPy found !"
    HAS_NUMPY=yes
else
    HAS_NUMPY=no
fi

INCFLAGS="-I${PYTHONHOME}/include -I../../port -I../../gcore -I../../alg -I../../ogr/"
LINKFLAGS="-L../../.libs -lgdal -L${PYTHONHOME}/libs -l${PYTHONLIB}"
CFLAGS="-O2 -D__MSVCRT_VERSION__=0x0601"

# Run native python
wine ${PYTHONHOME}/python setup.py build

# Build extensions
${CXX} ${CFLAGS} extensions/gdal_wrap.cpp -shared -o ${OUTDIR}/_gdal.pyd ${INCFLAGS} ${LINKFLAGS}
${CXX} ${CFLAGS} extensions/ogr_wrap.cpp -shared -o ${OUTDIR}/_ogr.pyd ${INCFLAGS} ${LINKFLAGS}
${CXX} ${CFLAGS} extensions/osr_wrap.cpp -shared -o ${OUTDIR}/_osr.pyd ${INCFLAGS} ${LINKFLAGS}
${CXX} ${CFLAGS} extensions/gdalconst_wrap.c -shared -o ${OUTDIR}/_gdalconst.pyd ${INCFLAGS} ${LINKFLAGS}

if test x${HAS_NUMPY} = "xyes"; then
    ${CXX} ${CFLAGS} extensions/gdal_array_wrap.cpp -shared -o ${OUTDIR}/_gdal_array.pyd ${INCFLAGS} -I${PYTHONHOME}/Lib/site-packages/numpy/core/include ${LINKFLAGS}
fi

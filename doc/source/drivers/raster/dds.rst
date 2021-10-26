.. _raster.dds:

================================================================================
DDS -- DirectDraw Surface
================================================================================

.. shortname:: DDS

.. build_dependencies:: Crunch Lib

The DirectDraw Surface file format
(uses the filename extension DDS), from Microsoft, is a standard for
storing data compressed with the lossy S3 Texture Compression (S3TC)
algorithm. The DDS format and compression are provided by the crunch
library.

Support for reading has been added in GDAL 3.1. Previous versions have write-only
support.

The driver supports the following texture formats: DXT1, DXT1A, DXT3
(default), DXT5 and ETC1. You can set the texture format using the creation
option FORMAT.

The driver supports the following compression quality: SUPERFAST, FAST,
NORMAL (default), BETTER and UBER. You can set the compression quality
using the creation option QUALITY.

More information about `Crunch Lib <https://github.com/BinomialLLC/crunch>`__

NOTE: Implemented as ``gdal/frmts/dds/ddsdataset.cpp``.

Driver capabilities
-------------------

.. supports_createcopy::


Build instructions
------------------

Building crunch can be a bit difficult. The `build_fixes` branch of
https://github.com/rouault/crunch/ includes a CMake build system, as well as
a few fixes that are not found in the upstream repository

Build crunch
++++++++++++

Linux
*****

.. code-block::

    git clone -b build_fixes https://github.com/rouault/crunch
    cd crunch
    mkdir build
    cd build
    cmake .. -DCMAKE_INSTALL_PREFIX=$HOME/install-crunch -DCMAKE_BUILD_TYPE=Release
    make -j8 install

Windows
*******

.. code-block::

    git clone -b build_fixes https://github.com/rouault/crunch
    cd crunch
    mkdir build
    cd build
    cmake .. -DCMAKE_INSTALL_PREFIX=c:\dev\install-crunch -G "Visual Studio 15 2017 Win64"
    cmake --build . --config Release --target install

Build GDAL against crunch
+++++++++++++++++++++++++

Linux
*****

.. code-block::

    ./configure --with-dds=$HOME/install-crunch

Windows
*******

In nmake.local, add the following lines:

.. code-block::

    CRUNCH_INC = -Ic:\dev\install-crunch\include\crunch
    CRUNCH_LIB = c:\dev\install-crunch\lib\crunch.lib

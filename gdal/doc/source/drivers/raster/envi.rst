.. _raster.envi:

================================================================================
ENVI -- ENVI .hdr Labelled Raster
================================================================================

.. shortname:: ENVI

.. built_in_by_default::

GDAL supports some variations of raw raster files with associated ENVI
style .hdr files describing the format. To select an existing ENVI
raster file select the binary file containing the data (as opposed to
the .hdr file), and GDAL will find the .hdr file by replacing the
dataset extension with .hdr.

GDAL should support reading bil, bip and bsq interleaved formats, and
most pixel types are supported, including 8bit unsigned, 16 and 32bit
signed and unsigned integers, 32bit and 64 bit floating point, and 32bit
and 64bit complex floating point. There is limited support for
recognising map_info keywords with the coordinate system and
georeferencing. In particular, UTM and State Plane should work.

All ENVI header fields are stored in the
ENVI metadata domain, and all of these can then be written out to the
header file.

Creation Options:

-  **INTERLEAVE=BSQ/BIP/BIL**: Force the generation specified type of
   interleaving. **BSQ** -- band sequential (default), **BIP** --- data
   interleaved by pixel, **BIL** -- data interleaved by line.
-  **SUFFIX=REPLACE/ADD**: Force adding ".hdr" suffix to supplied
   filename, e.g. if user selects "file.bin" name for output dataset,
   "file.bin.hdr" header file will be created. By default header file
   suffix replaces the binary file suffix, e.g. for "file.bin" name
   "file.hdr" header file will be created.

NOTE: Implemented as ``gdal/frmts/raw/envidataset.cpp``.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::


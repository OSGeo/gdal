.. _raster.lerc:

================================================================================
LERC -- Limited Error Raster Compression
================================================================================

.. versionadded:: 3.14

.. shortname:: LERC

.. built_in_by_default::

LERC is an open-source image or raster format which supports rapid encoding
and decoding for any pixel type (not just RGB or Byte). Users set the maximum
compression error per pixel while encoding, so the precision of the original
input image is preserved (within user defined error bounds).

This driver is read-only and opens raw LERC files.
LERC compressed content can also be found in MRF or TIFF files.

When a mask is included in a LERC file, it is reported as a GDAL mask band
for integer data types, or as NoData=nan for Float32 and Float64 data types.

Before GDAL 3.14, opening raw LERC file was supported by the MRF driver.

Driver capabilities
-------------------

.. supports_virtualio::

Links
-----

-  `liblerc <https://github.com/esri/lerc>`__

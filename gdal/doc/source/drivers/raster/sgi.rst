.. _raster.sgi:

================================================================================
SGI -- SGI Image Format
================================================================================

.. shortname:: SGI

.. built_in_by_default::

The SGI driver currently supports the reading and writing of SGI Image
files.

The driver currently supports 1, 2, 3, and 4 band images. The driver
currently supports "8 bit per channel value" images. The driver supports
both uncompressed and run-length encoded (RLE) images for reading, but
created files are always RLE compressed..

The GDAL SGI Driver was based on Paul Bourke's SGI image read code.

See Also:

-  `Paul Bourke's SGI Image Read
   Code <http://astronomy.swin.edu.au/~pbourke/dataformats/sgirgb/>`__
-  `SGI Image File Format
   Document <ftp://ftp.sgi.com/graphics/SGIIMAGESPEC>`__

NOTE: Implemented as ``gdal/frmts/sgi/sgidataset.cpp``.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_virtualio::

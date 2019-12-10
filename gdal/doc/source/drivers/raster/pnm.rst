.. _raster.pnm:

================================================================================
PNM -- Netpbm (.pgm, .ppm)
================================================================================

.. shortname:: PNM

.. built_in_by_default::

GDAL includes support for reading, and creating .pgm (greyscale), and
.ppm (RGB color) files compatible with the Netpbm tools. Only the binary
(raw) formats are supported.

Netpbm files can be created with a type of PNM.

Creation Options:

-  **MAXVAL=n**: Force setting the maximum color value to **n** in the
   output PNM file. May be useful if you planning to use the output
   files with software which is not liberal to this value.

NOTE: Implemented as ``gdal/frmts/raw/pnmdataset.cpp``.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_virtualio::

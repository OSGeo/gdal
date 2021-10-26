.. _raster.genbin:

================================================================================
GenBin -- Generic Binary (.hdr labelled)
================================================================================

.. shortname:: GenBin

.. built_in_by_default::

This driver supporting reading "Generic Binary" files labelled with a
.hdr file, but distinct from the more common ESRI labelled .hdr format
(EHdr driver). The origin of this format is not entirely clear. The .hdr
files supported by this driver are look something like this:

::

   {{{
   BANDS:      1
   ROWS:    6542
   COLS:    9340
   ...
   }}}

Pixel data types of U8, U16, S16, F32, F64, and U1 (bit) are supported.
Georeferencing and coordinate system information should be supported
when provided.

NOTE: Implemented as ``gdal/frmts/raw/genbindataset.cpp``.

Driver capabilities
-------------------

.. supports_virtualio::

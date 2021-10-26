.. _raster.bt:

================================================================================
BT -- VTP .bt Binary Terrain Format
================================================================================

.. shortname:: BT

.. built_in_by_default:: 

The .bt format is used for elevation data in the VTP software. The
driver includes support for reading and writing .bt 1.3 format including
support for Int16, Int32 and Float32 pixel data types.

The driver does **not** support reading or writing gzipped (.bt.gz) .bt
files even though this is supported by the VTP software. Please unpack
the files before using with GDAL using the "gzip -d file.bt.gz".

Projections in external .prj files are read and written, and support for
most internally defined coordinate systems is also available.

Read/write imagery access with the GDAL .bt driver is terribly slow due
to a very inefficient access strategy to this column oriented data. This
could be corrected, but it would be a fair effort.

NOTE: Implemented as ``gdal/frmts/raw/btdataset.cpp``.

See Also: The `BT file
format <http://www.vterrain.org/Implementation/Formats/BT.html>`__ is
defined on the `VTP <http://www.vterrain.org/>`__ web site.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::


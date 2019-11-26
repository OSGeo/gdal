.. _raster.jdem:

================================================================================
JDEM -- Japanese DEM (.mem)
================================================================================

.. shortname:: JDEM

.. built_in_by_default::

GDAL includes read support for Japanese DEM files, normally having the
extension .mem. These files are a product of the Japanese Geographic
Survey Institute.

These files are represented as having one 32bit floating band with
elevation data. The georeferencing of the files is returned as well as
the coordinate system (always lat/long on the Tokyo datum).

There is no update or creation support for this format.

NOTE: Implemented as ``gdal/frmts/jdem/jdemdataset.cpp``.

See Also: `Geographic Survey Institute (GSI) Web
Site. <http://www.gsi.go.jp/ENGLISH/>`__

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::


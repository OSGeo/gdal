.. _raster.ilwis:

================================================================================
ILWIS -- Raster Map
================================================================================

.. shortname:: ILWIS

.. built_in_by_default::

This driver implements reading and writing of ILWIS raster maps and map
lists. Select the raster files with ``the.mpr`` (for raster map) or
``the.mpl`` (for maplist) extensions.

Features:

-  Support for Byte, Int16, Int32 and Float64 pixel data types.
-  Supports map lists with an associated set of ILWIS raster maps.
-  Read and write geo-reference (.grf). Support for geo-referencing
   transform is limited to north-oriented GeoRefCorner only. If possible
   the affine transform is computed from the corner coordinates.
-  Read and write coordinate files (.csy). Support is limited to:
   Projection type of Projection and Lat/Lon type that are defined in
   .csy file, the rest of pre-defined projection types are ignored.

Limitations:

-  Map lists with internal raster map storage (such as produced through
   Import General Raster) are not supported.
-  ILWIS domain (.dom) and representation (.rpr) files are currently
   ignored.

NOTE: Implemented in ``gdal/frmts/ilwis``.

See Also: http://www.itc.nl/ilwis/default.asp .

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

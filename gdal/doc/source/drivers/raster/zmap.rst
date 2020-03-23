.. _raster.zmap:

================================================================================
ZMap -- ZMap Plus Grid
================================================================================

.. shortname:: ZMAP

.. built_in_by_default::

Supported for read access and creation. This format is an ASCII
interchange format for gridded data in an ASCII line format for
transport and storage. It is commonly used in applications in the Oil
and Gas Exploration field.

By default, files are interpreted and written according to the
PIXEL_IS_AREA convention. If you define the ZMAP_PIXEL_IS_POINT
configuration option to TRUE, the PIXEL_IS_POINT convention will be
followed to interpret/write the file (the georeferenced values in the
header of the file will then be considered as the coordinate of the
center of the pixels). Note that in that case, GDAL will report the
extent with its usual PIXEL_IS_AREA convention (the coordinates of the
topleft corner as reported by GDAL will be a half-pixel at the top and
left of the values that appear in the file).

Informal specification given in this `GDAL-dev mailing list
thread <http://lists.osgeo.org/pipermail/gdal-dev/2011-June/029173.html>`__

NOTE: Implemented as ``gdal/frmts/zmap/zmapdataset.cpp``.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_georeferencing::

.. supports_virtualio::

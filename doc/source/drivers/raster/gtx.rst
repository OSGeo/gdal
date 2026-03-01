.. _raster_gtx:

================================================================================
GTX — NOAA Vertical Datum Grid Shift
================================================================================

.. shortname:: GTX

.. built_in_by_default::

The GTX format is used to store vertical datum grid shift grids, typically
distributed by NOAA. These grids are used internally by GDAL to apply vertical
datum and geoid transformations.

This driver is generally not accessed directly by end users, but is used
implicitly by GDAL when vertical datum transformations are requested.

Driver capabilities
-------------------

* Read-only
* Raster

File format
-----------

GTX files are binary raster grids containing vertical offset values. They are
commonly used in conjunction with coordinate reference systems that include
vertical components.

See also
--------

* https://gdal.org/programs/gdalwarp.html
* https://proj.org/
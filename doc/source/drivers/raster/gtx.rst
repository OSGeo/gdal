.. _raster_gtx:

================================================================================
GTX — NOAA Vertical Datum Grid Shift
================================================================================

.. shortname:: GTX

.. built_in_by_default::

The GTX format is used to store vertical datum grid shift grids, typically
<<<<<<< HEAD
distributed by NOAA. These grids are used internally by GDAL to apply vertical
datum and geoid transformations.

This driver is generally not accessed directly by end users, but is used
implicitly by GDAL when vertical datum transformations are requested.
=======
distributed by NOAA. These grids have been used historically by PROJ,
until PROJ 7.0 where `GeoTIFF-based grids <https://proj.org/en/stable/specifications/geodetictiffgrids.html>`__ have been introduced and are now the preferred way to convey geoid models.
>>>>>>> 7f70be708b8a562b4d90bd76bd664f32f280f2e7

Driver capabilities
-------------------

* Read-only
* Raster

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

File format
-----------

GTX files are binary raster grids containing vertical offset values. They are
commonly used in conjunction with coordinate reference systems that include
vertical components.

See also
--------

<<<<<<< HEAD
* https://gdal.org/programs/gdalwarp.html
=======
* :ref:`gdalwarp`
>>>>>>> 7f70be708b8a562b4d90bd76bd664f32f280f2e7
* https://proj.org/
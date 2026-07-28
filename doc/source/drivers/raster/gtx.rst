.. _raster_gtx:

================================================================================
GTX — NOAA Vertical Datum Grid Shift
================================================================================

.. shortname:: GTX

.. built_in_by_default::

The GTX format is used to store vertical datum grid shift grids, typically
distributed by NOAA. These grids have been used historically by PROJ,
until PROJ 7.0 where `GeoTIFF-based grids <https://proj.org/en/stable/specifications/geodetictiffgrids.html>`__ have been introduced and are now the preferred way to convey geoid models.

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

* :ref:`gdalwarp`
* https://proj.org/
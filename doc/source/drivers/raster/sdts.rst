.. _raster.sdts:

================================================================================
SDTS -- USGS SDTS DEM
================================================================================

.. shortname:: SDTS

.. built_in_by_default::

GDAL includes support for reading USGS SDTS formatted DEMs. USGS DEMs
are always returned with a data type of signed sixteen bit integer, or
32bit float. Projection and georeferencing information is also returned.

SDTS datasets consist of a number of files. Each DEM should have one
file with a name like XXXCATD.DDF. This should be selected to open the
dataset.

The elevation units of DEMs may be feet or meters. The GetType() method
on a band will attempt to return if the units are Feet ("ft") or Meters
("m").

NOTE: Implemented as ``gdal/frmts/sdts/sdtsdataset.cpp``.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

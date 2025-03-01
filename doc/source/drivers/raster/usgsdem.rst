.. _raster.usgsdem:

================================================================================
USGSDEM -- USGS ASCII DEM (and CDED)
================================================================================

.. shortname:: USGSDEM

.. built_in_by_default::

GDAL includes support for reading USGS ASCII DEM files. This is the
traditional format used by USGS before being replaced by SDTS, and is
the format used for CDED DEM data products from Canada. Most popular
variations on USGS DEM files should be supported, including correct
recognition of coordinate system, and georeferenced positioning.

The 7.5 minute (UTM grid) USGS DEM files will generally have regions of
missing data around the edges, and these are properly marked with a
nodata value. Elevation values in USGS DEM files may be in meters or
feet, and this will be indicated by the return value of
GDALRasterBand::GetUnitType() (either "m" or "ft").

Note that USGS DEM files are represented as one big tile. This may cause
cache thrashing problems if the GDAL tile cache size is small. It will
also result in a substantial delay when the first pixel is read as the
whole file will be ingested.

Some of the code for implementing usgsdemdataset.cpp was derived from
VTP code by Ben Discoe. See the `Virtual
Terrain <http://www.vterrain.org/>`__ project for more information on
VTP.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

--------------

NOTE: Implemented as :source_file:`frmts/usgsdem/usgsdemdataset.cpp`.

The USGS DEM reading code in GDAL was derived from the importer in the
`VTP <http://www.vterrain.org/>`__ software. The export capability was
developed with the financial support of the Yukon Department of
Environment.

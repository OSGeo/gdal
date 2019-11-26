.. _raster.snodas:

================================================================================
SNODAS -- Snow Data Assimilation System
================================================================================

.. shortname:: SNODAS

.. built_in_by_default::

This is a convenience driver to read Snow Data Assimilation System data.
Those files contain Int16 raw binary data. The file to provide to GDAL
is the .Hdr file.

`Snow Data Assimilation System (SNODAS) Data Products at
NSIDC <http://nsidc.org/data/docs/noaa/g02158_snodas_snow_cover_model/index.html>`__

NOTE: Implemented as ``gdal/frmts/raw/snodasdataset.cpp``.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

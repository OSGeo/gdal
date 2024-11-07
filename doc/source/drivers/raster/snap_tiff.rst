.. _raster.snap_tiff:

================================================================================
SNAP_TIFF -- Sentinel Application Processing GeoTIFF
================================================================================

.. versionadded:: 3.10

.. shortname:: SNAP_TIFF

.. built_in_by_default::

This driver deals specifically with GeoTIFF files produced by the
Sentinel Application Processing (SNAP) toolbox.

Such files are formulated in a way that makes it difficult to read them with the
generic :ref:`raster.gtiff` driver.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Georeferencing
--------------

SNAP GeoTIFF files contain a geolocation array (stored as a regular grid of
GeoTIFF tie points). It is reported in the ``GEOLOCATION`` metadata domain.

The 4 corners of the geolocation array are also reported as ground control points
for faster (but less accurate) georeferencing.

Metadata
--------

Extensive metadata following DIMAP conventions is reported in the ``xml:DIMAP``
metadata domain.

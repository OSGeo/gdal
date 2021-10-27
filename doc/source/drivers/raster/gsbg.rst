.. _raster.gsbg:

================================================================================
GSBG -- Golden Software Binary Grid File Format
================================================================================

.. shortname:: GSBG

.. built_in_by_default::

This is the binary (non-human-readable) version of one of the raster
formats used by Golden Software products (such as the Surfer series).
Like the ASCII version, this format is supported for both reading and
writing (including create, delete, and copy). Currently the associated
formats for color, metadata, and shapes are not supported.

Surfer 8 uses a fixed nodata value at 1.701410009187828e+38. When writing a
GSBG dataset, GDAL remaps the user-specified input nodata value to 1.701410009187828e+38.

NOTE: Implemented as ``gdal/frmts/gsg/gsbgdataset.cpp``.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

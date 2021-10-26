.. _raster.sigdem:

================================================================================
SIGDEM -- Scaled Integer Gridded DEM
================================================================================

.. shortname:: SIGDEM

.. versionadded:: 2.4

.. built_in_by_default:: 

The SIGDEM driver supports reading and writing `Scaled Integer Gridded
DEM <https://github.com/revolsys/sigdem>`__ files.

SIGDEM files contain exactly 1 band. The in-memory band data is stored
using GDT_Float64.

SIGDEM prefers use of an EPSG ID inside the file for coordinate systems.
Only if the spatial reference doesn't have an EPSG ID will a .prj file
be written or read.

NOTE: Implemented as ``gdal/frmts/sigdem/sigdemdataset.cpp``.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_georeferencing::

.. supports_virtualio::

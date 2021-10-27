.. _raster.grassasciigrid:

================================================================================
GRASSASCIIGrid -- GRASS ASCII Grid
================================================================================

.. shortname:: GRASSASCIIGrid

.. built_in_by_default::

Supports reading GRASS ASCII grid format (similar to Arc/Info ASCIIGRID
command).

By default, the datatype returned for GRASS ASCII grid datasets by GDAL
is autodetected, and set to Float32 for grid with floating point values
or Int32 otherwise. This is done by analysing the format of the null
value and the first 100k bytes of data of the grid. You can also
explicitly specify the datatype by setting the GRASSASCIIGRID_DATATYPE
configuration option (Int32, Float32 and Float64 values are supported
currently)

NOTE: Implemented as ``gdal/frmts/aaigrid/aaigriddataset.cpp``.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

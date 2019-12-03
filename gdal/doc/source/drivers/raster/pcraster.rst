.. _raster.pcraster:

================================================================================
PCRaster -- PCRaster raster file format
================================================================================

.. shortname:: PCRaster

.. build_dependencies:: (internal libcf provided)

GDAL includes support for reading and writing PCRaster raster files.
PCRaster is a dynamic modeling system for distributed simulation models.
The main applications of PCRaster are found in environmental modeling:
geography, hydrology, ecology to name a few. Examples include models for
research on global hydrology, vegetation competition models, slope
stability models and land use change models.

The driver reads all types of PCRaster maps: booleans, nominal,
ordinals, scalar, directional and ldd. The same cell representation used
to store values in the file is used to store the values in memory.

The driver detects whether the source of the GDAL raster is a PCRaster
file. When such a raster is written to a file the value scale of the
original raster will be used. The driver **always** writes values using
UINT1, INT4 or REAL4 cell representations, depending on the value scale:

============ ===================
Value scale  Cell representation
============ ===================
VS_BOOLEAN   CR_UINT1
VS_NOMINAL   CR_INT4
VS_ORDINAL   CR_INT4
VS_SCALAR    CR_REAL4
VS_DIRECTION CR_REAL4
VS_LDD       CR_UINT1
============ ===================

For rasters from other sources than a PCRaster raster file a value scale
and cell representation is determined according to the following rules:

=============== =================== ==========================
Source type     Target value scale  Target cell representation
GDT_Byte        VS_BOOLEAN          CR_UINT1
GDT_Int32       VS_NOMINAL          CR_INT4
GDT_Float32     VS_SCALAR           CR_REAL4
GDT_Float64     VS_SCALAR           CR_REAL4
=============== =================== ==========================

The driver can convert values from one supported cell representation to
another. It cannot convert to unsupported cell representations. For
example, it is not possible to write a PCRaster raster file from values
which are used as CR_INT2 (GDT_Int16).

Although the de-facto file extension of a PCRaster raster file is .map,
the PCRaster software does not require a standardized file extension.

NOTE: Implemented as ``gdal/frmts/pcraster/pcrasterdataset.cpp``.

See also: `PCRaster website at Utrecht
University <http://pcraster.geo.uu.nl>`__.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

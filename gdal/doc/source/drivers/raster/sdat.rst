.. _raster.sdat:

================================================================================
SAGA -- SAGA GIS Binary Grid File Format
================================================================================

.. shortname:: SAGA

.. built_in_by_default::

The driver supports both reading and writing (including create, delete,
and copy) SAGA GIS binary grids. SAGA binary grid datasets are made of
an ASCII header (.SGRD) and a binary data (.SDAT) file with a common
basename. The .SDAT file should be selected to access the dataset.
Starting with GDAL 2.3, the driver can read compressed .sg-grd-z files
that are ZIP archives with .sgrd, .sdat and .prj files.

The driver supports reading the following SAGA datatypes (in brackets
the corresponding GDAL types): BIT (GDT_Byte), BYTE_UNSIGNED (GDT_Byte),
BYTE (GDT_Byte), SHORTINT_UNSIGNED (GDT_UInt16), SHORTINT (GDT_Int16),
INTEGER_UNSIGNED (GDT_UInt32), INTEGER (GDT_Int32), FLOAT (GDT_Float32)
and DOUBLE (GDT_Float64).

The driver supports writing the following SAGA datatypes: BYTE_UNSIGNED
(GDT_Byte), SHORTINT_UNSIGNED (GDT_UInt16), SHORTINT (GDT_Int16),
INTEGER_UNSIGNED (GDT_UInt32), INTEGER (GDT_Int32), FLOAT (GDT_Float32)
and DOUBLE (GDT_Float64).

Currently the driver does not support zFactors other than 1 and reading
SAGA grids which are written TOPTOBOTTOM.

NOTE: Implemented as ``gdal/frmts/saga/sagadataset.cpp``.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::


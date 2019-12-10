.. _raster.ndf:

================================================================================
NDF -- NLAPS Data Format
================================================================================

.. shortname:: NDF

.. built_in_by_default::

GDAL has limited support for reading NLAPS Data Format files. This is a
format primarily used by the Eros Data Center for distribution of
Landsat data. NDF datasets consist of a header file (often with the
extension .H1) and one or more associated raw data files (often .I1,
.I2, ...). To open a dataset select the header file, often with the
extension .H1, .H2 or .HD.

The NDF driver only supports 8bit data. The only supported projection is
UTM. NDF version 1 (NDF_VERSION=0.00) and NDF version 2 are both
supported.

NOTE: Implemented as ``gdal/frmts/raw/ndfdataset.cpp``.

See Also: `NLAPS Data Format
Specification <http://landsat.usgs.gov/documents/NLAPSII.pdf>`__.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::


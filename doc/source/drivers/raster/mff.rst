.. _raster.mff:

================================================================================
MFF -- Vexcel MFF Raster
================================================================================

.. shortname:: MFF

.. built_in_by_default::

GDAL includes read, update, and creation support for Vexcel's MFF raster
format. MFF dataset consist of a header file (typically with the
extension .hdr) and a set of data files with extensions like .x00, .b00
and so on. To open a dataset select the .hdr file.

Reading lat/long GCPs (TOP_LEFT_CORNER, ...) is supported but there is
no support for reading affine georeferencing or projection information.

Unrecognized keywords from the .hdr file are preserved as metadata.

All data types with GDAL equivalents are supported, including 8, 16, 32
and 64 bit data precisions in integer, real and complex data types. In
addition tile organized files (as produced by the Vexcel SAR Processor -
APP) are supported for reading.

On creation (with a format code of MFF) a simple, ungeoreferenced raster
file is created.

MFF files are not normally portable between systems with different byte
orders. However GDAL honours the new BYTE_ORDER keyword which can take a
value of LSB (Integer - little endian), and MSB (Motorola - big
endian). This may be manually added to the .hdr file if required.

NOTE: Implemented as ``gdal/frmts/raw/mffdataset.cpp``.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

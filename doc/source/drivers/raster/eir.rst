.. _raster.eir:

================================================================================
EIR -- Erdas Imagine Raw
================================================================================

.. shortname:: EIR

.. built_in_by_default::

GDAL supports the Erdas Imagine Raw format for read access including 1,
2, 4, 8, 16 and 32bit unsigned integers, 16 and 32bit signed integers
and 32 and 64bit complex floating point. Georeferencing is supported.

To open a dataset select the file with the header information. The
driver finds the image file from the header information. Erdas documents
call the header file the "raw" file and it may have the extension .raw
while the image file that contains the actual raw data may have the
extension .bl.

NOTE: Implemented as ``gdal/frmts/raw/eirdataset.cpp``.


Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

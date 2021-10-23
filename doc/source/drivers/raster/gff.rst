.. _raster.gff:

================================================================================
GFF -- Sandia National Laboratories GSAT File Format
================================================================================

.. shortname:: GFF

.. built_in_by_default::

This read-only GDAL driver is designed to provide access to processed
data from Sandia National Laboratories' various experimental sensors.
The format is essentially an arbitrary length header containing
instrument configuration and performance parameters along with a binary
matrix of 16- or 32-bit complex or byte real data.

The GFF format was implemented based on the Matlab code provided by
Sandia to read the data. The driver supports all types of data (16-bit
or 32-bit complex, real bytes) theoretically, however due to a lack of
data only 32-bit complex data has been tested.

Sandia provides some sample data at
http://www.sandia.gov/radar/complex-data/.

The extension for GFF formats is .gff.

NOTE: Implemented as ``gdal/frmts/gff/gff_dataset.cpp``.

Driver capabilities
-------------------

.. supports_virtualio::

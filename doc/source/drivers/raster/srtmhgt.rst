.. _raster.srtmhgt:

================================================================================
SRTMHGT -- SRTM HGT Format
================================================================================

.. shortname:: SRTMHGT

.. built_in_by_default::

The SRTM HGT driver currently supports the reading of SRTM-3 and SRTM-1
V2 (HGT) files. The files must be named like NXXEYYY.hgt, or starting
with GDAL 2.1.2, NXXEYYY[.something].hgt

Starting with GDAL 2.2, the driver can directly read .hgt.zip files
provided that they are named like NXXEYYY[.something].hgt.zip and
contain a NXXEYYY.hgt file. For previous versions, use
/vsizip//path/to/NXXEYYY[.something].hgt.zip/NXXEYYY.hgt syntax

The driver does support creating new files, but the input data must be
exactly formatted as a SRTM-3 or SRTM-1 cell. That is the size, and
bounds must be appropriate for a cell.

See Also:

-  `SRTM
   documentation <http://dds.cr.usgs.gov/srtm/version2_1/Documentation>`__
-  `SRTM FAQ <http://www2.jpl.nasa.gov/srtm/faq.html>`__
-  `SRTM data <http://dds.cr.usgs.gov/srtm/version2_1/>`__

NOTE: Implemented as ``gdal/frmts/srtmhgt/srtmhgtdataset.cpp``.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

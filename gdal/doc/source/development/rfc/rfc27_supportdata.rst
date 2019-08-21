.. _rfc-27:

================================================================================
RFC 27: Improved Supporting Data File Options
================================================================================

Author: Frank Warmerdam

Contact: warmerdam@pobox.com

Status: Development

Summary
-------

Currently GDAL depends on a variety of supporting data files from the
`gdal data <http://svn.osgeo.org/gdal/trunk/gdal/data>`__ directory. The
largest part of these are coordinate system dictionaries from EPSG and
other sources. It also includes S-57 dictionaries, seed DGN and DXF
files, and project logos. Uncompressed it currently comes to roughly
1.8MB and it is expected to grow as additional dictionaries are added
(for PCI and IAU coordinate systems for instance).

It has also been a frequent problem at run time to find the data files
when they are installed in unusual locations.

This RFC aims to overhaul support file handling with two new major
features.

1. The ability to read from compressed data files to reduce the disk
   footprint of GDAL.
2. The ability to embed the data files with the GDAL DLL or shared
   library to remove the "finding" problem.

CPL CSV Access via VSI*L
------------------------

The large majority of the support data file access is via the CPL CSV
API (gdal/port/cpl_csv.cpp). Finding support data files is done via
CPLFindFile(). It turns out these functions are still using the old VSI
API which does not support special handlers (like /vsizip/), or in at
least one case direct fopen() calls. So the first stage of this RFC is
to convert these functions to all use the VSI*L API. A
`patch <http://trac.osgeo.org/gdal/attachment/wiki/rfc27_supportdata/rfc27_csv_vsil.patch>`__
has been prepared that demonstrates the bulk of the required changes.
With this patch it is possible to access files from a GDAL_DATA setting
like /vsizip//home/warmerda/gdal/data/gdaldata.zip.

Note that we are explicitly changing the contract about the nature of
the FILE\* passed to functions like CSVReadParseLine() (real FILE\* vs.
VSI\ *L style FILE*). It is possible, though relatively unlikely that
application code, or private driver implementations will be using the SV
functions and will need to be changed. This change should be noted in
the GDAL 1.8 release notes.

It is also unclear if there will be bad interactions with the cpl_csv
implementation embedded in libgeotiff in some situations, such as when
using libgeotiff as an external library. Some review will be needed.

Another point to investigate is what the performance impact of doing all
the file finding through the VSI*L API will be.

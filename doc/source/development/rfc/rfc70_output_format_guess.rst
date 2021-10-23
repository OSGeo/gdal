.. _rfc-70:

============================================================================
RFC 70: Guessing output format from output file name extension for utilities
============================================================================

======================= ==========================
Author:                 Even Rouault
Contact:                even.rouault@spatialys.com
Started:                Aug 2017
Status:                 Adopted, implemented
Implementation version: 2.3.0
======================= ==========================

Summary
-------

This proposal is to add syntaxic sugar to make GDAL and OGR command line
utilities, so they take into account the extension of the output
filename to guess which output driver to use, when it is not explicitly
specified with -f / -of switch.

Motivation
----------

Currently command line utilities require to explicitly specify the
output format when not wishing to use the default format (generally
GeoTIFF for raster, and Shapefile for vector). But this is rather
counter-intuitive. For example "gdal_translate in.tif out.png" will
generate a GeoTIFF, and "ogr2ogr out.gpkg in.shp" a shapefile. So you
have to specify respectively -of PNG and -f GPKG to get the expected
result.

Guessing the output format from the extension of the output filename is
for example a behavior found in ImageMagick convert utility, or in
OpenJPEG opj_compress/opj_decompress utilities.

Changes in C/C++ and Python utilities
-------------------------------------

Command line utilities, when neither -f nor -of are specified (note:
since r39878 both switches can be indifferently used), will loop through
the registered drivers and check if one or several drivers, with output
capabilities, declare to recognize the extension of the output filename.

-  When one and only one driver declares this extension (.tif, .png,
   .jpg etc), it will be used automatically
-  When several drivers declare this extension (for example KML and
   LIBKML for .kml), the utility will select the first registered driver
   (except netCDF instead of GMT for .nc files), and a warning is
   emitted specifying which driver is used
-  When no driver declares this extension, and the extension is not
   empty (e.g a .mpg filename), the utility will error out

For completeness:

-  When there's no extension, and no prefix is recognized (see below),
   the default output driver will be silently used, as currently

Since at least GDAL 1.10, the base of this logic already exists since a
warning is emitted for C/C++ utilities, when the extension of the output
format is known to be recognized by another driver than the default
output driver.

Similarly, for vector output, if doing something like "ogr2ogr
PG:dbname=mydb out.shp", a PG:dbname=mydb directory is created with
shapefiles, instead of ingesting the shapefile into PostgreSQL. A
warning is emitted in that case since the PG driver declares the PG:
prefix in its metadata. The new behavior will be to imply the -update
switch in such situation.

When the utilities are available as library functions (GDALTranslate(),
etc.), output format guessing will also be applied if the -f/-of switch
is not specified

Changes in SWIG bindings
------------------------

For librarified utilities (gdal.Translate, etc.), the format argument
now defaults to None.

Potential issues
----------------

There might be some fragility with the new logic in the situation where
a GDAL version has only one driver that supports extension xxx, but a
later version adds another driver that also supports extension xxx (or
another distribution of the same version has a plugin that handles xxx).
So scripts that did "gdal_translate in out.xxx" would now error out in
the next version since several drivers are available.

Bottom line: always specify the output driver when
reliability/reproducibility is desired.

This RFC mostly helps for interactive conversions where the less you
type the better.

Backward compatibility
----------------------

This will break scripts that use an output filename whose extension is
matched by a driver which is not the default one. This incompatibility
is rather unlikely since previous GDAL versions already emit a warning
in this situation (for C/C++ utilities only. for Python utilities
default driver is silently used), so people have likely specified the
output driver if they really want to do "gdal_translate in.tif out.png
-of GTiff".

MIGRATION_GUIDE.TXT will mention those potential caveats.

Testing
-------

The existing autotest suite should continue to pass (with a few changes
related to tests for the current behavior)

Implementation
--------------

Implementation will be done by Even Rouault

Proposed implementation is in
`https://github.com/rouault/gdal2/tree/rfc70 <https://github.com/rouault/gdal2/tree/rfc70>`__

Diff:
`https://github.com/OSGeo/gdal/compare/trunk...rouault:rfc70 <https://github.com/OSGeo/gdal/compare/trunk...rouault:rfc70>`__

Voting history
--------------

+1 from JukkaR, TamasS, DanielM and EvenR

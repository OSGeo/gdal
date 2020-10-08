.. _rfc-25:

================================================================================
RFC 25: Fast Open (withdrawn)
================================================================================

Authors: Frank Warmerdam

Contact: warmerdam@pobox.com

Status: Withdrawn (in favor of some specific improvements in #2957 - rfc
may be renewed at a later date)

Summary
-------

This document proposes a mechanism for application to indicate a desire
for the fastest possible open of a raster file, even if a variety of
metadata and supporting information may not be available. It is
primarily intended to optimize applications working with catalogs
containing many raster files.

Implementation
--------------

An application can request fast open mode by setting the
"GDAL_FAST_OPEN" configuration option to "YES" - the default is assumed
to be no. When this option is set to YES selected drivers can operate
differently so as to optimize the speed of opening a dataset.
Acceleration options include:

-  Skip establishing a coordinate system - particularly helpful if it
   avoids an EPSG lookup.
-  Skip probing for supporting .aux.xml, world files and other files.

It is anticipated that fast open mode will be used primarily for fast
raster display from datasets where required metadata is already provided
by a catalog of some sort. Because of this it is essential that in fast
open mode datasets will still accurately return a list of bands, their
datatypes, and their overviews.

Thread Local Configuration Options
----------------------------------

In multi-threaded applications use of a process-global configuration
option - turned on just while calling GDALOpen() - may not be
appropriate. In particular, it is hard to ensure that this configuration
option won't affect GDALOpen()'s in other threads of the same process.
This problem also affects other configuration options that. To resolve
this problem it is intended to add a new function to set "thread local"
configuration options.

::

     void CPLSetThreadLocalConfigOption( const char *pszKey, const char *pszValue );

This mechanism will be implemented using normal thread local data
handling (CPLGetTLS(), etc).

It should be noted that CPLSetConfigOption() will continue to set
configuration options to apply to all threads. CPLGetConfigOption() will
first search thread local values, then process global values and then
the environment.

Work Plan
---------

For the time being the following changes will be made to drivers to
accelerate them in fast open mode.

-  GDALOpenInfo will avoid loading a list of all files in a directory.
-  GTIFF driver will avoid collecting a coordinate systems.

This work will be completed in trunk in time for the GDAL 1.7.0 release
by Frank Warmerdam.

Utilization
-----------

There is no plan to do this immediately, but the GDAL VRT driver would
be a good candidate to utilize this mechanism.

In theory, it would also be desirable for MapServer to utilize this mode
for tileindexed rasters - except that MapServer depends on the
geotransform coming from the underlying raster file rather than coming
from the raster catalog. MapServer also assumes the color table, and
nodata values will be available.

ArcGIS is also expected to utilize this feature.

Backward Compatibility Issues
-----------------------------

There are no known backward compatibility issues. However, there may be
forward compatibility issues if we are not precise and consistent from
version to version on what supporting info is allowed to be omitted in
fast open mode.

Testing
-------

-  Tests would be added to the appropriate driver test scripts to test
   fast open mode - confirming that expected information is discarded,
   and kept.

Issues
------

-  Potentially desirable things like ignoring .aux.xml files are not
   possible as they are also sometimes the source of overview
   information.
-  Potentially discarding all metadata including color tables, nodata
   values, and geotransforms makes this mode not useful for applications
   like MapServer that don't keep such information in their catalog.
-  This RFC does not discuss a way of accelerating GDALOpen() by
   skipping unnecessary drivers, though that would also potentially help
   quite a bit.

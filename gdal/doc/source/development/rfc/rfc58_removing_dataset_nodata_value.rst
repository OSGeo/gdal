.. _rfc-58:

=======================================================================================
RFC 58: Removing Dataset Nodata Value
=======================================================================================

Authors: Sean Gillies

Contact: sean at mapbox.com

Status: Adopted, implemented

Implementation version: 2.1

Summary
-------

This RFC concerns addition of a ``DeleteNoDataValue()`` function to the
C++ GDALRasterBand API. This function removes the nodata value of a
raster band. When it succeeds, the raster band will have no nodata
value. When it fails, the nodata value will be unchanged.

Rationale
---------

The nodata value has accessors ``GetNoDataValue()`` and
``SetNoDataValue()``. For GeoTIFFs, the value is stored in a
TIFFTAG_GDAL_NODATA TIFF tag. Newly created GeoTIFF files can have no
nodata value (no tag), but once a nodata value is set and stored it can
only be given new values, it can not be removed. Nor can it be set to a
value outside the range of the data type; for 8-bit data passing
``nan``, ``-inf``, or ``256`` to ``GDALSetNoDataValue()`` has the same
effect as passing 0.

The problem with un-removable nodata values is this:

-  Nodata masks (see GDAL RFC 15) can cover up a nodata value but if the
   .msk file gets lost (and this is ever the problem with sidecar
   files), the nodata value you were hiding is exposed again.
-  Nodata masks are not available everywhere in GDAL, nodata values are
   the only definition of valid data in some parts of GDAL.

The current recommended practice for removing a nodata value is to copy
the GeoTIFF using gdal_translate, specifying that the nodata tag not be
copied over along with the data. By making the nodata value fully
editable and removable we could avoid copying unnecessarily.

Changes
-------

The ``virtual CPLErr DeleteNoDataValue()`` method will be added to the
GDALRasterBand method in gdal_priv.h (C++ API), and
``CPLErr GDALDeleteRasterNoDataValue()`` to gdal.h (C API)

Updated drivers
~~~~~~~~~~~~~~~

The following drivers will be updated: GTiff, MEM, VRT and KEA.

For GTiff, the TIFFTAG_GDAL_NODATA TIFF tag is unset if GDAL is built
against libtiff 4.X, which is the appropriate behavior. For libtiff 3.X
where TIFFUnsetField() does not exist, the tag is set to the empty
string : GDAL 2.0 will detect that as the absence of a nodata value,
older versions will parse it as 0.

The ``GDALPamRasterBand`` class will also be updated (for drivers that
have no built-in mechanism nodata mechanism and rely on .aux.xml
sidecars). Note that this only removes the tag from the .aux.xml, so in
the situation where a driver would have a internal way of storing
nodata, but would be opened in read-only mode (so defaulting to PAM),
DeleteNoData() would have no effect.

SWIG bindings (Python / Java / C# / Perl) changes
-------------------------------------------------

The DeleteNoDataValue() method is added to the RasterBand object.

Utilities
---------

The gdal_edit.py script is enhanced with a -unsetnodata option.

Documentation
-------------

The new method and function are documented.

Test Suite
----------

The tests of the updated drivers test the effect of the new method.

Compatibility Issues
--------------------

None for the C API. ABI change for the C++ API because of introduction
of a new virtual method.

Related ticket
--------------

#2020 mentions the issue.

Implementation
--------------

Implementation will be done by Even Rouault and be sponsored by Mapbox.

A proposed implementation is available in
`https://github.com/OSGeo/gdal/compare/trunk...rouault:rfc58_removing_nodata_value <https://github.com/OSGeo/gdal/compare/trunk...rouault:rfc58_removing_nodata_value>`__

Voting history
--------------

+1 from EvenR, HowardB, DanielM and JukkaR

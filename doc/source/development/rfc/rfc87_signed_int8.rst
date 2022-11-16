.. _rfc-87:

=============================================================
RFC 87: Signed int8 data type for raster
=============================================================

============== =============================================
Author:        Even Rouault
Contact:       even.rouault @ spatialys.com
Started:       2022-Nov-8
Status:        Adopted and implemented
Target:        GDAL 3.7
============== =============================================

Summary
-------

This RFC describes the addition of a new GDT_Int8 data type, for signed
8-bit integer data, to the GDALDataType enumeration.

Motivation
----------

:ref:`rfc-14` introduced in GDAL 1.5.0 a way of specifying that a 8-bit integer
should be interpreted as signed, by (ab)using the GDT_Byte type, with the
addition of the PIXELTYPE=SIGNEDBYTE metadata item in the IMAGE_STRUCTURE metadata
domain of bands for which this is desired.

While this approach avoided the introduction of a new data type, which has
consequences internally in the GDAL code base at all places where a "switch" is
done on a GDALDataType variable, as well as impact on external code using GDAL,
this historical approach had the following shortcomings:

- drivers that need to support reading signed 8-bit integers must remind to
  expose the PIXELTYPE=SIGNEDBYTE metadata item.

- drivers that need to support writing signed 8-bit integers must provide a
  creation option for that purpose (generally called PIXELTYPE=SIGNEDBYTE
  itself).

- but more importantly, the absence of a proper data type means that it is easy
  to forget to test the PIXELTYPE=SIGNEDBYTE metadata item in all places where
  the value of pixels is taken into account. There were special cases for
  statistics computations, but most of the other code, such as the overview or
  warping computation code had no provision for it, and consequently
  mis-interpreted negative values in the range [-128,-1] as positive values in
  the range [128,255].

Details
-------

A new ``GDT_Int8`` = 14 item is added to the :cpp:enum:`GDALDataType` enumeration.

It is taken into account in all places in gcore/ and alg/ that have specific
behavior depending on the value of GDALDataType. All places that only dealt
specifically with a subset of types (e.g Byte, Float32) and promoted other
types to the closest one will automatically have proper support for the new
data type (e.g overview computation).

Existing drivers that, on reading, reported GDT_Byte + PIXELTYPE=SIGNEDBYTE are
modified to report GDT_Int8. This has backward compatibility impact to external
code. This affects the EEDAI, ERS, GTA, GTiff, HFA, netCDF, PostGISRaster, EHDR,
RRaster, Rasterlite2 and Zarr drivers.

Existing drivers that, on writing, accepted the PIXELTYPE=SIGNEDBYTE, are modified
to accept GDT_Int8 as a valid data type in their Create()/CreateCopy() implementations,
and advertise "GDT_Int8" in their GDAL_DMD_CREATIONDATATYPES metadata item.
The PIXELTYPE=SIGNEDBYTE creation option is kept but deprecated and discouraged:
working with it prior to this RFC was clunky at times, and will remain such.

The MEM, HDF4, HDF5, KEA and PDS4 drivers are extended to support GDT_Int8 on reading
(and writing, except HDF5 that is read-only).

Bindings
--------

The GDT_Int8 constant is made available in SWIG bindings.

The numpy interface of the Python bindings is modified to map GDT_Int8 to
``numpy.int8``.

Backward compatibility
----------------------

The PIXELTYPE=SIGNEDBYTE band metadata item in IMAGE_STRUCTURE is no longer
reported. Code that depended on it needs to switch testing for GDT_Int8.
Code that has "switch"-like constructs on GDALDataType must also account for
GDT_Int8.

Documentation
-------------

Documentation of utilities that have a ``-ot`` switch is modified to mention Int8.

:ref:`rfc-14` will be amended to point to this RFC, and mention that starting
with GDAL 3.7.0, PIXELTYPE=SIGNEDBYTE is no longer the technical solution.

Testing
-------

The test_gdal.cpp and testcopywords.cpp test files are extended to test functions
like :cpp:func:`GDALDataTypeIsInteger`, :cpp:func:`GDALDataTypeIsFloating`,
:cpp:func:`GDALDataTypeIsComplex`, etc. and all data type conversion combinations
from/into GInt8.

Parts of autotest, in driver specific tests, that were testing GByte +
PIXELTYPE=SIGNEDBYTE on reading or writing are adapted to the new technical solution.

Related tickets and PRs:
------------------------

https://github.com/OSGeo/gdal/issues/4002

https://github.com/OSGeo/gdal/pull/6633

Voting history
--------------

+1 from KurtS, JukkaR, MateuszL and EvenR

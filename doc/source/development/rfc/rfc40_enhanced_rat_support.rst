.. _rfc-40:

=======================================================================================
RFC 40: Improving performance of Raster Attribute Table implementation for large tables
=======================================================================================

Summary:
--------

Raster Attrbute Tables from some applications (notably segmentation) can
be very large and are slow to access with the current API due to the way
only one element can get read or written at a time. Also, when an
attribute table is requested by the application the whole table must be
read - there is no way of delaying this so just the required subset is
read off disk. These changes will bring the attribute table support more
in line with the way raster data is accessed.

Implementation:
---------------

It is proposed that GDALRasterAttributeTable be re-written as a virtual
base class. This will allow drivers to have their own implementation
that only reads and writes data when requested. A new derived class,
GDALDefaultRasterAttributeTable will be provided that provides the
functionality of the GDAL 1.x GDALRasterAttributeTable (ie holds all
data in memory).

Additional methods will be provided in the GDALRasterAttributeTable
class that allow 'chunks' of data from a column to be read/written in
one call. As with the GetValueAs functions columns of different types
would be able to read as a value of a different type (i.e., read a int
column as a double) with the appropriate conversion taking place. The
following overloaded methods will be available:

::

   CPLErr ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength, double *pdfData);
   CPLErr ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength, int *pnData);
   CPLErr ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength, char **papszStrList);

It is expected that the application will allocate the required space for
reading in the same way as with the RasterIO() call.

The char*\* type will be used for reading and writing strings. When
reading strings, it is expected that the array is created of the correct
size and ValuesIO will just create the individual strings for each row.
The application should call CPLFree on each of the strings before
de-allocating the array.

These methods will be available from C as GDALRATValuesIOAsDouble,
GDALRATValuesIOAsInteger and GDALRATValuesIOAsString.

This is also an opportunity to remove unused functions on the attribute
table such as GetRowMin(), GetRowMax() and GetColorOfValue().

Language Bindings:
------------------

The Python bindings will be altered so ValuesIO will be supported using
numpy arrays for the data with casting of types as appropriate. Strings
will be supported using the numpy support for string arrays.

Backward Compatibility:
-----------------------

The proposed additions will extend the C API. However, the C++ binary
interface will be broken and so GDAL 2.0 is suggested as an appropriate
time to introduce the changes.

Care will be taken to still support the use of Clone() and Serialize()
in derived implementations of the GDALRasterAttributeTable class as
these are called by existing code. For implementations where the table
is not held in memory these may fail if the table is larger than some
suitable limit (for example, GetRowCount() \* GetColCount() < 1 000
000). Clone() should return a instance of
GDALDefaultRasterAttributeTable to prevent problems with sharing memory
between objects.

Existing code may need to be altered to use create instances of
GDALDefaultRasterAttributeTable rather than GDALRasterAttributeTable if
an in memory implementation is still required.

Impact on Drivers
-----------------

The HFA driver will be updated to support all aspects of the new
interface, such as the new functions and reading/writing upon request.
Other drivers will be modified to continue to use the in memory
implementation (GDALDefaultRasterAttributeTable).

Testing
-------

The Python autotest suite will be extended to test the new API, both for
the default implementation and specialised implementation in the HFA
driver.

Timeline
--------

We (Sam Gillingham and Pete Bunting) are prepared undertake the work
required and have it ready for inclusion in GDAL 1.11 There needs to be
a discussion on the names of the methods and on the internal logic of
the methods.

Ticket
------

Ticket #5129 has been opened to track the progress of this RFC.

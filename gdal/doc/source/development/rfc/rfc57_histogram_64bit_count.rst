.. _rfc-57:

=======================================================================================
RFC 57: 64-bit bucket counts for histograms
=======================================================================================

Author: Even Rouault

Contact: even dot rouault at spatialys dot com

Status: Adopted, implemented

Version: 2.0

Summary
-------

This RFC modifies the GDALRasterBand GetHistogram(),
GetDefaultHistogram() and SetDefaultHistogram() methods to accept arrays
of 64-bit integer instead of the current arrays of 32-bit integer for
bucket counts. It also changes GetRasterSampleOverview() to take a
64-bit integer. This will fix issues when operating on large rasters
that have more than 2 billion pixels.

Core changes
------------

The following methods of GDALRasterBand class are modified to take a
GUIntBig\* argument for GetHistogram() and SetDefaultHistograph(),
GUIntBig*\* for GetDefaultHistogram() and GUIntBig for
GetRasterSampleOverview()

::

       virtual CPLErr  GetHistogram( double dfMin, double dfMax,
                             int nBuckets, GUIntBig * panHistogram,
                             int bIncludeOutOfRange, int bApproxOK,
                             GDALProgressFunc, void *pProgressData );

       virtual CPLErr GetDefaultHistogram( double *pdfMin, double *pdfMax,
                                           int *pnBuckets, GUIntBig ** ppanHistogram,
                                           int bForce,
                                           GDALProgressFunc, void *pProgressData);

       virtual CPLErr SetDefaultHistogram( double dfMin, double dfMax,
                                           int nBuckets, GUIntBig *panHistogram );

       virtual GDALRasterBand *GetRasterSampleOverview( GUIntBig );

PAM serialization/deserialization is also updated.

C API changes
~~~~~~~~~~~~~

Only additions :

::

   CPLErr CPL_DLL CPL_STDCALL GDALGetRasterHistogramEx( GDALRasterBandH hBand,
                                          double dfMin, double dfMax,
                                          int nBuckets, GUIntBig *panHistogram,
                                          int bIncludeOutOfRange, int bApproxOK,
                                          GDALProgressFunc pfnProgress,
                                          void * pProgressData );

   CPLErr CPL_DLL CPL_STDCALL GDALGetDefaultHistogramEx( GDALRasterBandH hBand,
                                          double *pdfMin, double *pdfMax,
                                          int *pnBuckets, GUIntBig **ppanHistogram,
                                          int bForce,
                                          GDALProgressFunc pfnProgress,
                                          void * pProgressData );

   CPLErr CPL_DLL CPL_STDCALL GDALSetDefaultHistogramEx( GDALRasterBandH hBand,
                                          double dfMin, double dfMax,
                                          int nBuckets, GUIntBig *panHistogram );

   GDALRasterBandH CPL_DLL CPL_STDCALL
                              GDALGetRasterSampleOverviewEx( GDALRasterBandH, GUIntBig );

The existing methods GDALGetRasterHistogram(), GDALGetDefaultHistogram()
and GDALSetDefaultHistogram() are marked deprecated. They internally
call the 64-bit methods, and, for GDALGetRasterHistogram() and
GDALGetDefaultHistogram(), warn if a 32-bit overflow would occur, in
which case the bucket count is set to INT_MAX.

Changes in drivers
------------------

All in-tree drivers that use/implement the C++ histogram methods are
modified: ECW, VRT, MEM and HFA.

Changes in utilities
--------------------

gdalinfo and gdalenhance are modified to use the modified methods.

Changes in SWIG bindings
------------------------

For Python bindings only, RasterBand.GetHistogram(),
GetDefaultHistogram() and SetDefaultHistogram() use the new 64-bit C
functions.

Other bindings could be updated, but likely need new typemaps for (int,
GUIntBig*). In the meantime, they still use the 32-bit C functions.

Compatibility
-------------

This modifies the C++ API and ABI.

Out-of-tree drivers must make sure to take into account the updated C++
API if they implement some of the 4 modified virtual methods.

Related ticket
--------------

#5159

Documentation
-------------

All new/modified methods/functions are documented. MIGRATION_GUIDE.TXT
is updated with a new section for this RFC.

Testing
-------

Setting/getting 64 bit values is tested in gcore/pam.y and
gdrivers/mem.py

Implementation
--------------

Implementation will be done by Even Rouault
(`Spatialys <http://spatialys.com>`__).

The proposed implementation lies in the "histogram_64bit_count" branch
of the
`https://github.com/rouault/gdal2/tree/histogram_64bit_count <https://github.com/rouault/gdal2/tree/histogram_64bit_count>`__.

The list of changes :
`https://github.com/rouault/gdal2/compare/histogram_64bit_count <https://github.com/rouault/gdal2/compare/histogram_64bit_count>`__

Voting history
--------------

+1 from DanielM, JukkaR and EvenR

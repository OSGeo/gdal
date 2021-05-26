.. _rfc-19:

================================================================================
RFC 19: Safer memory allocation in GDAL
================================================================================

Author: Even Rouault

Contact: even.rouault@spatialys.com

Status: Adopted, implemented

Summary
-------

This document contains proposal on how to make GDAL safer (prevent
crashes) when doing memory allocations. The starting point of this
discussion is ticket #2075.

Details
-------

In many places in GDAL source code, multiplications are done to compute
the size of the memory buffer to allocate, like raster blocks,
scanlines, whole image buffers, etc.. Currently no overflow checking is
done, thus leading to potential allocation of not large enough buffers.
Overflow can occur when raster dimensions are very large (this can be
the case with a WMS raster source for example) or when a dataset is
corrupted, intentionnaly or unintentionnaly. This can lead to latter
crash.

This RFC introduces new API to allocate memory when the computation of
the size to allocate is based on multiplications. These new API report
overflows when they occur. Overflows are detected by checking that
((a*b)/b) == a. This does not require to make assumptions on the size of
the variable types, their signedness, etc.

::

   /**
    VSIMalloc2 allocates (nSize1 * nSize2) bytes.
    In case of overflow of the multiplication, or if memory allocation fails, a
    NULL pointer is returned and a CE_Failure error is raised with CPLError().
    If nSize1 == 0 || nSize2 == 0, a NULL pointer will also be returned.
    CPLFree() or VSIFree() can be used to free memory allocated by this function.
   */
   void CPL_DLL *VSIMalloc2( size_t nSize1, size_t nSize2 );

   /**
    VSIMalloc3 allocates (nSize1 * nSize2 * nSize3) bytes.
    In case of overflow of the multiplication, or if memory allocation fails, a
    NULL pointer is returned and a CE_Failure error is raised with CPLError().
    If nSize1 == 0 || nSize2 == 0 || nSize3 == 0, a NULL pointer will also be returned.
    CPLFree() or VSIFree() can be used to free memory allocated by this function.
   */
   void CPL_DLL *VSIMalloc3( size_t nSize1, size_t nSize2, size_t nSize3 );

The behavior of VSIMalloc2 and VSIMalloc3 is consistent with the
behavior of VSIMalloc. Implementation of already existing memory
allocation API (CPLMalloc, CPLCalloc, CPLRealloc, VSIMalloc, VSICalloc,
VSIRealloc) will not be changed.

:ref:`rfc-8` will be
updated to promote new API for safer memory allocation. For example
using VSIMalloc2(x, y) instead of doing CPLMalloc(x \* y) or VSIMalloc(x
\* y).

Implementation steps
--------------------

1. Introduce the new API in gdal/port

2. Use the new API in GDAL core where it is relevant. The following
   files have been identified as candidates :
   gcore/gdalnodatamaskband.cpp, gcore/overview.cpp,
   gcore/gdaldriver.cpp, gcore/gdalrasterblock.cpp

3. Use the new API in GDAL drivers. This step can be done incrementally.
   Transition from CPL to VSI allocation can be necessary in some cases
   too. Candidate drivers : Idrisi, PNG, GXF, BSB, VRT, MEM, JP2KAK,
   RPFTOC, AIRSAIR, AIGRIB, XPM, USGDEM, BMP, GSG, HFA, AAIGRID. (See
   gdal_svn_trunk_use_vsi_safe_mul_in_frmts.patch in ticket #2075)

Even Rouault will implement the changes described in this RFC for the
GDAL 1.6.0 release.

Voting history
--------------

+1 from all PSC members (FrankW, DanielM, HowardB, TamasS, AndreyK)

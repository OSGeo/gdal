.. _rfc-95:

=============================================================
RFC 95: Use standard C/C++ integer types
=============================================================

============== =============================================
Author:        Even Rouault
Contact:       even.rouault @ spatialys.com
Started:       2023-Sep-15
Status:        Draft
Target:        GDAL 3.8
============== =============================================

Summary
-------

This RFC replaces the use of the custom integer types defined in cpl_port.h
(G[U]Int[8/16/32/64/Big]) by their standard [u]int[8/16/32/64]_t C99/C++11
counterparts.

Motivation
----------

- The existing Gxxxxx typedefs have been defined at a time that predates
  C99 and C++11 which introduced those fixed with integer types.
  If GDAL would be created today, we would use C99 data types.

- Using those aliases, particularly GIntBig/GUIntBig which are not
  self-explanatory on their actual width (64 bit), adds cognitive load to
  developers.

- Due to their poor namespacing, those short typenames may occasionaly clash
  with other libraries. At a time, Poppler used a ``GBool`` data type as well,
  which was clashing with GDAL's one.

- Other projects (e.g. libtiff), have recently switched from the use of
  similar custom types to C99 ones.

Details
-------

The following data types replacement are done in the whole code base:

- ``GBool``     --> ``bool``  (``GBool`` was aliased to ``int``)
- ``GInt8``     --> ``int8_t``
- ``GInt16``    --> ``int16_t``
- ``GUInt16``   --> ``uint16_t``
- ``GInt32``    --> ``int32_t``
- ``GUInt32``   --> ``uint32_t``
- ``GInt64``    --> ``int64_t``
- ``GUInt64``   --> ``uint64_t``
- ``GIntBig``   --> ``int64_t``
- ``GUIntBig``  --> ``uint64_t``

Not yet done in the candidate implementation, but candidates:

- ``GSpacing``    -->  ``int64_t``   (affects mostly C++ raster drivers)
- ``GPtrDiff_t``  -->  ``ptrdiff_t`` (affects the multidimensional C and C++ API)

The following macro replacement are done in the whole code base:

- ``CPL_FRMT_GIB``  --> ``"%" PRId64``
- ``CPL_FRMT_GUIB`` --> ``"%" PRIu64``
- ``GINT64_MAX``    --> ``std::numeric_limits<int64_t>::max()``
- ``GINT64_MIN``    --> ``std::numeric_limits<int64_t>::min()``
- ``GUINT64_MAX``   --> ``std::numeric_limits<uint64_t>::max()``
- ``GINTBIG_MAX``   --> ``std::numeric_limits<int64_t>::max()``
- ``GINTBIG_MIN``   --> ``std::numeric_limits<int64_t>::min()``
- ``GUINTBIG_MAX``  --> ``std::numeric_limits<uint64_t>::max()``

The old types are no longer used, and usable, in the GDAL code base since
their definition is protected by ``#if !defined(GDAL_COMPILATION)``, which
means they are still usable by external code.

Impacts in the code base
------------------------

Significant part of the code base (735 files changed).
Most changes have been done in a automated way, with manual changes
specifically for the CPL_FRMT_GIB/CPL_FRMT_GUIB replacement
which was harder to automate.

SWIG bindings
-------------

While .i files are impacted to cope with the C type changes, the language
specific API of SWIG bindings is not impacted.

Backward compatibility
----------------------

C and C++ API and ABI are impacted.

Main impacts are:

* GBool was aliased to ``int``: changing to ``bool`` in C++ methods of the
  OGRStyleXXXX classes affect the API and ABI. Impact should be modest as this
  functionality is thought to be marginally used, and this only impacts the C++ API
  (the C API for those methods uses ``int`` and not ``GBool``)

* GIntBig/GInt64 was aliased to ``long long`` and GUIntBig/GUInt64 to
  ```unsigned long long``. While ``int64_t`` and ``long long`` have in practice
  same width and signedness, they are formally different data types.
  For scalar usage of those types, compilers shouldn't warn.
  But for usage of those types as pointers, compilers warn in C
  (``-Wincompatible-pointer-types`` with gcc) and error outs in C++
  (unless ``-fpermissive`` is passed), when mixing a
  ``int64_t`` pointer and a ``long long`` pointer (similarly for their unsigned
  counterparts).

  C Raster API impacts (and equivalent C++ methods):

  - :cpp:func:`GDALGetDefaultHistogramEx` (e.g. used by QGIS)
  - :cpp:func:`GDALSetDefaultHistogramEx`
  - :cpp:func:`GDALGetRasterHistogramEx` (e.g. used by QGIS)
  - :cpp:func:`GDALGetVirtualMemAuto`

  C Vector API impacts (and equivalent C++ methods):

  - :cpp:func:`OGR_F_GetFieldAsInteger64List`
  - :cpp:func:`OGR_F_SetFieldInteger64List`

  C Multidimension API impacts (and equivalent C++ methods):

  - :cpp:func:`GDALGroupCreateDimension`
  - :cpp:func:`GDALGroupCreateAttribute`
  - :cpp:func:`GDALMDArrayRead`
  - :cpp:func:`GDALMDArrayWrite`
  - :cpp:func:`GDALMDArrayAdviseRead`
  - :cpp:func:`GDALMDArrayAdviseReadEx`
  - :cpp:func:`GDALMDArrayCreateAttribute`
  - :cpp:func:`GDALMDArrayResize`
  - :cpp:func:`GDALMDArrayGetBlockSize`
  - :cpp:func:`GDALMDArrayGetStatistics`
  - :cpp:func:`GDALMDArrayComputeStatistics`
  - :cpp:func:`GDALMDArrayComputeStatisticsEx`
  - :cpp:func:`GDALAttributeGetDimensionsSize`

* Out-of-tree drivers are also impacted:

  - the vector ones that implement :cpp:func:`OGRLayer::ISetFeature`,
    :cpp:func:`OGRLayer::SetNextByIndex`, :cpp:func:`OGRLayer::DeleteFeature`,
    :cpp:func:`OGRLayer::GetFeatureCount`

  - the ones that implement the multidimensional API (no publicly ones known by us)

Questions to answer before adoption
-----------------------------------

Q1: What to do with the 64-bit integer changes?

A1: Potential alternatives:

1) Proceed with them, keep GDAL 3.8 as version number. External code will have
   to use #ifdef if support for GDAL < 3.8 and >= 3.8 is needed.

2) Same as 1), but bump GDAL version number to 4.0. Cleaner way

3) Revert all 64-bit integer related changes

4) Partial revert of 64-bit integer related changes, to reintroduce
   GIntBig/GUIntBig for the above mentionned methods of the C API
   (i.e. GDALGetDefaultHistogramEx, etc), but not the C++ one.
   This preserves C API.

5) Variant of 4) where the revert is done only for the C raster and vector API,
   but the thought-to-be-marginally-used multidimensional one uses the
   int64_t/uint64_t types.


Q2: Should the ``#if !defined(GDAL_COMPILATION)`` in cpl_port.h that controls
whether the old types are accessible be changed to
``#if !defined(GDAL_COMPILATION) && defined(GDAL_USE_OLD_INT_TYPES)``,
so that users have to opt-in to use the legacy types?


Q3: Should ``vsi_l_offset`` be replaced by ``uint64_t``? (they are now aliased
through a typedef)

A3: I'm 50% 50% on this. The name captures a semantic, which can be interesting
to preserve (like they is a ``off_t`` type in Unix file API)

Risks
-----

The changes of this RFC are somewhat risky, particularly the replacement of the
formatting macros CPL_FRMT_GIB/CPL_FRMT_GUIB with the PRId64/PRIu64 ones, which
requires to add a ``%`` formatting character. While compilers caught most of the
mismatches, there were remaining ones undetected at compilation time. Manual
corrections have been done to make the regression test suite pass.
Additional "grep"-based searches in the code base have been done to find faulty
patterns, but we cannot exclude that some might have been missed.

Documentation
-------------

MIGRATION_GUIDE.TXT will be updated to point to this RFC.

Testing
-------

No changes in Python tests. Updates in the C++ test suite.`

Related issues and PRs
----------------------

- https://github.com/OSGeo/gdal/issues/8288: Consider using standard C/C++
  integer types

- https://github.com/OSGeo/gdal/pull/8396: candidate implementation

Voting history
--------------

TBD

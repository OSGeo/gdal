.. _rfc-95:

==================================================================
RFC 95: Use standard C/C++ integer types (proposed, *NOT* adopted)
==================================================================

============== =============================================
Author:        Even Rouault
Contact:       even.rouault @ spatialys.com
Started:       2023-Sep-15
Status:        Proposed, but *not* adopted
Target:        GDAL 4.0
============== =============================================

Summary
-------

This RFC replaces the use of the custom integer types defined in cpl_port.h
(G[U]Int[8/16/32/64/Big]) by their standard [u]int[8/16/32/64]_t C99/C++11
counterparts, as well as other derived integer data types.
Due to the API and ABI break, this will be implemented in GDAL 4.0.

Motivation
----------

- The existing Gxxxxx typedefs have been defined at a time that predates
  C99 and C++11 which introduced those fixed with integer types.
  If GDAL would be created today, we would use C99 data types.

- Using those aliases, particularly GIntBig/GUIntBig which are not
  self-explanatory on their actual width (64 bit), adds cognitive load to
  developers.

- Due to their poor namespacing, those short typenames may occasionally clash
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

Other changes have been done for other integer data types:

- ``vsi_l_offset``-->  ``uint64_t``
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

The old types are no longer used in the GDAL code base since
their definition is protected by ``#ifdef GDAL_USE_OLD_INT_TYPES``, which
external code might define to help for the migration (particularly if supporting
GDAL < 4.0 and GDAL >= 4.0 is needed)

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

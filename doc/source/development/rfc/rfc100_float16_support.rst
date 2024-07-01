.. _rfc-100:

=============================
RFC 100: Support float16 type
=============================

============== =============================================
Author:        Erik Schnetter
Contact:       eschnetter @ perimeterinstitute.ca
Started:       2024-Jun-05
Status:        Proposed
Target:        GDAL 3.10
============== =============================================

Summary
-------

This RFC adds support for the IEEE 16-bit floating point data type
(aka ``half``, ``float16``). It adds a new pixel data type
``GDT_Float16``. Where supported by the compiler, it also ensures a
the C/C++ type ``_Float16`` is available. Even when the local system
does not support ``_Float16``, datasets of this type can still be
accessed by converting from/to ``float``.

Some drivers, in particular Zarr, will be extended to support this
type. Drivers that do not support this type will be checked to ensure
they report an appropriate errors when necessary.

Motivation
----------

- The float16 type is increasingly common. It is supported by GPUs, by
  modern CPUs, and by modern compilers (e.g. GCC 12).

- Using ``uint16`` values to shuffle ``float16`` values around is
  inconvenient and awkward. There is no mechanism in GDAL to mark
  attributes or datasets which ``uint16`` values should be interpreted
  as ``float16``.

- Some drivers (at least the HDF5, GTiff, and Zarr) already handle
  float16 by exposing it as float32, using software conversion
  routines. This type should be supported without converting to
  float32.

- C++23 will introduce native support for ``std::float16_t``. However,
  it will likely be several years until C++23 will be a requirement
  for GDAL. A shorter-term solution is needed.

- Many other projects and storage libraries support float16. GDAL will
  fall behind if it doesn't.

Details
-------

The type ``_Float16`` is defined in both C and C++, if supported by
the compiler. This is the type name that C will (most likely) use in
the future. Modern GCC releases provide this type already. OTher
compilers might support this type under a different name, and in this
case, GDAL will define ``_Float16`` as type alias.

The preprocessor symbol ``GDAL_HAVE_GFLOAT16`` indicates whether
``_Float16`` is available at compile time. This information will also
be available at run time via ``bool GDALHaveFloat16()``.

The following pixel data types are added:
- ``GDT_Float16``  --> ``_Float16``
- ``GDT_CFloat16`` --> ``std::complex<_Float16`` / ``_Complex _Float16``

These enum values are added independent of the value of whether the
compiler supports float16.

Some drivers (at least the HDF5, GTiff, and Zarr) already handle
float16 by exposing it as float32, using software conversion routines.
float16 is now supported directly, i.e., without converting to
float32, if the compiler supports float16. Otherwise, the current
behaviour is retained, which automatically converts float16 to
float32.

For simplicity there are no new functions handling attributes for
multidimensional datasets. Attributes of type float16 can still be
read/written as raw values.

Impacts in the code base
------------------------

It is likely that a large fraction of the code base will be impacted.

SWIG bindings
-------------

The SWIG bindings are extended as if the system supported float16
values to provide a uniform ABI. If the GDAL backend does not support
float16, as indicated by ``GDALHaveFloat16``, an error is reported.

Backward compatibility
----------------------

C and C++ API and ABI are impacted. This design is backward-compatible
manner, i.e. it does not break the ABI, and can thus be implemented in
GDAL 3.x.

Main impacts are:

* Two new enum values for ``GDALDataType``

* Some drivers already support float16 by converting from/to float32.
  This capability remains in a backward-compatible manner. In
  addition, these drivers might (if implemented) now also support
  reading/writing float16 values from/to float16 arrays in memory.

* C Multidimension API impacts (and equivalent C++ methods):

  - :cpp:func:`GDALGroupCreateAttribute`
  - :cpp:func:`GDALMDArrayRead`
  - :cpp:func:`GDALMDArrayWrite`

Risks
-----

The changes of this RFC add new features without removing or disabling
any current features. The risk should be low.

Documentation
-------------

To be written.

Testing
-------

The C++ test suite will be updated. Tests will be implemented in Python.

Related issues and PRs
----------------------

- https://github.com/OSGeo/gdal/issues/10144: Supporting float16

For comparison:

- https://github.com/OSGeo/gdal/pull/5257: [FEATURE] Add (initial)
  support Int64 and UInt64 raster data types

No candidate implementation exists yet.

Voting history
--------------

TBD

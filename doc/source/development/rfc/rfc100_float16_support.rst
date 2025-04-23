.. _rfc-100:

=============================
RFC 100: Support float16 type
=============================

============== =============================================
Author:        Erik Schnetter
Contact:       eschnetter @ perimeterinstitute.ca
Started:       2024-Jun-05
Status:        Adopted, implemented
Target:        GDAL 3.11
============== =============================================

Summary
-------

This RFC adds support for the IEEE 16-bit floating point data type
(aka ``half``, ``float16``). It adds new pixel data types
``GDT_Float16`` and ``GDT_CFloat16``, backed by a new ``GFloat16`` C++
type. This type will be emulated in software if not available
natively.

Some drivers, in particular Zarr, will be extended to support this
type. Drivers that do not support this type will be checked to ensure
they report an appropriate error when necessary.

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
  for GDAL.

- Many other projects and storage libraries support float16. GDAL will
  fall behind if it doesn't.

Details
-------

A new type ``GFloat16`` will be defined in C++ in :file:`cpl_float.h`
which will be an exported header of GDAL. This type will work in one
of these ways:

- if the standard C++ type ``std::float16_t`` is available,
  ``GFloat16`` will be an alias to this type.

- otherwise, if a fully-functional non-standard type is available,
  ``GFloat16`` will be an alias to that type.

- otherwise, if a float16-type is available that is not supported by
  the standard C++ library (such as ``_Float16`` for which e.g.
  ``std::isnan`` or ``std::numeric_limits`` may not be defined), then
  ``GFloat16`` will be a thin wrapper around that type, adding support
  for C++ library functions.

- otherwise, ``GFloat16`` will be a new type that emulates float16
  behavior (transparently to the user), and operations will be
  performed as ``float``.

Experimentation has shown that this is the most convenient way to
handle lack of support or partial support for a float16 type, both in
terms of implementation within GDAL and in terms of using GDAL as a
C++ library.

The following pixel data types are added:
- ``GDT_Float16``  --> ``GFloat16``
- ``GDT_CFloat16`` --> ``std::complex<GFloat16>``

Some drivers (at least the HDF5, GTiff, and Zarr) already handle
float16 by exposing it as float32, using software conversion routines.
float16 is now supported directly, i.e., without converting to
float32. In other drivers, the current behavior is retained, which
automatically converts float16 to float32.

For simplicity there will be no new functions handling attributes for
multidimensional datasets. Attributes of type float16 can still be
read/written as raw values.

Impacts in the code base
------------------------

It is likely that a large fraction of the code base will be impacted.

SWIG bindings
-------------

The SWIG bindings are extended where possible. Unfortunately, SWIG
does not support the native float16 Python type, but it does support
the float16 numpy type. This means that not all SWIG Python wrappers
can support float16.

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

Documentation will be added.

Testing
-------

The C++ test suite will be updated. Tests will be implemented in C++
and Python.

Related issues and PRs
----------------------

- https://github.com/OSGeo/gdal/issues/10144: Supporting float16

For comparison:

- https://github.com/OSGeo/gdal/pull/5257: [FEATURE] Add (initial)
  support Int64 and UInt64 raster data types

No candidate implementation exists yet.

Voting history
--------------

+1 from PSC members KurtS, DanB, JavierJS, JukkaR and EvenR


.. below is an allow-list for spelling checker.

.. spelling:word-list::
    Schnetter
    eschnetter
    perimeterinstitute

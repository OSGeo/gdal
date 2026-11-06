.. _rfc-113:

=============================
RFC 113: Build requirements
=============================

============== =============================================
Author:        Kurt Schwehr, Gemini 4 Argon
Contact:       schwehr@gmail.com
Started:       2026-Oct-18
Status:        Draft
Target:        GDAL 3.14
============== =============================================

Summary
-------

This document updates and supersedes :ref:`rfc-98` and :ref:`rfc-68` with the
build requirements for GDAL 3.14:

- C++ >= 20
- C >= 99 (C23 ``#embed`` is used opportunistically when available)
- CMake >= 3.22
- PROJ >= 9
- Ubuntu >= 22.04 (advancing to newer LTS releases as the oldest LTS exits
  "standard security maintenance" per https://ubuntu.com/about/release-cycle)
- Windows >= Windows Server 2022 / Windows 11
- macOS >= 14 (Sonoma) with Xcode >= 15

The minimum versions for optional dependencies are:

- Python >= 3.11 (support for a Python version will be dropped when it reaches
  end-of-life as described in https://devguide.python.org/versions/)
- SWIG >= 4.0.2
- Java >= 17
- .NET SDK >= 8.0 (C# >= 10; Mono support dropped per :ref:`rfc-112`)
- GTest >= 1.11.0
- GEOS >= 3.11
- libcurl >= 7.81
- libtiff >= 4.3
- libpng >= 1.6.37
- libsqlite3 >= 3.37
- libspatialite >= 5.0.1
- librasterlite2 >= 1.1.0
- libhdf5 >= 1.10.7
- libnetcdf >= 4.8 and built with NC4 enabled
- OpenEXR >= 2.5
- libheif >= 1.12
- Poppler >= 22.02
- libopenjp2 (OpenJPEG) >= 2.4.0
- TileDB >= 2.15
- Grok (https://github.com/GrokImageCompression/grok) >= 20.3.9
- Shapelib >= 1.5.0 (when using an external Shapelib; internal is used by default)
- Kakadu >= 7.5
- ECW >= 3.3
- libOpenDRIVE >= 0.6.0

Details
-------

This RFC updates the C++ standard, CMake, PROJ, Python, GEOS, and optional
dependency minimum version requirements to match the oldest supported versions
(and at least the versions shipped in ``ubuntu-22.04``, keeping GDAL's existing
minimum when it is already newer):

- **C++ >= 20**: Our previous requirement in :ref:`rfc-98` was C++17. Several
  dependencies (such as Poppler >= 24.05 and PDFium) already require C++20 for
  their headers, and drivers such as PDF, Arrow, and Parquet already have
  provisions for C++20. Requiring C++20 across the codebase simplifies build
  configuration and allows using C++20 language and standard library features.
  To minimize disruption for external C++ consumers of GDAL, exported public
  headers will continue to avoid requiring C++20 unless necessary or decided
  otherwise.

- **C >= 99**: C99 remains the portable baseline for the C sources and the
  public C headers. C23 is not required: the only C23 feature in use is
  ``#embed`` for embedded resources (:ref:`rfc-102`), which is enabled
  per-target only when ``is_sharp_embed_available()`` in ``gdal.cmake``
  detects a compiler that supports it (GCC >= 15 or Clang >= 19), and falls
  back to generated C arrays otherwise.

- **Python >= 3.11**: Python 3.8, 3.9, and 3.10 have reached end-of-life as of
  October 2026 (https://devguide.python.org/versions/). Updating the minimum
  supported Python version from 3.8 to 3.11 aligns GDAL with currently
  supported upstream Python releases and allows removing legacy compatibility
  workarounds in the SWIG Python bindings and test suite. Going forward, Python
  version support will be dropped when a version reaches end-of-life as
  described in https://devguide.python.org/versions/. Ubuntu 22.04 ships
  Python 3.10; the ``ubuntu-22.04`` CI configuration will obtain Python 3.11
  from another source (such as the deadsnakes PPA or conda).

- **GEOS >= 3.11**: GEOS 3.10 and older are unsupported upstream
  (https://libgeos.org/usage/download/). Updating the minimum version from 3.8
  to 3.11 allows removing conditional compilation paths for older GEOS
  versions (such as in ``MakeValid()``, geometry precision handling, and
  ``ConcaveHull()``). Ubuntu 22.04 ships GEOS 3.10.2; the ``ubuntu-22.04`` CI
  configuration will obtain GEOS >= 3.11 from UbuntuGIS or build it from
  source.

- **CMake >= 3.22**: Ubuntu 22.04 includes CMake 3.22.1. Updating the baseline
  minimum version from 3.16 to 3.22 satisfies the requirements for C23
  ``#embed`` support (CMake >= 3.21) and ``GDAL_USE_PCODEC=ON`` in the Zarr
  driver (CMake >= 3.22), and allows removing compatibility workarounds and
  backported find modules for CMake < 3.22.

- **PROJ >= 9**: PROJ 9.0.0 was released in March 2022. Updating the minimum
  required version from 6.3.1 to 9 allows removing the
  ``#if PROJ_VERSION_MAJOR < 9`` (and ``>= 7``, ``>= 8``) conditional
  compilation paths (such as in ``ogr_proj_p.cpp``, ``ogrct.cpp``,
  ``ogrspatialreference.cpp``, and ``gdalsrsinfo.cpp``), and the CMake logic
  that distrusts the ``PROJ`` config file for PROJ < 8. Ubuntu 22.04 ships
  PROJ 8.2.1; the ``ubuntu-22.04`` CI configuration will obtain PROJ >= 9 from
  UbuntuGIS or build it from source.

- **Ubuntu >= 22.04 (ubuntu-22.04)**: Ubuntu 20.04 reached the end of its
  5-year standard security maintenance window in May 2025. The minimum
  supported Ubuntu version is ``ubuntu-22.04``, and will be moved up to newer
  LTS releases as the oldest LTS goes out of "standard security maintenance"
  per https://ubuntu.com/about/release-cycle.

- **Windows >= Windows Server 2022 / Windows 11**: Windows 10 reached end of
  support in October 2025 and Windows Server 2019 left mainstream support in
  January 2024. GitHub retired the ``windows-2019`` runner in June 2025, so
  ``windows-2022`` is the oldest Windows environment exercised by CI. Visual
  Studio 2022 (MSVC >= 19.30) is the oldest supported toolchain on those
  releases.

- **macOS >= 14 (Sonoma) with Xcode >= 15**: ``std::bit_cast`` and the rest
  of the C++20 library features listed below require Apple Clang >= 14.0.3
  (Xcode 14.3), which in turn requires macOS >= 13. macOS 13 no longer
  receives security updates from Apple since the release of macOS 26 in
  September 2026, Homebrew (used by the macOS CI) only supports the three most
  recent macOS releases, and GitHub retired the ``macos-13`` runner in
  December 2025. macOS 14 is therefore the oldest release on which GDAL can be
  built and tested with supported tooling; Xcode 15 is the first Xcode that
  targets it. This is a build and CI requirement only; packagers (conda-forge,
  Homebrew, MacPorts) remain free to set an older deployment target.

- **Optional dependencies**: Minimum versions for optional dependencies are
  updated to match the oldest supported versions and the versions available in
  Ubuntu 22.04 (while retaining GDAL's existing minimum where it is newer):

  - **Java >= 17**: Java 17 is the oldest Java LTS release with upstream
    support from the main vendors (OpenJDK 11 updates from Oracle ended in
    2023, Ubuntu 22.04 ships OpenJDK 17, and the ``ubuntu-22.04`` CI image
    currently installs OpenJDK 8, which will be updated). SWIG 4.0.2 generates
    code compatible with Java 17.
  - **.NET SDK >= 8.0**: .NET 6.0 reached end-of-support in November 2024
    (https://dotnet.microsoft.com/en-us/platform/support/policy/dotnet-core)
  - **SWIG >= 4.0.2** and **GTest >= 1.11.0**: versions in Ubuntu 22.04
  - **Poppler >= 22.02**: removes ``#if POPPLER_MAJOR_VERSION < 21`` checks in
    the PDF driver
  - **libtiff >= 4.3**: removes ``#if TIFFLIB_VERSION > 20210416`` checks in
    the GeoTIFF driver
  - **libcurl >= 7.81**: version in Ubuntu 22.04
  - **libpng >= 1.6.37**: version in Ubuntu 22.04
  - **libsqlite3 >= 3.37**: removes ``#if SQLITE_VERSION_NUMBER >= 3035005L``
    checks in the SQLite driver
  - **libspatialite >= 5.0.1**: removes legacy Spatialite 4.x compatibility paths
  - **libopenjp2 (OpenJPEG) >= 2.4.0**: removes ``#if IS_OPENJPEG_OR_LATER(2, 4, 0)``
    checks in the OpenJPEG driver
  - **libnetcdf >= 4.8** (built with NC4 enabled) and **libhdf5 >= 1.10.7**:
    versions in Ubuntu 22.04
  - **OpenEXR >= 2.5**: version in Ubuntu 22.04
  - **libheif >= 1.12**: removes ``#if LIBHEIF_HAVE_VERSION(1, 7, 0)`` checks
    in the HEIF driver
  - **Shapelib >= 1.5.0**: version in Ubuntu 22.04 (when using external Shapelib)
  - **librasterlite2 >= 1.1.0**, **TileDB >= 2.15**,
    **Grok** (https://github.com/GrokImageCompression/grok) **>= 20.3.9**,
    **Kakadu >= 7.5**, **ECW >= 3.3**, and **libOpenDRIVE >= 0.6.0**: retained
    from GDAL's existing configuration

C++20 capable compilers
-----------------------

Based on https://en.cppreference.com/w/cpp/compiler_support and the per-feature
table at https://en.cppreference.com/cpp/compiler_support/20, compilers
supporting the subset of C++20 used by GDAL are:

- GCC >= 11
- Clang >= 14
- Apple Clang >= 15 (Xcode >= 15)
- Visual Studio >= 2022 (MSVC 19.30)

Only features supported by all of the above may be used. In particular
``std::format``, modules, coroutines, and most of ``<ranges>`` are not yet
available across that set and remain off limits for now.

C++20 features
--------------

Features that can be used in the codebase (not an exhaustive list):

- ``std::span`` for non-owning contiguous views
- ``std::string::starts_with`` and ``std::string::ends_with``
- ``contains()`` on associative containers (``std::map``, ``std::set``, etc.)
- ``<bit>`` utilities (such as ``std::bit_cast``, ``std::countl_zero``, ``std::popcount``)

Banned features:

- Use of ``std::filesystem`` (https://en.cppreference.com/w/cpp/filesystem)
  remains disallowed since our existing :cpp:class:`VSIFilesystemHandler`
  mechanism has broader support for all ``/vsi`` virtual file systems.

Changes in continuous integration
---------------------------------

Continuous integration configurations are updated to use compilers and
environments meeting at least the new build requirements, with ``ubuntu-22.04``
as the oldest Ubuntu LTS environment (with PROJ, GEOS, Python, and Java
obtained from UbuntuGIS, deadsnakes, or source where the distribution packages
are older than the minimums), ``windows-2022`` as the oldest Windows
environment, and ``macos-14`` as the oldest macOS environment, advancing as
older releases leave support.

Changes in SWIG bindings
------------------------

The Python SWIG bindings require Python >= 3.11. Workarounds for Python 3.8,
3.9, and 3.10 in ``swig/python/`` and ``autotest/`` can be removed. The Java
bindings are compiled with ``--release 17``.

Backward compatibility
----------------------

No change to the C API. External projects building against GDAL's C++ headers
or building GDAL from source will require a C++20 capable compiler if C++20
constructs are used in headers they include, Python >= 3.11 when building
or running the Python bindings, and Java >= 17 when using the Java bindings.
Binary wheels for Python 3.8, 3.9, and 3.10 will no longer be published.

Documentation
-------------

The Build requirements and Development practices documentation pages will be
updated to reflect the new minimum versions.

Testing
-------

The existing autotest suite should continue to pass.

Related RFCs
------------

- This RFC supersedes :ref:`rfc-68` and :ref:`rfc-98`
- :ref:`rfc-112` (Drop Mono Support from CMake build system)

Voting history
--------------

TBD

.. below is an allow-list for spelling checker.

.. spelling:word-list::
    GTest
    GrokImageCompression
    Kakadu
    MacPorts
    OpenEXR
    OpenJDK
    PCODEC
    PDFium
    Poppler
    Shapelib
    Sonoma
    Spatialite
    TileDB
    UbuntuGIS
    Xcode
    deadsnakes
    libOpenDRIVE
    libheif
    libopenjp
    librasterlite
    libspatialite

.. _rfc-98:

=======================================
RFC 98: Build requirements for GDAL 3.9
=======================================
============== =============================================
Author:        Even Rouault
Contact:       even.rouault @ spatialys.com
Started:       2023-Nov-23
Status:        Adopted, implemented
Target:        GDAL 3.9
============== =============================================

Summary
-------

The document updates :ref:`rfc-68` with the new build requirements for GDAL 3.9:

- C++ >= 17
- CMake >= 3.16
- PROJ >= 6.3.1.

The minimum version for the following optional dependencies is also updated:

- Python >= 3.8
- GEOS >= 3.8
- Poppler >= 0.86
- libtiff >= 4.1
- libcurl >= 7.68
- libpng >= 1.6.0
- libsqlite3 >= 3.31
- libopenjp2 >= 2.3.1
- libnetcdf >= 4.7 and built with NC4 enabled
- libhdf5 >= 1.10

Details
-------

Our past build requirements were C++11, CMake 3.9, and PROJ 6.0.0. All of them
correspond to now outdated versions of those components, and it makes sense to
update to more up-to-date versions to be able to clean up code, leverage new
capabilities and be consistent with the current state of our software environment.

The proposed updates are all compatible with versions of those components
available by default in the old LTS (Long Term Support) Ubuntu 20.04, which
corresponds to the oldest environment used by our continuous integration:

- C++17 is the minimum version required by the latest versions of some of our
  C++ dependencies, including Poppler, PDFium, PoDoFo, TileDB, libarrow-cpp.
  Ubuntu 20.04 includes GCC 9.4, which supports C++17. While we want to allow
  C++17 features to build GDAL, for now, we will stick to exposing at most
  C++11 features in the exported headers of the library to minimize disruption
  for GDAL C++ users. That might be revisited later.
  At the time of writing, the C++17 requirement has already been implemented in
  master / 3.9.0dev.

- CMake 3.16.0 was released in November 2019. Updating to CMake 3.16 enables us
  to make a number of cleanups in GDAL CMakeLists.txt scripts, and in particular
  to make it possible to use the
  [CMAKE_UNITY_BUILD](https://cmake.org/cmake/help/latest/variable/CMAKE_UNITY_BUILD.html)
  feature. Ubuntu 20.04 includes CMake 3.16.4.

- PROJ 6.3.1 was released in February 2020. Updating to that version implies
  PROJ >= 6.2 and the availability of PROJJSON output, and PROJ >= 6.3 enables
  us to remove a few specific code paths in ogrct.cpp. Ubuntu 20.04
  includes PROJ 6.3.1. Earlier PROJ 6.x versions had a number of annoying issues.

By the time GDAL 3.9 is released (May 2024), all the above requirements
correspond to versions of the tools/libraries that have been released more than
4 years earlier.

More generally, we also update requirements for optional dependencies to be
consistent with the versions available in Ubuntu 20.04, to eliminate code paths
that are no longer exercised by our continuous integration:

- Python >= 3.8: Python 3.7 is already end-of-life (https://devguide.python.org/versions/)
  Python 3.8 is the minimum version used by our CI
- GEOS >= 3.8: ensures that MakeValid() is available when GEOS is available,
  which simplifies the code base and test suite
- Poppler >= 0.86: removes a lot of #ifdef trickery in the PDF driver
- libtiff >= 4.1: simplifies a few code paths in the GeoTIFF driver
- libcurl >= 7.68: removes outdated code paths in CPL networking code
- libpng >= 1.6.0: removes outdated code paths in the PNG driver
- libsqlite3 >= 3.31: removes outdated code paths in the SQLite and GPKG drivers
- libopenjp2 >= 2.3.1: removes outdated code paths in the OpenJPEG driver
- libnetcdf >= 4.7 built with NC4 support enabled (i.e. libnetcdf built against
  libhdf5): removes #ifdef code paths in netCDF driver. The netCDF multidimensional
  code already requires NC4.
- libhdf5 >= 1.10: removes outdated code paths in the HDF5 driver

C++17 capable compilers
-----------------------

From https://en.wikipedia.org/wiki/C%2B%2B17, compilers supporting C++17 are:

- GCC >= 8
- clang >= 5
- Visual Studio >= 2017 15.8 (MSVC 19.15)

C++14 and C++17 features
------------------------

Features that can be used in the code base (not an exhaustive list):

- Use of ``std::make_unique<>``  instead of ``cpl::make_unique<>``
- Use of ``[[fallthrough]]`` instead of ``CPL_FALLTHROUGH``
- Use of ``[[maybe_unused]]`` instead of ``CPL_UNUSED``
- Nicer iteration in ``std::map`` with ``for (const auto &[key, value]: my_map )``
  (more generally "structured binding declarations")

Banned features:

- Use of ``std::filesystem`` (https://en.cppreference.com/w/cpp/filesystem)
  is not appropriate since our existing :cpp:class:`VSIFilesystemHandler`
  mechanism has broader support for all our /vsi specific file systems.

Changes in continuous integration
---------------------------------

Continuous integration is modified to test configurations that have at least
the new set of build requirements.

Changes in SWIG bindings
------------------------

None

Backward compatibility
----------------------

No change in API and ABI

Documentation
-------------

The Build requirements documentation page will be updated.

Testing
-------

The existing autotest suite should continue to pass.

Related tickets / pull requests
-------------------------------

- https://github.com/OSGeo/gdal/pull/8680: CI: Remove Ubuntu 18.04 configurations
- https://github.com/OSGeo/gdal/issues/8270: Bump to C++17
- https://github.com/OSGeo/gdal/pull/8687: Switch default C++ standard to C++17
- https://github.com/OSGeo/gdal/pull/8710: Cxx17 tunings
- https://github.com/OSGeo/gdal/pull/8716: C++17: use if constexpr() construct
- https://github.com/OSGeo/gdal/pull/8723: C++17: replace CPL_FALLTHROUGH by standard [[fallthrough]];
- https://github.com/OSGeo/gdal/pull/8725: C++17: use structured bindings declaration and class template argument deduction for std::pair()
- https://github.com/OSGeo/gdal/issues/8751: Bumping minimum CMake version to 3.16
- https://github.com/OSGeo/gdal/issues/8796: Add CI test we can use GDAL public/installed headers with C++11
- https://github.com/OSGeo/gdal/pull/8804: Make build compatible of -DCMAKE_UNITY_BUILD=ON for faster builds

Related RFCs
------------

- This RFC supersedes :ref:`rfc-68`

Voting history
--------------

+1 from PSC members JavierJS, KurstS, HowardB, JukkaR and EvenR

# GDAL build instructions

This file contains and points to instructions about building GDAL from source.

# Building with cmake (GDAL >= 3.5.0)

CMake is the only build system supported since GDAL 3.6.0.

There is a [build hints](https://gdal.org/build_hints.html) page on the website.

Beyond that page, note:
  - cmake builds in the source directory are not supported (expected to fail)

# Building with autoconf or nmake (GDAL < 3.6.0)

autoconf and nmake build systems were only available in GDAL < 3.5.0, have
been deprecated in GDAL 3.5.x, and finally completely removed in GDAL 3.6.0.

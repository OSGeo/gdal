# GDAL build instructions

This file contains and points to instructions about building GDAL from source.

# Building with cmake

There is a [build hints](https://gdal.org/build_hints.html) page on the website.

Beyond that page, note:
  - cmake builds in the source directory are not supported (expected to fail)

# Building with autoconf

Building with autoconf is deprecated.  While there are many --with
options, the build system mostly follows autoconf norms.

# Building from vcpkg

The gdal port in vcpkg is kept up to date by Microsoft team members and community contributors. The url of vcpkg is: https://github.com/Microsoft/vcpkg . You can download and install gdal using the vcpkg dependency manager:

```shell
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh  # ./bootstrap-vcpkg.bat for Windows
./vcpkg integrate install
./vcpkg install gdal
```

If the version is out of date, please [create an issue or pull request](https://github.com/Microsoft/vcpkg) on the vcpkg repository.

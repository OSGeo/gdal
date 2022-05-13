## Post-install tests

These tests are performed with an *installed* GDAL library. The test_pkg-config.sh script checks that pkg-config on *nix platforms can discover the correct compilation and linking options to build C and C++ applications. The test_cmake.sh script tests building with C and C++  applications with CMake.

To run these tests, use the test script with the install prefix as the first argument, for example if GDAL was configured with `--prefix=/tmp/gdal` (or `-DCMAKE_INSTALL_PREFIX=/usr/local`):
```bash
./autotest/postinstall/test_pkg-config.sh /tmp/gdal
```
the prefix is used to set both `PKG_CONFIG_PATH` and `LD_LIBRARY_PATH` (or `DYLD_LIBRARY_PATH`) environment variables during testing.

If the GDAL library was built as a static library, append `--static` to disable tests for shared libraries.

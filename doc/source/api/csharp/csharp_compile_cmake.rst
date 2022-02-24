.. _csharp_compile_cmake:

================================================================================
Compiling the C# bindings - CMake Scripts
================================================================================

This page describes the primary steps when creating the GDAL/OGR C# binaries from the source using the new CMake scripts.

In most cases this is not necessary and it is better to use one of the pre-compiled sources, such as `GisInternals <https://gisinternals.com/>`__ or Conda.

You can either build the bindings as part of a full GDAL build - or standalone on top of an existing installation.

Requirements
++++++++++++

The build environment has the following dependencies:

* CMake 3.10 or later
* the appropriate C++ build environment (i.e. gcc or Visual Studio etc).
* SWIG 4
* .NET 5.0 or Mono

.NET Build Toolchain
++++++++++++++++++++

The build scripts can use either .NET 5.0 and  :file:`dotnet.exe` or Mono and :file:`msc.exe` to compile the bindings.

.NET is used for preference if it found on all platforms but the use of Mono can be forced using a command line variable.

Building as part of a GDAL Build
++++++++++++++++++++++++++++++++

The build environment uses the following variables:

+---------------------------+------------+-----------------------------------------------+
| CSHARP_MONO               | Boolean    | Forces the use of Mono                        |
+---------------------------+------------+-----------------------------------------------+
| CSHARP_LIBRARY_VERSION    | String     | Set the .NET version for the shared libraries |
+---------------------------+------------+-----------------------------------------------+
| CSHARP_APPLICATION_VERSION| String     | Set the .NET version for the sample apps      |
+---------------------------+------------+-----------------------------------------------+
| GDAL_CSHARP_ONLY          | Boolean    | Build standalone on GDAL binaries             |
+---------------------------+------------+-----------------------------------------------+
| BUILD_CSHARP_BINDINGS     | Boolean    | Build the C# bindings DEFAULT ON              |
+---------------------------+------------+-----------------------------------------------+

Building with .NET
------------------

If the build environment has .NET 5.0 installed and GDAL is built, then the c# bindings will be built using .NET by default.

The details of building GDAL are documented elsewhere, but there are likely to be variants of the following commands run from the root directory of the gdal repository:

.. code-block::

    cmake -DCMAKE_INSTALL_PREFIX ../install -B ../build -S .
    cmake --build ../build --config Release
    cmake --build ../build --config Release --target install

The C# bindings and sample apps are installed in the install directory (in the above case that would be `../install`, in the `share/csharp` sub folder. There would be the following files:

* :file:`gdal_csharp.dll`
* :file:`ogr_csharp.dll`
* :file:`osr_csharp.dll`
* :file:`gdalconst_csharp.dll`
* :file:`gdal_wrap.dll` or :file:`libgdal_wrap.so` or :file:`libgdal_wrap.dylib`
* :file:`ogr_wrap.dll` or :file:`libogr_wrap.so` or :file:`libogr_wrap.dylib`
* :file:`osr_wrap.dll` or :file:`libosr_wrap.so` or :file:`libosr_wrap.dylib`
* :file:`osr_wrap.dll` or :file:`libosr_wrap.so` or :file:`libosr_wrap.dylib`
* :file:`gdalconst_wrap.dll` or :file:`libgdalconst_wrap.so` or :file:`libgdalconst_wrap.dylib`
* various sample applications - as \*.exe on Windows, or just as \* on Unix, along with \*.dll for each app and the runtime config files.

There are also subdirectories for each of the sample apps, holding the config files.

There are also the following NuGET packages:

* :file:`OSGeo.GDAL`
* :file:`OSGeo.OGR`
* :file:`OSgeo.OSR`
* :file:`OSGeo.GDAL.CONST`
* various sample application

Using the .NET Bindings
-----------------------

The easiest way to use the bindings in development would be use the NuGET packages created.

To do this you need to add a local repistory pointing to the GDAL install directory. `This is explained here <https://docs.microsoft.com/en-us/nuget/hosting-packages/local-feeds>`__ .

Once this is done, you add the GDAL packages into your project as normal.

.. note:: These packages only install the bindings and do not install core GDAL. It is for you as the developer to make sure that the GDAL binaries are available in the search path.


.. note:: The NuGET packages are created with the same version number as the version of GDAL in the build system.
          If you are building in a GIT repository, then the build system automatically makes the version with a x.y.z-dev pre-release tag.
          This means that to load the package into Visual Studio (for instance), you have to tick the pre-release box.
          This is all intentional and not a bug.


Building on Mono
----------------

If the build environment does not have .NET 5.0 or msbuild installed and GDAL is built, then the c# bindings will be built using Mono by default. Mono building can also be forced 
by setting CSHARP_MONO.

The details of building GDAL are documented elsewhere, but the there are likely to be variants of the following commands run from the root directory of the gdal repository:

.. code-block::

    cmake -DCMAKE_INSTALL_PREFIX ../install -DCSHARP_MONO=ON -B ../build -S .
    cmake --build ../build --config Release
    cmake --build ../build --config Release --target install

The C# bindings and sample apps are installed in the install directory (in the above case that would be `../install`, in the `share/csharp` sub folder. There would be the following files:

* :file:`gdal_csharp.dll`
* :file:`ogr_csharp.dll`
* :file:`osr_csharp.dll`
* :file:`gdalconst_csharp.dll`
* :file:`gdal_wrap.dll` or :file:`libgdal_wrap.so` or :file:`libgdal_wrap.dylib`
* :file:`ogr_wrap.dll` or :file:`libogr_wrap.so` or :file:`libogr_wrap.dylib`
* :file:`osr_wrap.dll` or :file:`libosr_wrap.so` or :file:`libosr_wrap.dylib`
* :file:`osr_wrap.dll` or :file:`libosr_wrap.so` or :file:`libosr_wrap.dylib`
* :file:`gdalconst_wrap.dll` or :file:`libgdalconst_wrap.so` or :file:`libgdalconst_wrap.dylib`
* various sample applications as \*.exe on all platforms.

Using the Mono Bindings
-----------------------

Note that the bindings created by this process will only work with Mono.

To run one of the prebuilt executables - you can run them with Mono as follows :

:program:`mono GDALInfo.exe`

Both the managed libraries (i.e. the DLLs) and the unmanaged libraries must be available to Mono.
This is in more detail in `the Mono documentation <https://www.mono-project.com/docs/advanced/pinvoke/>`__ 

Building Standalone
+++++++++++++++++++

The Bindings using both the .NET or Mono toolchains can be build on top of an existing implementation of GDAL
that includes the include files and libs - for instance the Conda distribution.

To do this, Cmake must be run with the GDAL_CSHARP_ONLY flag set and only one of the following targets should be built:


+--------------------------------+---------------------------------+
| csharp_binding                 | Just the bindings               |
+--------------------------------+---------------------------------+
| csharp_samples                 | The bindings and the sample apps|
+--------------------------------+---------------------------------+

.. note:: Do not build the install target when running standalone, it will fail!

.. note:: Do not run a bare ctest command on this build, it will likely fail! Use something like `ctest -R "^csharp.*"` instead.

As an example:

.. code-block::

    cmake -DGDAL_CSHARP_ONLY=ON -B ../build -S .
    cmake --build ../build --config Release --target csharp_samples

The output from this build is axactly the same as documented as above, except that the outputs will be in `../build/swig/csharp` and some of the sub folders.


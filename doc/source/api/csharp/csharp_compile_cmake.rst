.. _csharp_compile_cmake:

================================================================================
Compiling the C# bindings
================================================================================

This page describes the primary steps when creating the GDAL/OGR C# binaries from the source using the new CMake scripts.

In most cases this is not necessary and it is better to use one of the pre-compiled sources, such as `GisInternals <https://gisinternals.com/>`__ or Conda.

You can either build the bindings as part of a full GDAL build - or standalone on top of an existing installation.

Requirements
++++++++++++

See :ref:`build_requirements`

In addition, you will require a valid installation of `dotnet` with at least one SDK installed that can build the requested target frameworks.

C# Version
++++++++++

The C# bindings themselves are written in SWIG and designed to be compatible with `netstandard2.0`.

However, the sample applications are written in C# version 10. Incompatible syntax will break the build.

Building as part of a GDAL Build
++++++++++++++++++++++++++++++++

The build environment uses the following variables:

.. option:: BUILD_CSHARP_BINDINGS:BOOL=ON/OFF

    Whether C# bindings should be built. It is ON by default, but only
    effective if a valid .NET SDK is found.

.. option:: CSHARP_LIBRARY_VERSION

    Sets the .NET target Framework (in TFM format) to be used when compiling the C# binding libraries. `List of acceptable contents for .NET <https://docs.microsoft.com/en-us/dotnet/standard/frameworks#supported-target-frameworks>`_.
    Defaults to `netstandard2.0`.

.. option:: CSHARP_APPLICATION_VERSION

    Sets the .NET target Framework (in TFM format) to be used when compiling the C# sample applications. `List of acceptable contents for .NET <https://docs.microsoft.com/en-us/dotnet/standard/frameworks#supported-target-frameworks>`_. 
    Defaults to the highest version installed on the build system, i.e. `latest`.

.. option:: GDAL_CSHARP_ONLY=OFF/ON

    Build the C# bindings without building GDAL. This should be used when building the bindings on top of an existing GDAL installation - for instance on top of the CONDA package.

.. option:: CSHARP_BUILD_SAMPLES=OFF/ON

    Whether to build the C# sample applications. Defaults to the value of `BUILD_TESTING` (i.e. ON when tests are enabled, OFF otherwise).

.. option:: CSHARP_RUN_TESTS=OFF/ON

    Whether to run the C# tests. Defaults to the value of `CSHARP_BUILD_SAMPLES` (i.e. ON when tests are enabled, OFF otherwise).

.. option:: CSHARP_INSTALL_NUGET_PACKAGE=OFF/ON

    Whether to install the generated NuGet packages for the C# bindings. Defaults to ON.

.. note::

    It is possible using these switches to force the sample apps NOT to be built but to force the tests to be created based on those apps. Those tests are guaranteed to fail.

.. note::

    The C# bindings are made of several modules (OSGeo.GDAL, OSGeo.OGR, etc.)
    which link each against libgdal. Consequently, a static build of libgdal is
    not compatible with the bindings.

Building the Bindings
---------------------

If the build environment has `dotnet` installed, and the `BUILD_CSHARP_BINDINGS` switch is `ON` (the default), then the C# bindings will be built when GDAL is built.

The details of building GDAL are documented elsewhere, but there are likely to be variants of the following commands run from the root directory of the gdal repository:

.. code-block::

    cmake -DCMAKE_INSTALL_PREFIX ../install -B ../build -S .
    cmake --build ../build --config Release
    cmake --build ../build --config Release --target install

The C# bindings and sample apps are installed in the install directory. In the above case that would be `../install`, in the `share/csharp` sub folder. There would be the following files:

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

Using the C# Bindings
-----------------------

The easiest way to use the bindings in development when built in this way is to use the NuGET packages created.

To do this you need to add a local repository pointing to the GDAL install directory. `This is explained here <https://docs.microsoft.com/en-us/nuget/hosting-packages/local-feeds>`__ .

Once this is done, you add the GDAL packages into your project as normal.

.. note:: These packages only install the bindings and do not install core GDAL. It is for you as the developer to make sure that the GDAL binaries are available in the search path.


.. note:: The NuGET packages are created with the same version number as the version of GDAL in the build system.
          If you are building in a GIT repository, then the build system automatically makes the version with a x.y.z-dev pre-release tag.
          This means that to load the package into Visual Studio (for instance), you have to tick the pre-release box.
          This is all intentional and not a bug.


Building Standalone
+++++++++++++++++++

The Bindings can be built on top of an existing installation of GDAL
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

The output from this build is exactly the same as documented as above, except that the outputs will be in `../build/swig/csharp` and some of the sub folders.

Signing of build artifacts
++++++++++++++++++++++++++

The CSharp assemblies are strong name signed by default with the provided key file in :source_file:`swig/csharp/gdal.snk`.
If authenticode signing of the assemblies is wished, it should be done in a post-build
manual step, for example with:

.. code-block::

    signtool sign /f "path\to\your\certificate.pfx" /p "password" /tr http://timestamp.digicert.com /td sha256 "path\to\your\gdal_csharp.dll"

.. _csharp_compile_legacy:

================================================================================
Compiling the C# bindings - Legacy Scripts
================================================================================

This page describes the primary steps when creating the GDAL/OGR C# binaries from the source.

In most cases this is not necessary and it is better to use one of the pre-compiled sources, such as `GisInternals <https://gisinternals.com/>`__ or Conda.

Building on Windows
-------------------

To building the C# interface, you need a compiled version of the GDAL core. This can be the result of a manual compilation or can be linking to one of the prebuilt binaries.
In the former case the following should be run in the clone of the GitHub repository used to build GDAL and the steps to create the environment should not be necessary.

Requirements
++++++++++++

The build environment has the following dependencies:

* nmake / Visual Studio
* SWIG 3/4

.. note:: The `GDAL test scripts <https://github.com/OSGeo/gdal/blob/master/.github/workflows/windows_build.yml>`__ use VS 2019 (MSVC Ver 1920) so it would make sense to use the same versions.

.. note:: `SWIG <http://www.swig.org/>`__ is used to build the API bindings. The GDAL test scripts use version 3 and the conda build use version 4. Both Work.

Build Environment
+++++++++++++++++

You need to set up the build environment. If you are using VS 2019, this might be the command:

:program:`VsDevCmd.bat -arch=x64`

.. note:: The :program:`VsDevCmd.bat` command can usually be found in :file:`C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Enterprise\\Common7\\Tools` or the equivalent for the Community Edition.

If you don't want to bother with executing the proper vcvars*.bat you might use the development environment specific command prompt to accomplish this task. When using a Win64 compilation be careful to activate the x64 version of the command prompt installed on your system.

.. note:: If you are not running in an environment that has been used to compile GDAL locally, then there are a number of variables that need to be configured. The Conda ``gdal-feedstock`` configuration app can be used as a guideline about how to do that - `build.bat <https://github.com/conda-forge/gdal-feedstock/blob/master/recipe/set_bld_opts.bat>`__.

Creating the SWIG interface code
++++++++++++++++++++++++++++++++

The first step is to generate the SWIG interface code. This will create a set of ``.cs`` definitions that will be compiled into the ``.dll`` files

To create the interface execute the following command (from the ``swig\csharp`` directory):

.. code-block::

    nmake /f makefile.vc interface`

.. note:: You should edit nmake.opt adding the actual location of the :file:`swig.exe` file.

Compiling the code
++++++++++++++++++

After creating the interface the code can be compiled using this command (from the ``swig\csharp`` directory):

.. code-block::

    nmake /f makefile.vc

Upon a successful compilation the following files are created:

* :file:`gdal_csharp.dll`
* :file:`ogr_csharp.dll`
* :file:`osr_csharp.dll`
* :file:`gdalconst_csharp.dll`
* :file:`gdal_wrap.dll`
* :file:`ogr_wrap.dll`
* :file:`osr_wrap.dll`
* :file:`gdalconst_wrap.dll`
* various sample applications

The :file:`\*_csharp.dll` binaries are the managed part of the interface. You should add a reference to these assemblies for using the classes of the interface. These :file:`\*_csharp.dll` files will load the corresponding :file:`\*_wrap.dll` files, which are the unmanaged part of the interface hosting the code of the gdal core.

Testing the successful compilation
++++++++++++++++++++++++++++++++++

To test the compiled binaries, you can use:

.. code-block::

    nmake /f makefile.vc test`

This command will invoke some of the sample applications. 

.. note:: For the tests to work the location of the proj and gdal DLLs should be available in the PATH.

Using MONO on Windows
+++++++++++++++++++++

If you have the Windows version of the MONO package installed you can compile the C# code using the MONO compiler. In this case uncomment the following entry in csharp.opt:

:program:`MONO = YES` 

.. note:: mcs.exe must be in the PATH.


Building on Linux/OSX
---------------------

Requirements
++++++++++++

The build environment has the following dependencies:

* make
* SWIG 3/4
* mono (probably any reasonable version)

Build Environment
+++++++++++++++++

The build environment needs to be correctly configured. If you are not running in an environment that has been used to locally build GDAL then you should run the :program:`configure` command from the GDAL root directory.

The conda gdal-feedstock recipe provides an example of how to do that - `build.sh <https://github.com/conda-forge/gdal-feedstock/blob/master/recipe/build.sh>`__

Creating the SWIG interface code
++++++++++++++++++++++++++++++++

The first step is to generate the SWIG interface code. This will create a set of :file:`.cs` definitions that will be compiled into the :file:`.dll` files

To create the interface execute the following command (from the :file:`swig/    csharp` directory):

.. code-block::

    make generate

.. warning:: In versions of GDAL < 3.3.0 - this command will create incorrect interfaces without the correct namespace. See `#3670 <https://github.com/OSGeo/gdal/pull/3670/commits/777c9d0e86602740199cf9a4ab44e040c52c2283>`__.

Compiling the code
++++++++++++++++++

After creating the interface the code can be compiled using this command (from the :file:`swig/csharp` directory):

.. code-block::

    make

Upon a successful compilation the following files are created:

* :file:`gdal_csharp.dll` and :file:`gdal_csharp.dll.config`
* :file:`ogr_csharp.dll` and :file:`ogr_csharp.dll.config`
* :file:`osr_csharp.dll` and :file:`osr_csharp.dll.config`
* :file:`gdalconst_csharp.dll` and :file:`gdalconst_csharp.dll.config`
* :file:`libgdalcsharp.so / .dylib` etc
* :file:`libogrcsharp.so / .dylib` etc
* :file:`libosrcsharp.so / .dylib` etc
* :file:`libgdalconst_wrap.so / .dylib` etc
* various sample applications (:file:`\*.exe`)

The :file:`\*_csharp.dll` binaries are the managed part of the interface. You should add a reference to these assemblies for using the classes of the interface.

The :file:`\*_csharp.dll` files will try to load the corresponding :file:`\*_wrap.dll` and are redirected to the :file:`libxxxcsharp.\*` libraries, which are the unmanaged part of the interface hosting the code of the gdal core,
by the :file:`\*.dll.config` definitions.

Testing the successful compilation
++++++++++++++++++++++++++++++++++

To test the compiled binaries, you can use:

.. code-block::

    nmake test

This command will invoke some of the sample applications. 

.. note:: For the tests to work the location of the proj and gdal libraries should be available in the PATH.

Using The Bindings on Unix
++++++++++++++++++++++++++

Note that the bindings created by this process will only work with Mono at the moment.

To run one of the prebuilt executables - you can run them with Mono as follows :

:program:`mono GDALInfo.exe`

Both the managed libraries (i.e. the DLLs) and the unmanaged libraries must be available to Mono.
This is in more detail in `the Mono documentation <https://www.mono-project.com/docs/advanced/pinvoke/>`__ 

.. note:: This document was amended from the previous version at `https://trac.osgeo.org/gdal/wiki/GdalOgrCsharpCompile <https://trac.osgeo.org/gdal/wiki/GdalOgrCsharpCompile>`__



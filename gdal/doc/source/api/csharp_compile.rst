.. _csharp_compile:

================================================================================
Compiling the C# bindings
================================================================================

This page describes the primary steps when creating the GDAL/OGR C# binaries from the source.

In most cases this is not necessary and it would preferable to use one of the pre-compiled sources, such as `GisInternals <https://gisinternals.com/>`__ or Conda.

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

..note:: The :program:`VsDevCmd.bat` command can usually be found in :file:`C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Enterprise\\Common7\\Tools` or the equivalent for the Community Edition.

If you don't want to bother with executing the proper vcvars*.bat you might use the development environment specific command prompt to accomplish this task. When using a Win64 compilation be careful to activate the x64 version of the command prompt installed on your system.

Creating the SWIG interface code
++++++++++++++++++++++++++++++++

The first step is to generate the SWIG interface code. This will create a set of ``.cs`` definitions that will be compiled into the ``.dll`` files

To create the interface execute the following command (from the ``swig\csharp`` directory):

:program:`nmake /f makefile.vc interface`

.. note:: You should edit nmake.opt adding the actual location of the swig.exe file.

Compiling the code
++++++++++++++++++

After creating the interface the code can be compiled using this command (from the ``swig\csharp`` directory):

:program:`nmake /f makefile.vc`

Upon a successful compilation the following files are created:

* gdal_csharp.dll
* ogr_csharp.dll
* osr_csharp.dll
* gdalconst_csharp.dll
* gdal_wrap.dll
* ogr_wrap.dll
* osr_wrap.dll
* gdalconst_wrap.dll
* various sample applications (*.exe)

The *_csharp.dll binaries are the unmanaged part of the interface. You should add a reference to these assemblies for using the classes of the interface. These *_csharp.dll-s will load the corresponding *_wrap.dll which are the unmanaged part of the interface hosting the code of the gdal core.

Testing the successful compilation
++++++++++++++++++++++++++++++++++

To test the compiled binaries, you can use:

:program:`nmake /f makefile.vc test`

This command will invoke some of the sample applications. 

.. note For the tests to work the location of the proj.dll should be available in the PATH.

Using MONO on Windows
+++++++++++++++++++++

If you have the Windows version of the MONO package installed you can compile the C# code using the MONO compiler. In this case uncomment the following entry in csharp.opt:

.. describe:: MONO = YES 

.. note mcs.exe must be in the PATH.


Building on Linux/OSX
---------------------


.. note This document was ammended from the previous version at 


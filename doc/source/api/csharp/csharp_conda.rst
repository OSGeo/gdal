.. _csharp_conda:

================================================================================
C# Bindings Conda Package
================================================================================

The GDAL C# Bindings Conda Package is a community supported project.

Installation
------------

GDAL with the C# bindings and example applications can be loaded using

.. code-block::

    conda install -c conda-forge gdal-csharp

Usage 
-----

The Conda package for GDAL version 3.5.0 and later is built using the Cmake build scripts.

.NET Target Framework
+++++++++++++++++++++

The C# bindings are built targeting `netstandard2.0`, which means that they should be usable in any .NET implementation that supports that version of the standard, including .NET Framework, .NET Core, Mono and Unity.

Package Artifacts
+++++++++++++++++

The Conda package contains two sets of artifacts:

* The SWIG wrapper Dynamic Shared Objects (DSO) - :file:`gdal_wrap.dll` or :file:`libgdal_wrap.so` or :file:`libgdal_wrap.dylib` etc. These are loaded as one would expect for a Conda package (i.e. in the :file:`bin` folder in Windows and the :file:`lib` folder in Unix) and thus will automatically link to the correct version of the GDAL DSO and the dependencies, and

* Local NuGet packages for the actual C# bindings (i.e. :file:`gdal_csharp.dll`, :file:`gdalconst_csharp.dll`, :file:`osr_csharp.dll` and :file:`ogr_csharp.dll`). These are created as packages called :file:`OSGeo.GDAL`, :file:`OSGeo.OSR` and :file:`OSGeo.OGR`. These are loaded into the :file:`share/gdal` folder.

Usage
+++++

To use the bindings in your application, you will need to do the following:

#. Add the relevant Packages to you application as local packages, and
#. Add the DSOs to the search path for the application.

The former is not complicated and can be done by defining a local source, either global (as `is explained here <https://docs.microsoft.com/en-us/nuget/hosting-packages/local-feeds>`__ ) or in the build command as is shown below.

The latter can be based on Conda for a console application, as is shown below, but if you are working in an IDE (which being a .NET IDE knows nothing about Conda) and/or working on a GUI application (which is not going to be running in a Conda environment) you are going to have to sort out the DSOs your self; probably involving copying the relevant DSOs into the application package.

Usage Example - Windows
+++++++++++++++++++++++

The most simple example would be:

1. Create a new application (in a dedicated empty folder)

:program:`dotnet new console`

2. Create a small application (by replacing the contents of :file:`Program.cs`).

.. code-block:: c#

    using System;
    using OSGeo.GDAL;

    namespace testapp
    {
        class GdalTest
        {
            static void Main(string[] args)
            {
                Console.WriteLine("Testing GDAL C# Bindings");
                Gdal.UseExceptions();
                Console.WriteLine($"Gdal version {Gdal.VersionInfo(null)}");
            }
        }
    }

3. Add the GDAL package

:program:`dotnet add package OSGeo.GDAL -s %CONDA_PREFIX%\\Library\\share\\gdal`

4. Compile or run

:program:`dotnet run`

Provided you run these commands in a Conda environment (containing the gdal-csharp package) this should just work.

Usage Example - Unix
++++++++++++++++++++

1. Create a new application (in a dedicated empty folder)

:program:`dotnet new console`

2. Create a small application (by replacing the contents of :file:`Program.cs`).

.. code-block:: c#

    using System;
    using OSGeo.GDAL;

    namespace testapp
    {
        class GdalTest
        {
            static void Main(string[] args)
            {
                Console.WriteLine("Testing GDAL C# Bindings");
                Gdal.UseExceptions();
                Console.WriteLine($"Gdal version {Gdal.VersionInfo(null)}");
            }
        }
    }

3. Add the GDAL package

:program:`dotnet add package OSGeo.GDAL -s $CONDA_PREFIX/share/gdal`

4. Compile or run

:program:`dotnet run`

.. warning:: This will not just work under Unix since, unlike Windows, the Library Search Path is separate from the Process Search path and is not set by Conda.

    To make this work, you will probably have to change the Library search path, which is ok for development but should not be accepted for production (which means that you will need to copy the DSOs to the application search path).

    Under Linux:

    :program:`export LD_LIBRARY_PATH=$CONDA_PREFIX/lib`

    Under OSX:

    :program:`export DYLD_LIBRARY_PATH=$CONDA_PREFIX/lib`

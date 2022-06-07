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

The Conda package for GDAL version 3.5.0 and later uses the CMAKE build scripts and therefore there have been changes in the build process and the artefacts produced.

    :ref:`csharp_conda_35`

    :ref:`csharp_conda_34`



.. _csharp_conda_35:

GDAL 3.5.0 and later
--------------------

The Conda package for GDAL version 3.5.0 and later is built using the new Cmake build scripts.

.NET Target Framework
+++++++++++++++++++++

On all architectures (i.e. Windows, Linux and Mac), the bindings are compiled using .NET6.0 as the current (at the time of writing) LTS version.

Package Artefacts
+++++++++++++++++

The Conda package contains two sets of artefacts:

* The SWIG wrapper Dynamic Shared Objects (DSO) - :file:`gdal_wrap.dll` or :file:`libgdal_wrap.so` or :file:`libgdal_wrap.dylib` etc. These are loaded as one would expect for a Conda package (i.e. in the :file:`bin` folder in Windows and the :file:`lib` folder in Unix) and thus will automatically link to the correct version of the GDAL DSO and the dependencies, and

* Local NuGet packages for the actual C# bindings (i.e. :file:`gdal_csharp.dll`, :file:`gdalconst_csharp.dll`, :file:`osr_csharp.dll` and :file:`ogr_csharp.dll`). These are created as packages called :file:`OSGeo.GDAL`, :file:`OSGeo.OSR` and :file:`OSGeo.OGR`. These are loaded into the :file:`share/gdal` folder.

Usage
+++++

To use the bindings in your application, you will need to basically do the following:

#. Add the relevent Packages to you application as local packages, and
#. Add the DSOs to the search path for the application.

The former is not complicated and can be done by defining a local source, either global (as `is explained here <https://docs.microsoft.com/en-us/nuget/hosting-packages/local-feeds>`__ ) or in the build command as is shown below.

The latter can be based on Conda for a console application, as is shown below, but if you are working in an IDE (which being a .NET IDE knows nothing about Conda) and/or working on a GUI application (which is not going to be running in a Conda environment) you are going to have to sort out the DSOs your self; probably involving copying the relevent DSOs into the application package.

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



.. _csharp_conda_34:

GDAL 3.4.x and earlier
----------------------

Windows
+++++++

.. note:: You can test if the C# bindings are working in a Conda environment by running :program:`%CONDA_PREFIX%\\Library\\bin\\gcs\\gdal_test`.

The DLLs are loaded into the :file:`%CONDA_PREFIX%\\Library\\bin` folder, as is normal for a Conda environment.

The  C# sample .EXEs are loaded into  :file:`%CONDA_PREFIX%\\Library\\bin\\gcs`, because otherwise they over write the standard GDAL tools.

To run a sample application - eg GDALinfo.exe - add :file:`%CONDA_PREFIX%\\Library\\bin\\gcs` to the path and just run :program:`gdalinfo`.

To link the DLLs into your code, you will need to include the DLLs into the project (which will almost certainly mean copying them to the project directory).

For a console app that is run from within the Conda environment (i.e. run :program:`conda activate`) then they should work once compiled.

For GUI apps or other apps that cannot be run from with the Conda environment then you will have to setup the environment to make the GDAL DLLs available to the app.


Mac / Linux
+++++++++++

.. note:: You test if the C# bindings are working in a Conda environment by running :program:`mono $CONDA_PREFIX/lib/gdal_test.exe`

The shared objects (i.e. :file:`\*.so` / :file:`\*.dylib`), the .EXE and .DLL files are all loaded into the :file:`$CONDA_PREFIX/lib`
folder (not the :file:`bin` folder as you might expect). This is in line with `the Mono documentation <https://www.mono-project.com/docs/getting-started/application-deployment/>`__.

To run one of the sample applications (e.g. :file:`GDALinfo.exe`), run :program:`mono $CONDA_PREFIX/lib/GDALinfo.exe`.

To build a console app in Mono, you can do this in a conda environment simple using a command similar to this (changing the source name to your own):

.. code-block:: C#

    msc /r:gdal_csharp.dll /r:ogr_csharp.dll /r:osr_csharp.dll /r:System.Drawing.dll /out:gdal_test.exe gdal_test.cs

If the compiled executable is run in the conda environment, this should work. For something more portable or a GUI app, then you have to work out the dependencies your self.

The DLLs can also be used in a .NET project, for instance built in VS. Just link the DLLs in as dependencies.

Differences in the Conda build
++++++++++++++++++++++++++++++

The Conda build is in some ways different from the "standard" GDAL 3.4.x build:

* On Mac and Linux, the SWIG files are built as :file:`\*_wrap` in line with the windows versions. This means that there are no :file:`.config` files. Most importantly, this means that the DLLs can be used in .NET and Unity projects as well as Mono.
* On Windows, the sample apps are built in .NET5 and not .NET CORE 2.1.

These changes anticipated the standard build for GDAL 3.5.x.
.. _csharp_conda:

================================================================================
C# Bindings Conda Package
================================================================================

Installation
------------

GDAL with the C# bindings and example applications can be loaded using

.. code-block::

    conda install -c conda-forge gdal-csharp

.. note:: On Mac and Linux, this command will also load Mono

Usage - Windows
---------------

.. note:: You can test if the C# bindings are working in a Conda environment by running :program:`%CONDA_PREFIX%\\Library\\bin\\gcs\\gdal_test`.

The DLLs are loaded into the :file:`%CONDA_PREFIX%\\Library\\bin` folder, as is normal for a Conda environment.

The  C# sample .EXEs are loaded into  :file:`%CONDA_PREFIX%\\Library\\bin\\gcs`, because otherwise they over write the standard GDAL tools.

To run a sample application - eg GDALinfo.exe - add :file:`%CONDA_PREFIX%\\Library\\bin\\gcs` to the path and just run :program:`gdalinfo`.

To link the DLLs into your code, you will need to include the DLLs into the project (which will almost certainly mean copying them to the project directory).

For a console app that is run from within the Conda environment (i.e. run :program:`conda activate`) then they should work once compiled.

For GUI apps or other apps that cannot be run from with the Conda environment then you will have to setup the environment to make the GDAL DLLs available to the app.


Usage - Mac / Linux
-------------------

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
------------------------------

The Conda build is in some ways different from the "standard" GDAL build:

* On Mac and Linux, the SWIG files are built as :file:`\*_wrap` in line with the windows versions. This means that there are no :file:`.config` files. Most importantly, this means that the DLLs can be used in .NET and Unity projects as well as Mono.
* On Windows, the sample apps are built in .NET5 and not .NET CORE 2.1.


.. _download:

================================================================================
Download
================================================================================

.. only:: html

    .. contents::
       :depth: 3
       :backlinks: none

The GDAL project distributes GDAL as source code and :ref:`Containers` only. :ref:`Binaries` produced by others are available for a variety of platforms and package managers.

Source Code
-----------

Current Release
...............

* **2024-08-16** `gdal-3.9.2.tar.gz`_ `3.9.2 Release Notes`_ (`3.9.2 md5`_)

.. _`3.9.2 Release Notes`: https://github.com/OSGeo/gdal/blob/v3.9.2/NEWS.md
.. _`gdal-3.9.2.tar.gz`: https://github.com/OSGeo/gdal/releases/download/v3.9.2/gdal-3.9.2.tar.gz
.. _`3.9.2 md5`: https://github.com/OSGeo/gdal/releases/download/v3.9.2/gdal-3.9.2.tar.gz.md5

Past Releases
.............

Links to :ref:`download_past` are also available.

.. _source:

Development Source
..................

The main repository for GDAL is located on GitHub at
https://github.com/OSGeo/GDAL.

You can obtain a copy of the active source code by issuing the following
command

::

    git clone https://github.com/OSGeo/GDAL.git


Additional information is available about :ref:`build_requirements` and :ref:`building_from_source`.


.. _binaries:

Binaries
------------------------------------------------------------------------------

In this section we list a number of the binary distributions of GDAL
all of which should have fully reproducible open source build recipes.

Note that the maintainers of those distributions are generally not the maintainers
of the GDAL sources, so please report any issue specific to those builds through
their own support channels.

Windows
................................................................................

Windows builds are available via `Conda Forge`_ (64-bit only). See the
:ref:`conda` section for more detailed information. GDAL is also distributed
by `GISInternals`_ and `OSGeo4W`_ and through the `NuGet`_ and :ref:`vcpkg` package managers.

.. _`Conda Forge`: https://anaconda.org/conda-forge/gdal
.. _`GISInternals`: https://www.gisinternals.com/index.html
.. _`OSGeo4W`: https://trac.osgeo.org/osgeo4w/
.. _`NuGet`: https://www.nuget.org/packages?q=GDAL

Linux
................................................................................

Packages are available for `Debian`_, `Alpine_`, `Fedora_`, and other distributions.

.. _`Debian`: https://tracker.debian.org/pkg/gdal
.. _`Alpine`: https://pkgs.alpinelinux.org/package/edge/community/x86/gdal
.. _`Fedora`: https://packages.fedoraproject.org/pkgs/gdal/


Mac OS
......

GDAL packages are available on `Homebrew`_.

.. _`Homebrew`: https://formulae.brew.sh/formula/gdal


Cross-Platform Package Managers
...............................

.. _conda:

Conda
^^^^^

`Conda <https://anaconda.org>`__ can be used on multiple platforms (Windows, macOS, and Linux) to
install software packages and manage environments. Conda packages for GDAL are
available through `conda-forge <https://anaconda.org/conda-forge/gdal>`__.

.. only:: html

    Latest version: |Conda badge|

    .. |Conda badge| image:: https://anaconda.org/conda-forge/gdal/badges/version.svg
        :target: https://anaconda.org/conda-forge/gdal

::

    conda install [-c channel] [package...]

GDAL is available as several subpackages:

- ``gdal``: Python bindings and Python utilities (depends on libgdal-core)
- ``libgdal``: meta-package gathering all below libgdal-* packages (except libgdal-arrow-parquet)
- ``libgdal-arrow-parquet``: :ref:`vector.arrow` and :ref:`vector.parquet` drivers as a plugin (depends on libgdal-core)
- ``libgdal-core``: core library and C++ utilities, with a number of builtin drivers (available since GDAL 3.9.1)
- ``libgdal-fits``: :ref:`raster.fits` driver as a plugin (depends on libgdal-core, available since GDAL 3.9.1)
- ``libgdal-grib``: :ref:`raster.grib` driver as a plugin (depends on libgdal-core, available since GDAL 3.9.1)
- ``libgdal-hdf4``: :ref:`raster.hdf4` driver as a plugin (depends on libgdal-core, available since GDAL 3.9.1)
- ``libgdal-hdf5``: :ref:`raster.hdf5` driver as a plugin (depends on libgdal-core, available since GDAL 3.9.1)
- ``libgdal-jp2openjpeg``: :ref:`raster.jp2openjpeg` driver as a plugin (depends on libgdal-core, available since GDAL 3.9.1)
- ``libgdal-kea``: :ref:`raster.kea` driver as a plugin (depends on libgdal-core, available since GDAL 3.9.1)
- ``libgdal-netcdf``: :ref:`raster.netcdf` driver as a plugin (depends on libgdal-core, available since GDAL 3.9.1)
- ``libgdal-pdf``: :ref:`raster.pdf` driver as a plugin (depends on libgdal-core, available since GDAL 3.9.1)
- ``libgdal-postgisraster``: :ref:`raster.postgisraster` driver as a plugin (depends on libgdal-core, available since GDAL 3.9.1)
- ``libgdal-pg``: :ref:`vector.pg` driver as a plugin (depends on libgdal-core, available since GDAL 3.9.1)
- ``libgdal-tiledb``: :ref:`raster.tiledb` driver as a plugin (depends on libgdal-core, available since GDAL 3.9.1)
- ``libgdal-xls``: :ref:`vector.xls` driver as a plugin (depends on libgdal-core, available since GDAL 3.9.1)


To install the ``gdal`` package (Python bindings and utilities), and ``libgdal-core``:

::

    conda install -c conda-forge gdal


To install the ``libgdal`` meta-package with all available drivers, but libgdal-arrow-parquet:

::

    conda install -c conda-forge libgdal


To install the Arrow and Parquet drivers as plugins:

::

    conda install -c conda-forge libgdal-arrow-parquet


GDAL master Conda builds
~~~~~~~~~~~~~~~~~~~~~~~~

GDAL master builds are available in the `gdal-master <https://anaconda.org/gdal-master/gdal>`__
channel. They are based on dependencies from the ``conda-forge`` channel.

First, install mamba into the ``base`` environment, create a dedicated ``gdal_master_env``
environment, and then activate the dedicated ``gdal_master_env`` environment.

::

    conda update -n base -c conda-forge conda
    conda install -n base --override-channels -c conda-forge mamba 'python_abi=*=*cp*'
    conda create --name gdal_master_env
    conda activate gdal_master_env

Then install GDAL from the ``gdal-master`` channel:

::

    mamba install -c gdal-master gdal
    mamba install -c gdal-master libgdal-arrow-parquet # if you need the Arrow and Parquet drivers


.. _vcpkg:

vcpkg
^^^^^

The GDAL port in the `vcpkg <https://github.com/Microsoft/vcpkg>`__ dependency manager is kept up to date by Microsoft team members and community contributors.
You can download and install GDAL using the vcpkg as follows:

::

    git clone https://github.com/Microsoft/vcpkg.git
    cd vcpkg
    ./bootstrap-vcpkg.sh  # ./bootstrap-vcpkg.bat for Windows
    ./vcpkg integrate install
    ./vcpkg install gdal

If the version is out of date, please `create an issue or pull request <https://github.com/Microsoft/vcpkg>`__ on the vcpkg repository.

Spack
^^^^^

Spack is a package management tool designed to support multiple versions and
configurations of software on a wide variety of platforms and environments.
It was designed for large supercomputing centers. Spack builds packages from
sources, and allows tweaking their configurations.

You can find information about GDAL in Spack at
https://packages.spack.io/package.html?name=gdal

For the default GDAL build with a reduced number of drivers:

::

    git clone -c feature.manyFiles=true https://github.com/spack/spack.git
    cd spack/bin
    ./spack install gdal

For a build with netcdf driver enabled:

::

    ./spack install gdal +netcdf


.. _containers:

Containers
----------

Docker images with nightly builds of GDAL master and tagged releases are available at
`GitHub Container registry <https://github.com/OSGeo/gdal/pkgs/container/gdal>`_.

Information on the content of the different configurations can be found at
`https://github.com/OSGeo/gdal/tree/master/docker <https://github.com/OSGeo/gdal/tree/master/docker>`_.


Documentation
-------------

Besides being included when downloading the software, the documentation is
also available independently as a `PDF file <https://gdal.org/gdal.pdf>`_,
and `a ZIP of individual HTML pages <https://github.com/OSGeo/gdal-docs/archive/refs/heads/master.zip>`_ for offline browsing. (The ZIP also includes the PDF.) The documentation reflects the latest state of the development branch of the software.

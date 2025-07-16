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

* **2025-07-12** `gdal-3.11.3.tar.gz`_ `3.11.3 Release Notes`_ (`3.11.3 md5`_)

.. _`3.11.3 Release Notes`: https://github.com/OSGeo/gdal/blob/v3.11.3/NEWS.md
.. _`gdal-3.11.3.tar.gz`: https://github.com/OSGeo/gdal/releases/download/v3.11.3/gdal-3.11.3.tar.gz
.. _`3.11.3 md5`: https://github.com/OSGeo/gdal/releases/download/v3.11.3/gdal-3.11.3.tar.gz.md5

Past Releases
.............

.. only:: html

    Links to :ref:`download_past` are also available.

.. only:: not html

    Links to `past releases <https://gdal.org/en/latest/download_past.html>`__ are also available.

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

Maintenance policy
..................

The GDAL upstream team only maintains the branch on which the latest release has
been done, with bugfixes releases issued roughly every 2 months.
So, for example, during the development phase of GDAL 3.10.0, GDAL 3.9.x bugfixes
releases are done based on the release/3.9 branch, but not older branches (GDAL 3.8.x or older).

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
:ref:`conda` or :ref:`pixi` section for more detailed information. GDAL is also distributed
by `GISInternals`_ and `OSGeo4W`_ and through the `NuGet`_ and :ref:`vcpkg` package managers.

.. _`Conda Forge`: https://anaconda.org/conda-forge/gdal
.. _`GISInternals`: https://www.gisinternals.com/index.html
.. _`OSGeo4W`: https://trac.osgeo.org/osgeo4w/
.. _`NuGet`: https://www.nuget.org/packages?q=GDAL

Linux
................................................................................

Packages are available for `Debian`_, `Alpine`_, `Fedora`_, and other distributions.

.. _`Debian`: https://tracker.debian.org/pkg/gdal
.. _`Alpine`: https://pkgs.alpinelinux.org/package/edge/community/x86/gdal
.. _`Fedora`: https://packages.fedoraproject.org/pkgs/gdal/


Mac OS
......

GDAL packages are available on `Homebrew`_.

.. _`Homebrew`: https://formulae.brew.sh/formula/gdal


Android
.......

GDAL can be installed using :ref:`vcpkg`. You may also refer to `vcpkg Android support <https://learn.microsoft.com/en-us/vcpkg/users/platforms/android>`__ for general instructions.

For example to install default configuration for the ``arm64-android`` target:

.. code-block:: shell

    git clone https://github.com/Microsoft/vcpkg.git
    cd vcpkg
    ./bootstrap-vcpkg.sh  # ./bootstrap-vcpkg.bat for Windows
    ./vcpkg integrate install
    export ANDROID_NDK_HOME=/path/to/android_ndk_home  # to adapt
    ./vcpkg search gdal --featurepackages  # list optional features
    ./vcpkg install gdal:arm64-android  # install with default configuration
    ./vcpkg install gdal[poppler,netcdf]:arm64-android  # install with Poppler and netdf support


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
- ``libgdal-avif``: :ref:`raster.avif` driver as a plugin (depends on libgdal-core, available since GDAL 3.10.0)
- ``libgdal-core``: core library and C++ utilities, with a number of builtin drivers (available since GDAL 3.9.1)
- ``libgdal-fits``: :ref:`raster.fits` driver as a plugin (depends on libgdal-core, available since GDAL 3.9.1)
- ``libgdal-grib``: :ref:`raster.grib` driver as a plugin (depends on libgdal-core, available since GDAL 3.9.1)
- ``libgdal-hdf4``: :ref:`raster.hdf4` driver as a plugin (depends on libgdal-core, available since GDAL 3.9.1)
- ``libgdal-hdf5``: :ref:`raster.hdf5` driver as a plugin (depends on libgdal-core, available since GDAL 3.9.1)
- ``libgdal-heif``: :ref:`raster.heif` driver as a plugin (depends on libgdal-core, available since GDAL 3.10.0)
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
channel. They are based on dependencies from the ``conda-forge`` channel. The latest master
build can be installed with the following command:

::

    conda install -c gdal-master -c conda-forge gdal-master::gdal

As with released versions of GDAL, additional drivers can be installed using `gdal-master::libgdal-{driver_name}`.


.. _pixi:

pixi
^^^^

`Pixi <https://pixi.sh/latest/>`__  is a package management tool for developers. It allows the developer
to install libraries and applications in a reproducible way. Packages for GDAL are
available through the `conda-forge <https://anaconda.org/conda-forge/gdal>`__ channel.

If you want to be able to use GDAL as part of a project:

::

    pixi init name-of-project
    cd name-of-project
    pixi add gdal libgdal-core
    pixi add libgdal-arrow-parquet # if you need the Arrow and Parquet drivers
    pixi shell

Pixi supports using tools like GDAL and OGR globally, similar to conda's base environment, without having to use an activate command:

::

    pixi global install gdal libgdal-core
    pixi global install libgdal-arrow-parquet # if you need the Arrow and Parquet drivers

.. _vcpkg:

vcpkg
^^^^^

The GDAL port in the `vcpkg <https://github.com/Microsoft/vcpkg>`__ dependency manager is kept up to date by Microsoft team members and community contributors.
You can download and install GDAL using the vcpkg as follows:

.. code-block:: shell

    git clone https://github.com/Microsoft/vcpkg.git
    cd vcpkg
    ./bootstrap-vcpkg.sh  # ./bootstrap-vcpkg.bat for Windows
    ./vcpkg integrate install
    ./vcpkg search gdal --featurepackages  # list optional features
    ./vcpkg install gdal  # install with default configuration
    ./vcpkg install gdal[poppler,netcdf]  # install with Poppler and netdf support

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

|offline-download|

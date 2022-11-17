.. include:: ../substitutions.rst

.. _dev_environment:

================================================================================
Setting up a development environment
================================================================================

.. _build_requirements:

Build requirements
--------------------------------------------------------------------------------

The minimum requirements are:

- CMake >= 3.10, and an associated build system (make, ninja, Visual Studio, etc.)
- C99 compiler
- C++11 compiler
- PROJ >= 6.0
- SWIG >= 4.0.2, for building bindings to other programming languages, such as Python
- Python, for running the test suite

A number of optional libraries are also strongly recommended for most builds:
SQLite3, expat, libcurl, zlib, libtiff, libgeotiff, libpng, libjpeg, etc.
Consult :ref:`raster_drivers` and :ref:`vector_drivers` pages for information
on dependencies of optional drivers.

.. note::

    If SWIG 4.0.2 is not provided by system package manager, it can be built and installed from source using the following commands:

    .. code-block:: bash

        export SWIG_PREFIX=/path/to/install
        export SWIG_VERSION=4.0.2

        mkdir /tmp/swig/
        cd /tmp/swig/
        wget https://sourceforge.net/projects/swig/files/swig/swig-${SWIG_VERSION}/swig-${SWIG_VERSION}.tar.gz/download -O swig-${SWIG_VERSION}.tar.gz
        tar xf swig-${SWIG_VERSION}.tar.gz
        cd swig-${SWIG_VERSION}
        ./configure --prefix=$SWIG_PREFIX
        make
        make install
        export PATH=$SWIG_PREFIX/bin:$PATH

    The path to the updated version of SWIG can be provided to provided to ``cmake`` using ``-DSWIG_EXECUTABLE=$SWIG_PREFIX/bin/swig``.


Vagrant
-------

`Vagrant <https://www.vagrantup.com>`_ is a tool that works with a virtualization product such as 
VirtualBox to create a reproducible development environment. GDAL includes a Vagrant configuration
file that sets up an Ubuntu virtual machine with a comprehensive set of dependencies.

Once Vagrant has been installed and the GDAL source downloaded, the virtual machine can be set up
by running the following from the source root directory:

.. code-block:: bash

    # VAGRANT_VM_CPU=number_of_cpus
    vagrant up

The source root directory is exposed inside the virtual machine at ``/vagrant``, so changes made to
GDAL source files on the host are seen inside the VM. To rebuild GDAL after changing source files,
you can connect to the VM and re-run the build command:

.. code-block:: bash

    vagrant ssh
    cmake --build .

Note that the following directories on the host will be created (and can be
removed if the Vagrant environment is no longer needed):

- ``../apt-cache/ubuntu/jammy64``: contains a cache of Ubuntu packages of the VM,
  to allow faster VM reconstruction
- ``build_vagrant``: CMake build directory
- ``ccache_vagrant``: CCache directory

Building on Windows with Conda dependencies and Visual Studio
--------------------------------------------------------------------------------

It is less appropriate for Debug builds of GDAL, than other methods, such as using vcpkg.

Install git
+++++++++++

Install `git <https://git-scm.com/download/win>`_

Install miniconda
+++++++++++++++++

Install `miniconda <https://repo.anaconda.com/miniconda/Miniconda3-latest-Windows-x86_64.exe>`_

Install GDAL dependencies
+++++++++++++++++++++++++

Start a Conda enabled console and assuming there is a c:\\dev directory

.. code-block:: console

    cd c:\dev
    conda create --name gdal
    conda activate gdal
    conda install --yes --quiet curl libiconv icu git python=3.7 swig numpy pytest zlib clcache
    conda install --yes --quiet -c conda-forge compilers
    conda install --yes --quiet -c conda-forge \
        cmake proj geos hdf4 hdf5 \
        libnetcdf openjpeg poppler libtiff libpng xerces-c expat libxml2 kealib json-c \
        cfitsio freexl geotiff jpeg libpq libspatialite libwebp-base pcre postgresql \
        sqlite tiledb zstd charls cryptopp cgal librttopo libkml openssl xz

.. note::

    The ``compilers`` package will install ``vs2017_win-64`` (at time of writing)
    to set the appropriate environment for cmake to pick up. It is also possible
    to use the ``vs2019_win-64`` package if Visual Studio 2019 is to be used.

Checkout GDAL sources
+++++++++++++++++++++

.. code-block:: console

    cd c:\dev
    git clone https://github.com/OSGeo/gdal.git

Build GDAL
++++++++++

From a Conda enabled console

.. code-block:: console

    conda activate gdal
    cd c:\dev\gdal
    cmake -S . -B build -DCMAKE_PREFIX_PATH:FILEPATH="%CONDA_PREFIX%" \
                        -DCMAKE_C_COMPILER_LAUNCHER=clcache
                        -DCMAKE_CXX_COMPILER_LAUNCHER=clcache
    cmake --build build --config Release -j 8

.. only:: FIXME

    Run GDAL tests
    ++++++++++++++

    ::

        cd c:\dev\GDAL
        cd _build.vs2019
        ctest -V --build-config Release

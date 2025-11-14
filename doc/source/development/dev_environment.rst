.. _dev_environment:

================================================================================
Setting up a development environment
================================================================================

Build requirements
------------------

See :ref:`build_requirements`

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

Docker
------

The Linux environments used for building and testing GDAL on GitHub Actions are
defined by Docker images that can be pulled to any machine for development. The
Docker image used for each build is specified in :source_file:`.github/workflows/linux_build.yml`. As an
example, the following commands can be run from the GDAL source root to build
and test GDAL using the clang address sanitizer (ASAN) in the same environment
that is used in GitHub Actions:

.. code-block:: bash

    docker run -it \
        -v $(pwd):/gdal:rw \
        ghcr.io/osgeo/gdal-deps:ubuntu20.04-master
    cd /gdal
    mkdir build-asan
    cd build-asan
    ../.github/workflows/asan/build.sh
    ../.github/workflows/asan/test.sh

To avoid built objects being owned by root, it may be desirable to add ``-u $(id
-u):$(id -g) -v /etc/passwd:/etc/passwd`` to the ``docker run`` command above.

Building on Windows with Conda dependencies and Visual Studio
--------------------------------------------------------------------------------

This approach is less appropriate for Debug builds of GDAL, than other methods, such as using vcpkg.

A minimal Windows build configuration utilizing Conda is provided within the GitHub Actions workflow
defined in :source_file:.github/workflows/cmake_builds.yml - see the ``build-windows-conda`` job.

Install git
+++++++++++

Install `git <https://git-scm.com/download/win>`_

Install miniconda
+++++++++++++++++

Install `miniconda <https://repo.anaconda.com/miniconda/Miniconda3-latest-Windows-x86_64.exe>`_

Install Visual Studio
+++++++++++++++++++++

Install `Visual Studio <https://visualstudio.microsoft.com/vs/community/>`_ 
In Visual Studio Installer Workloads tab check Desktop development with C++.
Only the latest Community Edition (2022) is available.

Install GDAL dependencies
+++++++++++++++++++++++++

Start a Conda-enabled PowerShell console and assuming there is a ``c:\\dev`` directory

.. code-block:: ps1

    cd c:\dev
    conda create --yes --name gdal
    conda activate gdal
    conda install --yes --quiet -c conda-forge compilers cmake curl libiconv icu python swig numpy `
        pytest pytest-env pytest-benchmark filelock zlib lxml jsonschema setuptools
    # --only-deps only installs the dependencies of the listed packages, not the packages themselves
    conda install --yes --quiet -c conda-forge --only-deps `
        libgdal libgdal-hdf4 libgdal-hdf5 libgdal-netcdf libgdal-pdf libgdal-jp2openjpeg

.. note::

    The ``compilers`` package will install ``vs2022_win-64`` (at time of writing)
    to set the appropriate environment for CMake to pick up. It also finds and works
    with Visual Studio 2022 (Professional or Enterprise) if that is installed.

Checkout GDAL sources
+++++++++++++++++++++

.. code-block:: ps1

    cd c:\dev
    git clone https://github.com/OSGeo/gdal.git

Build GDAL
++++++++++

From a Conda-enabled PowerShell console:

.. code-block:: ps1

    conda activate gdal
    cd c:\dev\gdal
    cmake -S . -B build -DCMAKE_PREFIX_PATH:FILEPATH="$env:CONDA_PREFIX"
    cmake --build build --config Release -j 8

Run GDAL tests
++++++++++++++

Check everything has built correctly, and then run the test suite using
the commands below from a Conda-enabled PowerShell console:

.. code-block:: ps1

    cd c:\dev\gdal\build
    # set environment variables, see section below
    ..\scripts\setdevenv.ps1
    # check the version
    gdal --version
    # check the Python module and version
    python -c "import osgeo; import inspect; print(inspect.getfile(osgeo))"
    python -c "from osgeo import gdal; print(gdal.__version__)"
    # run the test suite
    ctest -V --build-config Release

.. _setting_dev_environment_variables:

Setting development environment variables
-----------------------------------------

Once GDAL has been built, a number of environment variables must be set to be
able to execute C++ or Python utilities of the build directory, or run tests.

This can be done by sourcing the following from the build directory:

.. code-block:: bash

    . ../scripts/setdevenv.sh

(with adjustments to the above path if the build directory is not a subdirectory of the GDAL source root).

For Windows, a similar ``scripts/setdevenv.bat`` script exists (it currently assumes a Release build).

.. code-block:: console

    cd c:\dev\gdal\build
    ..\scripts\setdevenv.bat

Alternatively, on Windows, you can set up a PowerShell development environment using the ``scripts/setdevenv.ps1`` script:

.. code-block:: ps1

    cd c:\dev\gdal\build
    ..\scripts\setdevenv.ps1

To verify that environment variables have been set correctly, you can check the version of a GDAL binary:

.. code-block:: bash

    gdalinfo --version
    # GDAL 3.13.0dev-8c3cf3de02, released 2025/11/14

and the Python bindings:

.. code-block:: bash

    python3 -c 'from osgeo import gdal; print(gdal.__version__)'
    # 3.11.0dev-c4a2e0b926-dirty

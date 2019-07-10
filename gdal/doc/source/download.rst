.. _download:

================================================================================
Download
================================================================================

.. only:: html

    .. contents::
       :depth: 3
       :backlinks: none

Current Releases
------------------------------------------------------------------------------

* **2019-06-28** `gdal-3.0.1.tar.gz`_ `3.0.1 Release Notes`_ (`3.0.1 md5`_)

.. _`3.0.1 Release Notes`: https://github.com/OSGeo/gdal/blob/v3.0.1/gdal/NEWS
.. _`gdal-3.0.1.tar.gz`: https://github.com/OSGeo/gdal/releases/download/v3.0.1/gdal-3.0.1.tar.gz
.. _`3.0.1 md5`: https://github.com/OSGeo/gdal/releases/download/v3.0.1/gdal-3.0.1.tar.gz.md5


Past Releases
------------------------------------------------------------------------------

* **2019-06-28** `gdal-2.4.2.tar.gz`_ `2.4.2 Release Notes`_ (`2.4.2 md5`_)

.. _`2.4.2 Release Notes`: https://github.com/OSGeo/gdal/blob/v2.4.2/gdal/NEWS
.. _`gdal-2.4.2.tar.gz`: https://download.osgeo.org/gdal/2.4.2/gdal-2.4.2.tar.gz
.. _`2.4.2 md5`: https://download.osgeo.org/gdal/2.4.2/gdal-2.4.2.tar.gz.md5

.. _source:

Development Source
------------------------------------------------------------------------------

The main repository for GDAL is located on github at
https://github.com/OSGeo/GDAL.

You can obtain a copy of the active source code by issuing the following
command

::

    git clone https://github.com/OSGeo/GDAL.git


Binaries
------------------------------------------------------------------------------

In this section we list a number of the binary distributions of GDAL.


Windows
................................................................................

Windows builds are available via `Conda Forge`_ (64-bit only). See the
:ref:`conda` section for more detailed information.

.. _`Conda Forge`: https://anaconda.org/conda-forge/gdal


Debian
................................................................................

Debian packages are now available on `Debian Unstable`_.

.. _`Debian Unstable`: https://tracker.debian.org/pkg/gdal

.. _conda:

Conda
................................................................................

`Conda <https://anaconda.org>`__ can be used on multiple platforms (Windows, macOS, and Linux) to
install software packages and manage environments. Conda packages for GDAL are
available at https://anaconda.org/conda-forge/gdal.

.. only:: html

    Latest version: |Conda badge|

    .. |Conda badge| image:: https://anaconda.org/conda-forge/gdal/badges/version.svg
        :target: https://anaconda.org/conda-forge/gdal

::

    conda install [-c channel] [package...]


::

    conda install -c conda-forge gdal


Linux Docker images
................................................................................

Images with nightly builds of GDAL master and tagged releases are available at
`Docker Hub <https://hub.docker.com/r/osgeo/gdal/tags>`_

Information on the content of the different configurations can be found at
`https://github.com/OSGeo/gdal/tree/master/gdal/docker <https://github.com/OSGeo/gdal/tree/master/gdal/docker>`_

.. _download:

================================================================================
Download
================================================================================

.. only:: html

    .. contents::
       :depth: 3
       :backlinks: none

Current Release(s)
------------------------------------------------------------------------------

* **2019-05-05** `gdal-3.0.0.tar.gz`_ `Release Notes`_ (`md5`_)

.. _`Release Notes`: https://github.com/OSGeo/gdal/blob/v3.0.0/gdal/NEWS
.. _`gdal-3.0.0.tar.gz`: https://github.com/OSGeo/gdal/releases/download/v3.0.0/gdal-3.0.0.tar.gz
.. _`md5`: https://github.com/OSGeo/gdal/releases/download/v3.0.0/gdal-3.0.0.tar.gz.md5


Past Releases
------------------------------------------------------------------------------

TODO

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
:ref:`conda` for more detailed information.





Debian
................................................................................

Debian packages are now available on `Debian Unstable`_.

.. _`Debian Unstable`: https://tracker.debian.org/pkg/gdal


.. _`Conda Forge`: https://anaconda.org/conda-forge/gdal

.. _conda:

Conda
................................................................................

`Conda`_ can be used on multiple platforms (Windows, macOS, and Linux) to
install software packages and manage environments. Conda packages for GDAL are
available at https://anaconda.org/conda-forge/gdal.


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

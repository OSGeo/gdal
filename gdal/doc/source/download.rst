.. _download:

******************************************************************************
Download
******************************************************************************


.. contents::
   :depth: 3
   :backlinks: none


Current Release(s)
------------------------------------------------------------------------------

* **2019-04-09** `GDAL-1.9.0-src.tar.gz`_ `Release Notes`_ (`md5`_)

.. _`Release Notes`: https://github.com/GDAL/GDAL/releases/tag/1.8.0
.. _`md5`: https://github.com/GDAL/GDAL/releases/download/1.9.0/GDAL-1.9.0-src.tar.gz.md5


Past Releases
------------------------------------------------------------------------------

* **2018-10-12** `GDAL-1.8.0-src.tar.gz`_

.. _`GDAL-1.9.0-src.tar.gz`: https://github.com/GDAL/GDAL/releases/download/1.9.0/GDAL-1.9.0-src.tar.gz


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

In this section we list a number of the binary distributions of GDAL. T


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



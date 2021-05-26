.. _gdal-config:

================================================================================
gdal-config (Unix)
================================================================================

.. only:: html

Determines various information about a GDAL installation.

.. Index:: gdal-config

Synopsis
--------

.. code-block::

    gdal-config [OPTIONS]
    Options:
            [--prefix[=DIR]]
            [--libs]
            [--cflags]
            [--version]
            [--ogr-enabled]
            [--formats]

Description
-----------

This utility script (available on Unix systems) can be used to determine
various information about a GDAL installation. It is normally just used
by configure scripts for applications using GDAL but can be queried by
an end user.

.. option:: --prefix

    the top level directory for the GDAL installation.

.. option:: --libs

    The libraries and link directives required to use GDAL.

.. option:: --cflags

    The include and macro definition required to compiled modules using
    GDAL.

.. option:: --version

    Reports the GDAL version.

.. option:: --ogr-enabled

    Reports "yes" or "no" to standard output depending on whether OGR is
    built into GDAL.

.. option:: --formats

    Reports which formats are configured into GDAL to stdout.

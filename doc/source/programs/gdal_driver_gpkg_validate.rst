.. _gdal_driver_gpkg_validate:

================================================================================
``gdal driver gpkg validate``
================================================================================

.. versionadded:: 3.13

.. only:: html

    Validate conformance of a GeoPackage dataset against the GeoPackage specification

.. Index:: gdal driver gpkg validate

Synopsis
--------

.. program-output:: gdal driver gpkg validate --help-doc

Description
-----------

Validate if GeoPackage dataset conforms to the GeoPackage specification.

.. note:: This program requires the GDAL Python bindings to be available.

Program-Specific Options
------------------------

.. option:: --full-check

    Enable extensive checks, that may go beyond what is mandated by the
    GeoPackage specification

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/verbose.rst

    .. include:: gdal_options/quiet.rst

.. Return status code
.. ------------------

.. include:: return_code.rst

Examples
--------

.. example:: Check if a GeoPackage file is conformant

   .. code-block:: bash

       gdal driver gpkg validate my.gpkg

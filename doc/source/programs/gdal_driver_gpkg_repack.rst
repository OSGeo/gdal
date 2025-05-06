.. _gdal_driver_gpkg_repack:

================================================================================
``gdal driver gpkg repack``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Repack/vacuum in-place a GeoPackage dataset

.. Index:: gdal driver gpkg repack

Synopsis
--------

.. program-output:: gdal driver gpkg repack --help-doc

Description
-----------

Runs the ``VACUUM`` SQLite3 operation on a GeoPackage dataset to recover
lost space due to updates or deletions.

Examples
--------

.. example::

   .. code-block:: bash

       gdal driver gpkg repack my.gpkg

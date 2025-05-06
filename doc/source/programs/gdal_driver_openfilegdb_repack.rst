.. _gdal_driver_openfilegdb_repack:

================================================================================
``gdal driver openfilegdb repack``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Repack in-place a FileGeodatabase dataset

.. Index:: gdal driver openfilegdb repack

Synopsis
--------

.. program-output:: gdal driver openfilegdb repack --help-doc

Description
-----------

Runs the a repack operation on a FileGeodatabase dataset to recover
lost space due to updates or deletions.

Examples
--------

.. example::

   .. code-block:: bash

       gdal driver openfilegdb repack my.gdb

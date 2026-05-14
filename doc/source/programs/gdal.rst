.. _gdal_program:

================================================================================
Main ``gdal`` entry point
================================================================================

.. versionadded:: 3.11

.. only:: html

    Main "gdal" entry point.

.. Index:: gdal

Synopsis
--------

.. program-output:: gdal --help-doc

.. Return status code
.. ------------------

.. include:: return_code.rst

Examples
--------

.. example::
   :title: Display the GDAL version
   :id: gdal-version

   .. command-output:: gdal --version

.. example::
   :title: Getting information on the file :file:`utm.tif` (with JSON output)

   .. code-block:: console

       $ gdal info utm.tif

.. example::
   :title: Converting file :file:`utm.tif` to GeoPackage raster

   .. code-block:: console

       $ gdal convert utm.tif utm.gpkg

.. example::
   :title: Getting information on all available commands and subcommands as a JSON document.

   .. code-block:: console

       $ gdal --json-usage

.. example::
   :title: Getting list of all formats supported by the current GDAL build, as text

   .. code-block:: console

       $ gdal --formats

.. example::
   :id: gdal-driver-search
   :title: Search for Parquet in the list of all formats using ``jq``

   .. tabs::

      .. code-tab:: bash

        $ gdal --drivers | jq '.[] | select(.short_name == "Parquet")'

      .. code-tab:: ps1

        gdal --drivers | jq '.[] | select(.short_name == \"Parquet\")'


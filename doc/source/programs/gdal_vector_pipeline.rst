.. _gdal_vector_pipeline_subcommand:

================================================================================
"gdal vector pipeline" sub-command
================================================================================

.. versionadded:: 3.11

.. only:: html

    Process a vector dataset.

.. Index:: gdal vector pipeline

Synopsis
--------

.. program-output:: gdal vector pipeline --help-doc=main

A pipeline chains several steps, separated with the ``!`` (exclamation mark) character.
The first step must be ``read``, and the last one ``write``.

Potential steps are:

* read [OPTIONS] <INPUT>

.. program-output:: gdal vector pipeline --help-doc=read

* clip [OPTIONS]

.. program-output:: gdal vector pipeline --help-doc=clip

Details for options can be found in :ref:`gdal_vector_clip_subcommand`.

* filter [OPTIONS]

.. program-output:: gdal vector pipeline --help-doc=filter

Details for options can be found in :ref:`gdal_vector_filter_subcommand`.

* reproject [OPTIONS]

.. program-output:: gdal vector pipeline --help-doc=reproject

* select [OPTIONS]

.. program-output:: gdal vector pipeline --help-doc=select

Details for options can be found in :ref:`gdal_vector_select_subcommand`.

* sql [OPTIONS] <STATEMENT>

.. program-output:: gdal vector pipeline --help-doc=sql

Details for options can be found in :ref:`gdal_vector_sql_subcommand`.

* write [OPTIONS] <OUTPUT>

.. program-output:: gdal vector pipeline --help-doc=write

Description
-----------

:program:`gdal vector pipeline` can be used to process a vector dataset and
perform various on-the-fly processing steps.

Examples
--------

.. example::
   :title: Reproject a GeoPackage file to CRS EPSG:32632 ("WGS 84 / UTM zone 32N")

   .. code-block:: bash

        $ gdal vector pipeline --progress ! read in.gpkg ! reproject --dst-crs=EPSG:32632 ! write out.gpkg --overwrite

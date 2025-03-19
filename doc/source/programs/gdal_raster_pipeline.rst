.. _gdal_raster_pipeline_subcommand:

================================================================================
"gdal raster pipeline" sub-command
================================================================================

.. versionadded:: 3.11

.. only:: html

    Process a raster dataset.

.. Index:: gdal raster pipeline

Synopsis
--------

.. program-output:: gdal raster pipeline --help-doc=main

A pipeline chains several steps, separated with the ``!`` (exclamation mark) character.
The first step must be ``read``, and the last one ``write``.

Potential steps are:

* read [OPTIONS] <INPUT>

.. program-output:: gdal raster pipeline --help-doc=read

* clip [OPTIONS]

.. program-output:: gdal raster pipeline --help-doc=clip

Details for options can be found in :ref:`gdal_raster_clip_subcommand`.

* edit [OPTIONS]

.. program-output:: gdal raster pipeline --help-doc=edit

Details for options can be found in :ref:`gdal_raster_edit_subcommand`.

* reproject [OPTIONS]

.. program-output:: gdal raster pipeline --help-doc=reproject

Details for options can be found in :ref:`gdal_raster_reproject_subcommand`.

* resize [OPTIONS]

.. program-output:: gdal raster pipeline --help-doc=resize

Details for options can be found in :ref:`gdal_raster_resize_subcommand`.

* write [OPTIONS] <OUTPUT>

.. program-output:: gdal raster pipeline --help-doc=write


Description
-----------

:program:`gdal raster pipeline` can be used to process a raster dataset and
perform various on-the-fly processing steps.

Examples
--------

.. example::
   :title: Reproject a GeoTIFF file to CRS EPSG:32632 ("WGS 84 / UTM zone 32N") and adding a metadata item

   .. code-block:: bash

        $ gdal raster pipeline --progress ! read in.tif ! reproject --dst-crs=EPSG:32632 ! edit --metadata AUTHOR=EvenR ! write out.tif --overwrite

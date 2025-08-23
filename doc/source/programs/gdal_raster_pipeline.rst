.. _gdal_raster_pipeline:

================================================================================
``gdal raster pipeline``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Process a raster dataset applying several steps.

.. Index:: gdal raster pipeline

Description
-----------

:program:`gdal raster pipeline` can be used to process a raster dataset and
perform various processing steps that accept raster and generate raster.

For pipelines mixing raster and vector, consult :ref:`gdal_pipeline`.

Synopsis
--------

.. program-output:: gdal raster pipeline --help-doc=main

A pipeline chains several steps, separated with the `!` (exclamation mark) character.
The first step must be ``read``, ``calc``, ``mosaic`` or ``stack``,
and the last one ``write``, ``info`` or ``tile``.
Each step has its own positional or non-positional arguments.
Apart from ``read``, ``calc``, ``mosaic``, ``stack``, ``info``, ``tile`` and ``write``,
all other steps can potentially be used several times in a pipeline.

Potential steps are:

* read

.. program-output:: gdal raster pipeline --help-doc=read

* calc

.. program-output:: gdal raster pipeline --help-doc=calc

Details for options can be found in :ref:`gdal_raster_calc`.

* mosaic

.. program-output:: gdal raster pipeline --help-doc=mosaic

Details for options can be found in :ref:`gdal_raster_mosaic`.

* stack

.. program-output:: gdal raster pipeline --help-doc=stack

Details for options can be found in :ref:`gdal_raster_stack`.

* aspect

.. program-output:: gdal raster pipeline --help-doc=aspect

Details for options can be found in :ref:`gdal_raster_aspect`.

* clip

.. program-output:: gdal raster pipeline --help-doc=clip

Details for options can be found in :ref:`gdal_raster_clip`.

* color-map

.. program-output:: gdal raster pipeline --help-doc=color-map

Details for options can be found in :ref:`gdal_raster_color_map`.

* color-merge

.. program-output:: gdal raster pipeline --help-doc=color-merge

Details for options can be found in :ref:`gdal_raster_color_merge`.

* edit

.. program-output:: gdal raster pipeline --help-doc=edit

Details for options can be found in :ref:`gdal_raster_edit`.

* fill-nodata

.. program-output:: gdal raster pipeline --help-doc=fill-nodata

Details for options can be found in :ref:`gdal_raster_fill_nodata`.

* hillshade

.. program-output:: gdal raster pipeline --help-doc=hillshade

Details for options can be found in :ref:`gdal_raster_hillshade`.

* nodata-to-alpha

.. program-output:: gdal raster pipeline --help-doc=nodata-to-alpha

Details for options can be found in :ref:`gdal_raster_nodata_to_alpha`.

* pansharpen

.. program-output:: gdal raster pipeline --help-doc=pansharpen

Details for options can be found in :ref:`gdal_raster_pansharpen`.

* proximity

.. program-output:: gdal raster pipeline --help-doc=proximity

Details for options can be found in :ref:`gdal_raster_proximity`.

* reproject

.. program-output:: gdal raster pipeline --help-doc=reproject

Details for options can be found in :ref:`gdal_raster_reproject`.

* resize

.. program-output:: gdal raster pipeline --help-doc=resize

Details for options can be found in :ref:`gdal_raster_resize`.

* rgb-to-palette

.. program-output:: gdal raster pipeline --help-doc=rgb-to-palette

Details for options can be found in :ref:`gdal_raster_rgb_to_palette`.

* roughness

.. program-output:: gdal raster pipeline --help-doc=roughness

Details for options can be found in :ref:`gdal_raster_roughness`.

* scale

.. program-output:: gdal raster pipeline --help-doc=scale

Details for options can be found in :ref:`gdal_raster_scale`.

* select

.. program-output:: gdal raster pipeline --help-doc=select

Details for options can be found in :ref:`gdal_raster_select`.

* set-type

.. program-output:: gdal raster pipeline --help-doc=set-type

Details for options can be found in :ref:`gdal_raster_set_type`.

* sieve

.. program-output:: gdal raster pipeline --help-doc=sieve

Details for options can be found in :ref:`gdal_raster_sieve`.

* slope

.. program-output:: gdal raster pipeline --help-doc=slope

Details for options can be found in :ref:`gdal_raster_slope`.

* tpi

.. program-output:: gdal raster pipeline --help-doc=tpi

Details for options can be found in :ref:`gdal_raster_tpi`.

* tri

.. program-output:: gdal raster pipeline --help-doc=tri

Details for options can be found in :ref:`gdal_raster_tri`.

* unscale

.. program-output:: gdal raster pipeline --help-doc=unscale

Details for options can be found in :ref:`gdal_raster_unscale`.

* viewshed

.. program-output:: gdal raster pipeline --help-doc=viewshed

Details for options can be found in :ref:`gdal_raster_viewshed`.

* info

.. versionadded:: 3.12

.. program-output:: gdal raster pipeline --help-doc=info

Details for options can be found in :ref:`gdal_raster_info`.

* tile

.. versionadded:: 3.12

.. program-output:: gdal raster pipeline --help-doc=tile

Details for options can be found in :ref:`gdal_raster_tile`.

* write

.. program-output:: gdal raster pipeline --help-doc=write

GDALG output (on-the-fly / streamed dataset)
--------------------------------------------

A pipeline can be serialized as a JSON file using the ``GDALG`` output format.
The resulting file can then be opened as a raster dataset using the
:ref:`raster.gdalg` driver, and apply the specified pipeline in a on-the-fly /
streamed way.

The ``command_line`` member of the JSON file should nominally be the whole command
line without the final ``write`` step, and is what is generated by
``gdal raster pipeline ! .... ! write out.gdalg.json``.

.. code-block:: json

    {
        "type": "gdal_streamed_alg",
        "command_line": "gdal raster pipeline ! read in.tif ! reproject --dst-crs=EPSG:32632"
    }

The final ``write`` step can be added but if so it must explicitly specify the
``stream`` output format and a non-significant output dataset name.

.. code-block:: json

    {
        "type": "gdal_streamed_alg",
        "command_line": "gdal raster pipeline ! read in.tif ! reproject --dst-crs=EPSG:32632 ! write --output-format=streamed streamed_dataset"
    }


Substitutions
-------------

.. versionadded:: 3.12

It is possible to use :program:`gdal pipeline` to use a pipeline already
serialized in a .gdal.json file, and customize its existing steps, typically
changing input filename, specifying output filename, or adding/modifying arguments
of steps.

See :ref:`gdal_pipeline_substitutions`


Examples
--------

.. example::
   :title: Reproject a GeoTIFF file to CRS EPSG:32632 ("WGS 84 / UTM zone 32N") and adding a metadata item

   .. code-block:: bash

        $ gdal raster pipeline ! read in.tif ! reproject --dst-crs=EPSG:32632 ! edit --metadata AUTHOR=EvenR ! write out.tif --overwrite

.. example::
   :title: Serialize the command of a reprojection of a GeoTIFF file in a GDALG file, and later read it

   .. code-block:: bash

        $ gdal raster pipeline ! read in.tif ! reproject --dst-crs=EPSG:32632 ! write in_epsg_32632.gdalg.json --overwrite
        $ gdal raster info in_epsg_32632.gdalg.json

.. example::
   :title: Mosaic on-the-fly several input files and tile that mosaic.

   .. code-block:: bash

      gdal raster pipeline ! mosaic input*.tif ! tile output_folder



.. below is an allow-list for spelling checker.

.. spelling:word-list::
    tpi
    tri

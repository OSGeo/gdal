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

.. code-block::

    Usage: gdal raster pipeline [OPTIONS] <PIPELINE>

    Process a raster dataset.

    Positional arguments:

    Common Options:
      -h, --help    Display help message and exit
      --json-usage  Display usage as JSON document and exit
      --progress    Display progress bar

    <PIPELINE> is of the form: read [READ-OPTIONS] ( ! <STEP-NAME> [STEP-OPTIONS] )* ! write [WRITE-OPTIONS]


A pipeline chains several steps, separated with the `!` (quotation mark) character.
The first step must be ``read``, and the last one ``write``.

Potential steps are:

* read [OPTIONS] <INPUT>

.. code-block::

    Read a raster dataset.

    Positional arguments:
      -i, --input <INPUT>                                  Input raster dataset [required]

    Advanced Options:
      --if, --input-format <INPUT-FORMAT>                  Input formats [may be repeated]
      --oo, --open-option <KEY=VALUE>                      Open options [may be repeated]

* clip [OPTIONS]

.. code-block::

    Clip a raster dataset.

    Options:
      --bbox <BBOX>                                        Clipping bounding box as xmin,ymin,xmax,ymax
                                                           Mutually exclusive with --like
      --bbox-crs <BBOX-CRS>                                CRS of clipping bounding box
      --like <DATASET>                                     Raster dataset to use as a template for bounds
                                                           Mutually exclusive with --bbox

Details for options can be found in :ref:`gdal_raster_clip_subcommand`.

* edit [OPTIONS]

.. code-block::

    Edit a raster dataset.

    Options:
      --crs <CRS>                                          Override CRS (without reprojection)
      --bbox <EXTENT>                                      Bounding box as xmin,ymin,xmax,ymax
      --metadata <KEY>=<VALUE>                             Add/update dataset metadata item [may be repeated]
      --unset-metadata <KEY>                               Remove dataset metadata item [may be repeated]

Details for options can be found in :ref:`gdal_raster_edit_subcommand`.

* reproject [OPTIONS]

.. code-block::

    Reproject a raster dataset.

    Options:
      -s, --src-crs <SRC-CRS>                              Source CRS
      -d, --dst-crs <DST-CRS>                              Destination CRS
      -r, --resampling <RESAMPLING>                        Resampling method. RESAMPLING=near|bilinear|cubic|cubicspline|lanczos|average|rms|mode|min|max|med|q1|q3|sum (default: nearest)
      --resolution <xres>,<yres>                           Target resolution (in destination CRS units)
      --bbox <xmin>,<ymin>,<xmax>,<ymax>                   Target bounding box (in destination CRS units)
      --target-aligned-pixels                              Round target extent to target resolution

Details for options can be found in :ref:`gdal_raster_reproject_subcommand`.

* resize [OPTIONS]

.. code-block::

    Resize a raster dataset without changing the georeferenced extents.

    Options:
      --size <width>,<height>                              Target size in pixels [required]
      -r, --resampling <RESAMPLING>                        Resampling method. RESAMPLING=nearest|bilinear|cubic|cubicspline|lanczos|average|mode (default: nearest)

Details for options can be found in :ref:`gdal_raster_resize_subcommand`.

* write [OPTIONS] <OUTPUT>

.. code-block::

    Write a raster dataset.

    Positional arguments:
      -o, --output <OUTPUT>                                Output raster dataset [required]

    Options:
      -f, --of, --format, --output-format <OUTPUT-FORMAT>  Output format
      --co, --creation-option <KEY>=<VALUE>                Creation option [may be repeated]
      --overwrite                                          Whether overwriting existing output is allowed



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

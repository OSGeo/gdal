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

.. code-block::

    Usage: gdal vector pipeline [OPTIONS] <PIPELINE>

    Process a vector dataset.

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

    Read a vector dataset.

    Positional arguments:
      -i, --input <INPUT>                                  Input vector dataset [required]

    Options:
      -l, --layer, --input-layer <INPUT-LAYER>             Input layer name(s) [may be repeated]

    Advanced Options:
      --if, --input-format <INPUT-FORMAT>                  Input formats [may be repeated]
      --oo, --open-option <KEY=VALUE>                      Open options [may be repeated]

* clip [OPTIONS]

.. code-block::

    Clip a vector dataset.

    Options:
      --bbox <BBOX>                                        Clipping bounding box as xmin,ymin,xmax,ymax
                                                           Mutually exclusive with --geometry, --like
      --bbox-crs <BBOX-CRS>                                CRS of clipping bounding box
      --geometry <GEOMETRY>                                Clipping geometry (WKT or GeoJSON)
                                                           Mutually exclusive with --bbox, --like
      --geometry-crs <GEOMETRY-CRS>                        CRS of clipping geometry
      --like <DATASET>                                     Dataset to use as a template for bounds
                                                           Mutually exclusive with --bbox, --geometry
      --like-sql <SELECT-STATEMENT>                        SELECT statement to run on the 'like' dataset
      --like-layer <LAYER-NAME>                            Name of the layer of the 'like' dataset
      --like-where <WHERE-EXPRESSION>                      Expression for a WHERE SQL clause to run on the 'like' dataset


Details for options can be found in :ref:`gdal_vector_clip_subcommand`.

* filter [OPTIONS]

.. code-block::

    Filter a vector dataset.

    Options:
      --bbox <BBOX>                                        Bounding box as xmin,ymin,xmax,ymax

* reproject [OPTIONS]

.. code-block::

    Reproject a vector dataset.

    Options:
      -s, --src-crs <SRC-CRS>                              Source CRS
      -d, --dst-crs <DST-CRS>                              Destination CRS [required]

* write [OPTIONS] <OUTPUT>

.. code-block::

    Write a vector dataset.

    Positional arguments:
      -o, --output <OUTPUT>                                Output vector dataset [required]

    Options:
      -f, --of, --format, --output-format <OUTPUT-FORMAT>  Output format
      --co, --creation-option <KEY>=<VALUE>                Creation option [may be repeated]
      --lco, --layer-creation-option <KEY>=<VALUE>         Layer creation option [may be repeated]
      --overwrite                                          Whether overwriting existing output is allowed
      --update                                             Whether updating existing dataset is allowed
      --overwrite-layer                                    Whether overwriting existing layer is allowed
      --append                                             Whether appending to existing layer is allowed
      -l, --output-layer <OUTPUT-LAYER>                    Output layer name


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

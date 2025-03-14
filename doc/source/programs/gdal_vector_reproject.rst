.. _gdal_vector_reproject_subcommand:

================================================================================
"gdal vector reproject" sub-command
================================================================================

.. versionadded:: 3.11

.. only:: html

    Reproject a vector dataset.

.. Index:: gdal vector reproject

Synopsis
--------

.. code-block::

    Usage: gdal vector reproject [OPTIONS] <INPUT> <OUTPUT>

    Reproject a vector dataset.

    Positional arguments:
      -i, --input <INPUT>                                  Input vector dataset [required]
      -o, --output <OUTPUT>                                Output vector dataset [required]

    Common Options:
      -h, --help                                           Display help message and exit
      --version                                            Display GDAL version and exit
      --json-usage                                         Display usage as JSON document and exit
      --drivers                                            Display driver list as JSON document and exit
      --config <KEY>=<VALUE>                               Configuration option [may be repeated]
      --progress                                           Display progress bar

    Options:
      -l, --layer, --input-layer <INPUT-LAYER>             Input layer name(s) [may be repeated]
      -f, --of, --format, --output-format <OUTPUT-FORMAT>  Output format ("GDALG" allowed)
      --co, --creation-option <KEY>=<VALUE>                Creation option [may be repeated]
      --lco, --layer-creation-option <KEY>=<VALUE>         Layer creation option [may be repeated]
      --overwrite                                          Whether overwriting existing output is allowed
      --update                                             Whether to open existing dataset in update mode
      --overwrite-layer                                    Whether overwriting existing layer is allowed
      --append                                             Whether appending to existing layer is allowed
      --output-layer <OUTPUT-LAYER>                        Output layer name
      -s, --src-crs <SRC-CRS>                              Source CRS
      -d, --dst-crs <DST-CRS>                              Destination CRS [required]

    Advanced Options:
      --if, --input-format <INPUT-FORMAT>                  Input formats [may be repeated]
      --oo, --open-option <KEY=VALUE>                      Open options [may be repeated]



Description
-----------

:program:`gdal vector reproject` can be used to reproject a vector dataset.
The program can reproject to any supported projection.

Standard options
++++++++++++++++

.. include:: gdal_options/of_vector.rst

.. include:: gdal_options/co_vector.rst

.. include:: gdal_options/overwrite.rst

.. option:: --update

    Whether the output dataset must be opened in update mode. Implies that
    it already exists. This mode is useful when adding new layer(s) to an
    already existing dataset.

.. option:: --overwrite-layer

    Whether overwriting existing layer(s) is allowed.

.. option:: --append

    Whether appending features to existing layer(s) is allowed

.. option:: -l, --layer <LAYER>

    Name of one or more layers to inspect.  If no layer names are passed, then
    all layers will be selected.

.. option:: --output-layer <OUTPUT-LAYER>

    Output layer name. Can only be used to rename a layer, if there is a single
    input layer.

.. option:: -s, --src-crs <SRC-CRS>

    Set source spatial reference. If not specified the SRS found in the input
    dataset will be used.

    .. include:: options/srs_def_gdalwarp.rst

.. option:: -d, --dst-crs <SRC-CRS>

    Set source spatial reference. If not specified the SRS found in the input
    dataset will be used.

    .. include:: options/srs_def_gdalwarp.rst

Examples
--------

.. example::
   :title: Reproject a GeoPackage file to CRS EPSG:32632 ("WGS 84 / UTM zone 32N")

   .. code-block:: bash

        $ gdal vector reproject --dst-crs=EPSG:32632 in.gpkg out.gpkg --overwrite

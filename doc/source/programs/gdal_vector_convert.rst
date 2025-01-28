.. _gdal_vector_convert_subcommand:

================================================================================
"gdal vector convert" sub-command
================================================================================

.. versionadded:: 3.11

.. only:: html

    Convert a vector dataset.

.. Index:: gdal vector convert

Synopsis
--------

.. code-block::

    Usage: gdal vector convert [OPTIONS] <INPUT> <OUTPUT>

    Convert a vector dataset.

    Positional arguments:
      -i, --input <INPUT>                                  Input vector dataset [required]
      -o, --output <OUTPUT>                                Output vector dataset [required]

    Common Options:
      -h, --help                                           Display help message and exit
      --json-usage                                         Display usage as JSON document and exit
      --progress                                           Display progress bar

    Options:
      -f, --of, --format, --output-format <OUTPUT-FORMAT>  Output format
      --co, --creation-option <KEY>=<VALUE>                Creation option [may be repeated]
      --lco, --layer-creation-option <KEY>=<VALUE>         Layer creation option [may be repeated]
      --overwrite                                          Whether overwriting existing output is allowed
      --update                                             Whether updating existing dataset is allowed
      --overwrite-layer                                    Whether overwriting existing layer is allowed
      --append                                             Whether appending to existing layer is allowed
      -l, --layer, --input-layer <INPUT-LAYER>             Input layer name(s) [may be repeated]
      --output-layer <OUTPUT-LAYER>                        Output layer name

    Advanced Options:
      --oo, --open-option <KEY=VALUE>                      Open options [may be repeated]
      --if, --input-format <INPUT-FORMAT>                  Input formats [may be repeated]


Description
-----------

:program:`gdal vector convert` can be used to convert data data between
different formats.

The following options are available:

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

Advanced options
++++++++++++++++

.. include:: gdal_options/oo.rst

.. include:: gdal_options/if.rst

Examples
--------

.. example::
   :title: Converting file :file:`poly.shp` to a GeoPackage

   .. code-block:: console

       $ gdal vector convert poly.shp output.gpkg

.. example::
   :title: Add new layer from file :file:`line.shp` to an existing GeoPackage, and rename it "lines"

   .. code-block:: console

       $ gdal vector convert --update --output-layer=lines line.shp output.gpkg

.. example::
   :title: Append features from from file :file:`poly2.shp` to an existing layer ``poly`` of a GeoPackage, with progress bar

   .. code-block:: console

       $ gdal vector convert --append --output-layer=poly --progress poly2.shp output.gpkg

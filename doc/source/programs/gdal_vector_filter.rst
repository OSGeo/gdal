.. _gdal_vector_filter_subcommand:

================================================================================
"gdal vector filter" sub-command
================================================================================

.. versionadded:: 3.11

.. only:: html

    Filter a vector dataset.

.. Index:: gdal vector filter

Synopsis
--------

.. code-block::

    Usage: gdal vector filter [OPTIONS] <INPUT> <OUTPUT>

    Clip a vector dataset.

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
      -f, --of, --format, --output-format <OUTPUT-FORMAT>  Output format
      --co, --creation-option <KEY>=<VALUE>                Creation option [may be repeated]
      --lco, --layer-creation-option <KEY>=<VALUE>         Layer creation option [may be repeated]
      --overwrite                                          Whether overwriting existing output is allowed
      --update                                             Whether to open existing dataset in update mode
      --overwrite-layer                                    Whether overwriting existing layer is allowed
      --append                                             Whether appending to existing layer is allowed
      --output-layer <OUTPUT-LAYER>                        Output layer name
      --bbox <BBOX>                                        Bounding box as xmin,ymin,xmax,ymax
      --where <WHERE>|@<filename>                          Attribute query in a restricted form of the queries used in the SQL WHERE statement
      --fields <FIELDS>                                    Selected fields [may be repeated]

    Advanced Options:
      --if, --input-format <INPUT-FORMAT>                  Input formats [may be repeated]
      --oo, --open-option <KEY=VALUE>                      Open options [may be repeated]


Description
-----------

:program:`gdal vector filter` can be used to filter a vector dataset from
their spatial extent or a SQL WHERE clause.

``filter`` can also be used as a step of :ref:`gdal_vector_pipeline_subcommand`.


Standard options
++++++++++++++++

.. include:: gdal_options/of_vector.rst

.. include:: gdal_options/co_vector.rst

.. include:: gdal_options/overwrite.rst

.. option:: --bbox <xmin>,<ymin>,<xmax>,<ymax>

    Bounds to which to filter the dataset. They are assumed to be in the CRS of
    the input dataset.
    The X and Y axis are the "GIS friendly ones", that is X is longitude or easting,
    and Y is latitude or northing.
    Note that filtering does not clip geometries to the bounding box.

.. option:: --where <WHERE>|@<filename>

    Attribute query (like SQL WHERE).


Advanced options
++++++++++++++++

.. include:: gdal_options/oo.rst

.. include:: gdal_options/if.rst

Examples
--------

.. example::
   :title: Select features from a GeoPackage file that intersect the bounding box from longitude 2, latitude 49, to longitude 3, latitude 50 in WGS 84

   .. code-block:: bash

        $ gdal vector filter --bbox=2,49,3,50 in.gpkg out.gpkg --overwrite

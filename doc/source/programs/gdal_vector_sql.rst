.. _gdal_vector_sql_subcommand:

================================================================================
"gdal vector sql" sub-command
================================================================================

.. versionadded:: 3.11

.. only:: html

    Apply SQL statement(s) to a dataset.

.. Index:: gdal vector sql

Synopsis
--------

.. code-block::

    Usage: gdal vector sql [OPTIONS] <INPUT> <OUTPUT> <statement>|@<filename>

    Apply SQL statement(s) to a dataset.

    Positional arguments:
      -i, --input <INPUT>                                  Input vector dataset [required]
      -o, --output <OUTPUT>                                Output vector dataset [required]
      --sql <statement>|@<filename>                        SQL statement(s) [may be repeated] [required]

    Common Options:
      -h, --help                                           Display help message and exit
      --version                                            Display GDAL version and exit
      --json-usage                                         Display usage as JSON document and exit
      --drivers                                            Display driver list as JSON document and exit
      --config <KEY>=<VALUE>                               Configuration option [may be repeated]
      --progress                                           Display progress bar

    Options:
      -f, --of, --format, --output-format <OUTPUT-FORMAT>  Output format
      --co, --creation-option <KEY>=<VALUE>                Creation option [may be repeated]
      --lco, --layer-creation-option <KEY>=<VALUE>         Layer creation option [may be repeated]
      --overwrite                                          Whether overwriting existing output is allowed
      --update                                             Whether to open existing dataset in update mode
      --overwrite-layer                                    Whether overwriting existing layer is allowed
      --append                                             Whether appending to existing layer is allowed
      --output-layer <OUTPUT-LAYER>                        Output layer name(s) [may be repeated]
      --dialect <DIALECT>                                  SQL dialect (e.g. OGRSQL, SQLITE)

    Advanced Options:
      --if, --input-format <INPUT-FORMAT>                  Input formats [may be repeated]
      --oo, --open-option <KEY=VALUE>                      Open options [may be repeated]


Description
-----------

:program:`gdal vector sql` returns one or several layers evaluated from
SQL statements.

Standard options
++++++++++++++++

.. include:: gdal_options/of_vector.rst

.. include:: gdal_options/co_vector.rst

.. include:: gdal_options/overwrite.rst

.. option:: --sql <sql_statement>|@<filename>

    SQL statement to execute that returns a table/layer (typically a SELECT
    statement).

    Can be repeated to generated multiple output layers (repeating --sql <value>
    for each output layer)

.. include:: gdal_options/sql_dialect.rst


.. option:: --output-layer <OUTPUT-LAYER>

    Output SQL layer name(s). If not specified, a generic layer name such as
    "SELECT" may be generated.

    Must be specified as many times as there are SQL statements, either as
    several --output-layer arguments, or a single one with the layer names
    combined with comma.

Advanced options
++++++++++++++++

.. include:: gdal_options/oo.rst

.. include:: gdal_options/if.rst

Examples
--------

.. example::
   :title: Generate a GeoPackage file with a layer sorted by descending population

   .. code-block:: bash

        $ gdal vector sql in.gpkg out.gpkg --output-layer country_sorted_by_pop --sql="SELECT * FROM country ORDER BY pop DESC"

.. example::
   :title: Generate a GeoPackage file with 2 SQL result layers

   .. code-block:: bash

        $ gdal vector sql in.gpkg out.gpkg --output-layer=beginning,end --sql="SELECT * FROM my_layer LIMIT 100" --sql="SELECT * FROM my_layer OFFSET 100000 LIMIT 100"

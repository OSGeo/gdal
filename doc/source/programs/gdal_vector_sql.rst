.. _gdal_vector_sql:

================================================================================
``gdal vector sql``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Apply SQL statement(s) to a dataset.

.. Index:: gdal vector sql

Synopsis
--------

.. program-output:: gdal vector sql --help-doc

Description
-----------

:program:`gdal vector sql` returns one or several layers evaluated from
SQL statements.

Starting with GDAL 3.12, when using :option:`--update`, and without an output
dataset specified, this can be used to execute statements that modify
the input dataset, such as UPDATE, DELETE, etc.

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_vector_compatible.rst

Program-Specific Options
------------------------

.. option:: --output-layer <OUTPUT-LAYER>

    Output SQL layer name(s). If not specified, a generic layer name such as
    "SELECT" may be generated.

    Must be specified as many times as there are SQL statements, either as
    several --output-layer arguments, or a single one with the layer names
    combined with comma.

.. option:: --quiet

    .. versionadded:: 3.12

    Silence potential information messages.

.. option:: --sql <sql_statement>|@<filename>

    SQL statement to execute that returns a table/layer (typically a SELECT
    statement).

    Can be repeated to generated multiple output layers (repeating --sql <value>
    for each output layer)

.. include:: gdal_options/sql_dialect.rst


Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/append_vector.rst

    .. include:: gdal_options/co_vector.rst

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/lco.rst

    .. include:: gdal_options/oo.rst

    .. include:: gdal_options/of_vector.rst

    .. include:: gdal_options/output_oo.rst

    .. include:: gdal_options/overwrite.rst

    .. include:: gdal_options/overwrite_layer.rst

    .. include:: gdal_options/skip_errors.rst

    .. include:: gdal_options/update.rst

    .. include:: gdal_options/upsert.rst


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

.. example::
   :title: Modify in-place a GeoPackage dataset

   .. code-block:: bash

        $ gdal vector sql --update my.gpkg --sql "DELETE FROM countries WHERE pop > 1e6"

.. example::
   :title: Append to an existing layer of a GeoPackage file

   .. code-block:: bash

       $ gdal vector pipeline read europe.gpkg ! \
                              sql --sql "SELECT * FROM country WHERE pop > 1e6" ! \
                              write --append --output-layer-name=world world.gpkg

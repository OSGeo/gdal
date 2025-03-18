.. _gdal_vector_info_subcommand:

================================================================================
"gdal vector info" sub-command
================================================================================

.. versionadded:: 3.11

.. only:: html

    Get information on a vector dataset.

.. Index:: gdal vector info

Synopsis
--------

.. program-output:: gdal vector info --help-doc

Description
-----------

:program:`gdal vector info` lists various information about a GDAL supported
vector dataset.

The following options are available:

Standard options
++++++++++++++++

.. option:: -f, --of, --format, --output-format json|text

    Which output format to use. Default is JSON.

.. option:: -l, --layer <LAYER>

    Name of one or more layers to inspect.  If no layer names are passed and
    :option:`--sql` is not specified, then all layers will be selected.

.. option:: --features

    List all features. Beware of RAM consumption on large layers when using
    JSON output.

.. option:: --sql <statement>|@<filename>

    Execute the indicated SQL statement and return the result. The
    ``@<filename>`` syntax can be used to indicate that the content is
    in the pointed filename (e.g ``@my_select.txt`` where my_select.txt is a file
    in the current directory). Data can also be edited with SQL INSERT, UPDATE,
    DELETE, DROP TABLE, ALTER TABLE etc if the dataset is opened in update mode.
    Editing capabilities depend on the selected
    dialect with :option:`--dialect`.

.. option:: --where <WHERE>|@<filename>

    An attribute query in a restricted form of the queries used in the SQL
    `WHERE` statement. Only features matching the attribute query will be
    reported. The ``@<filename>`` syntax can be used to indicate that the
    content is in the pointed filename.

    Example of ``--where`` and quoting:

    .. code-block:: console

        --where "\"Corner Point Identifier\" LIKE '%__00_00'"

.. option:: --dialect <dialect>

    SQL dialect. In some cases can be used to use (unoptimized) :ref:`ogr_sql_dialect` instead
    of the native SQL of an RDBMS by passing the ``OGRSQL`` dialect value.
    The :ref:`sql_sqlite_dialect` can be selected with the ``SQLITE``
    and ``INDIRECT_SQLITE`` dialect values, and this can be used with any datasource.


Advanced options
++++++++++++++++

.. include:: gdal_options/oo.rst

.. include:: gdal_options/if.rst

Examples
--------

.. example::
   :title: Getting information on the file :file:`poly.gpkg` (with text output), listing all features

   .. code-block:: console

       $ gdal vector info --format=text --features poly.gpkg

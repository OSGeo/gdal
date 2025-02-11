.. _gdal_vector_command:

================================================================================
"gdal vector" command
================================================================================

.. versionadded:: 3.11

.. only:: html

    Entry point for vector commands

.. Index:: gdal vector

Synopsis
--------

.. code-block::

    Usage: gdal vector <SUBCOMMAND>
    where <SUBCOMMAND> is one of:
      - clip:      Clip a vector dataset.
      - convert:   Convert a vector dataset.
      - filter:    Filter a vector dataset.
      - info:      Return information on a vector dataset.
      - pipeline:  Process a vector dataset.
      - reproject: Reproject a vector dataset.
      - select:    Select a subset of fields from a vector dataset.
      - sql:       Apply SQL statement(s) to a dataset.

Available sub-commands
----------------------

- :ref:`gdal_vector_clip_subcommand`
- :ref:`gdal_vector_convert_subcommand`
- :ref:`gdal_vector_filter_subcommand`
- :ref:`gdal_vector_info_subcommand`
- :ref:`gdal_vector_pipeline_subcommand`
- :ref:`gdal_vector_select_subcommand`
- :ref:`gdal_vector_sql_subcommand`

Examples
--------

.. example::
   :title: Getting information on the file :file:`poly.gpkg` (with JSON output)

   .. code-block:: console

       $ gdal vector info poly.gpkg

.. example::
   :title: Converting file :file:`poly.gpkg` to Esri File Geodatabase

   .. code-block:: console

       $ gdal vector convert --format=OpenFileGDB poly.gpkg poly.gdb

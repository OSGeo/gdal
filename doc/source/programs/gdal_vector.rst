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

.. program-output:: gdal vector --help-doc

Available sub-commands
----------------------

- :ref:`gdal_vector_clip_subcommand`
- :ref:`gdal_vector_convert_subcommand`
- :ref:`gdal_vector_edit_subcommand`
- :ref:`gdal_vector_filter_subcommand`
- :ref:`gdal_vector_geom_subcommand`
- :ref:`gdal_vector_info_subcommand`
- :ref:`gdal_vector_pipeline_subcommand`
- :ref:`gdal_vector_rasterize_subcommand`
- :ref:`gdal_vector_reproject_subcommand`
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

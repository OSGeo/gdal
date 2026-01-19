.. _gdal_vector:

================================================================================
``gdal vector``
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

- :ref:`gdal_vector_clip`
- :ref:`gdal_vector_concat`
- :ref:`gdal_vector_convert`
- :ref:`gdal_vector_edit`
- :ref:`gdal_vector_filter`
- :ref:`gdal_vector_index`
- :ref:`gdal_vector_info`
- :ref:`gdal_vector_layer_algebra`
- :ref:`gdal_vector_partition`
- :ref:`gdal_vector_pipeline`
- :ref:`gdal_vector_rasterize`
- :ref:`gdal_vector_reproject`
- :ref:`gdal_vector_select`
- :ref:`gdal_vector_set_field_type`
- :ref:`gdal_vector_set_geom_type`
- :ref:`gdal_vector_sql`
- :ref:`gdal_vector_update`

Examples
--------

.. example::
   :title: Getting information on the file :file:`poly.gpkg` (with text output)

   .. code-block:: console

       $ gdal vector info poly.gpkg

.. example::
   :title: Converting file :file:`poly.gpkg` to Esri File Geodatabase

   .. code-block:: console

       $ gdal vector convert --format=OpenFileGDB poly.gpkg poly.gdb

.. _gdal_vector_filter:

================================================================================
``gdal vector filter``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Filter a vector dataset.

.. Index:: gdal vector filter

Synopsis
--------

.. program-output:: gdal vector filter --help-doc

Description
-----------

:program:`gdal vector filter` can be used to filter a vector dataset from
their spatial extent or a SQL WHERE clause.

``filter`` can also be used as a step of :ref:`gdal_vector_pipeline`.


Standard options
++++++++++++++++

.. include:: gdal_options/of_vector.rst

.. include:: gdal_options/co_vector.rst

.. include:: options/lco.rst

.. include:: gdal_options/overwrite.rst

.. include:: gdal_options/active_layer.rst

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

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_vector_compatible.rst

Examples
--------

.. example::
   :title: Select features from a GeoPackage file that intersect the bounding box from longitude 2, latitude 49, to longitude 3, latitude 50 in WGS 84

   .. code-block:: bash

        $ gdal vector filter --bbox=2,49,3,50 in.gpkg out.gpkg --overwrite

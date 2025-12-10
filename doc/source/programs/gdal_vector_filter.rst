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

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_vector_compatible.rst


Program-Specific Options
------------------------

.. option:: --bbox <xmin>,<ymin>,<xmax>,<ymax>

    Bounds to which to filter the dataset. They are assumed to be in the CRS of
    the input dataset.
    The X and Y axis are the "GIS friendly ones", that is X is longitude or easting,
    and Y is latitude or northing.
    Note that filtering does not clip geometries to the bounding box.

.. option:: --update-extent

    Update layer extent to take into account the filter(s). Otherwise the layer
    extent will generally be the one of the source layer before applying the
    filter(s). Note that using this option requires doing a full scan of the
    filtered layers.

.. option:: --where <WHERE>|@<filename>

    Attribute query (like SQL WHERE).


Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/active_layer.rst

    .. include:: gdal_options/append_vector.rst

    .. include:: gdal_options/co_vector.rst

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/input_layer.rst

    .. include:: gdal_options/lco.rst

    .. include:: gdal_options/oo.rst

    .. include:: gdal_options/of_vector.rst

    .. include:: gdal_options/output_layer.rst

    .. include:: gdal_options/output_oo.rst

    .. include:: gdal_options/overwrite.rst

    .. include:: gdal_options/overwrite_layer.rst

    .. include:: gdal_options/skip_errors.rst

    .. include:: gdal_options/update.rst

    .. include:: gdal_options/upsert.rst

Examples
--------

.. example::
   :title: Select features from a GeoPackage file that intersect the bounding box from longitude 2, latitude 49, to longitude 3, latitude 50 in WGS 84

   .. code-block:: bash

        $ gdal vector filter --bbox=2,49,3,50 in.gpkg out.gpkg --overwrite

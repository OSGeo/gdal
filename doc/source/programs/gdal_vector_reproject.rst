.. _gdal_vector_reproject:

================================================================================
``gdal vector reproject``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Reproject a vector dataset.

.. Index:: gdal vector reproject

Synopsis
--------

.. program-output:: gdal vector reproject --help-doc

Description
-----------

:program:`gdal vector reproject` can be used to reproject a vector dataset.
The program can reproject to any supported projection.

This command can also be used as a step of :ref:`gdal_vector_pipeline`.

Standard options
++++++++++++++++

.. include:: gdal_options/of_vector.rst

.. include:: gdal_options/co_vector.rst

.. include:: options/lco.rst

.. include:: gdal_options/overwrite.rst

.. include:: gdal_options/active_layer.rst

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

.. option:: -s, --src-crs <SRC-CRS>

    Set source spatial reference. If not specified the SRS found in the input
    dataset will be used.

    .. include:: gdal_options/srs_def_gdal_raster_reproject.rst

.. option:: -d, --dst-crs <SRC-CRS>

    Set destination spatial reference.

    .. include:: gdal_options/srs_def_gdal_raster_reproject.rst

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_vector_compatible.rst

Examples
--------

.. example::
   :title: Reproject a GeoPackage file to CRS EPSG:32632 ("WGS 84 / UTM zone 32N")

   .. code-block:: bash

        $ gdal vector reproject --dst-crs=EPSG:32632 in.gpkg out.gpkg --overwrite

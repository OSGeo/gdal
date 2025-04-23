.. _gdal_vector_convert:

================================================================================
``gdal vector convert``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Convert a vector dataset.

.. Index:: gdal vector convert

Synopsis
--------

.. program-output:: gdal vector convert --help-doc

Description
-----------

:program:`gdal vector convert` can be used to convert data data between
different formats.

The following options are available:

Standard options
++++++++++++++++

.. include:: gdal_options/of_vector.rst

.. include:: gdal_options/co_vector.rst

.. include:: gdal_options/lco.rst

.. include:: gdal_options/overwrite.rst

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

Advanced options
++++++++++++++++

.. include:: gdal_options/oo.rst

.. include:: gdal_options/if.rst

Examples
--------

.. example::
   :title: Converting file :file:`poly.shp` to a GeoPackage

   .. code-block:: console

       $ gdal vector convert poly.shp output.gpkg

.. example::
   :title: Add new layer from file :file:`line.shp` to an existing GeoPackage, and rename it "lines"

   .. code-block:: console

       $ gdal vector convert --update --output-layer=lines line.shp output.gpkg

.. example::
   :title: Append features from from file :file:`poly2.shp` to an existing layer ``poly`` of a GeoPackage, with progress bar

   .. code-block:: console

       $ gdal vector convert --append --output-layer=poly --progress poly2.shp output.gpkg

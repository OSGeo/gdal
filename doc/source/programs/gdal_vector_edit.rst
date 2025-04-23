.. _gdal_vector_edit:

================================================================================
``gdal vector edit``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Edit metadata of a vector dataset.

.. Index:: gdal vector edit

Synopsis
--------

.. program-output:: gdal vector edit --help-doc

Description
-----------

:program:`gdal vector edit` can be used to edit metadata of a vector dataset:
its CRS, the layer geometry type, dataset and layer metadata.

``edit`` can also be used as a step of :ref:`gdal_vector_pipeline`.

Features are not modified.

Standard options
++++++++++++++++

.. include:: gdal_options/of_vector.rst

.. include:: gdal_options/co_vector.rst

.. include:: gdal_options/overwrite.rst

.. include:: gdal_options/active_layer.rst

.. option:: --geometry-type <GEOMETRY-TYPE>

   Change the layer geometry type to be one of
   ``GEOMETRY``, ``POINT``, ``LINESTRING``, ``POLYGON``, ``MULTIPOINT``, ``MULTILINESTRING``,
   ``MULTIPOLYGON``, ``GEOMETRYCOLLECTION``, ``CURVE``, ``CIRCULARSTRING``, ``COMPOUNDCURVE``,
   ``SURFACE``, ``CURVEPOLYGON``, ``MULTICURVE``, ``MULTISURFACE``, ``POLYHEDRALSURFACE`` or ``TIN``.
   ``Z``, ``M`` or ``ZM`` suffixes can be appended to the above values to
   indicate the dimensionality.
   Note that feature geometries themselves are not modified. Thus this option
   can be used to fix an inappropriate geometry type at the layer level.

.. option:: --crs <CRS>

    Override CRS, without reprojecting.

    The coordinate reference systems that can be passed are anything supported by the
    :cpp:func:`OGRSpatialReference::SetFromUserInput` call, which includes EPSG Projected,
    Geographic or Compound CRS (i.e. EPSG:4296), a well known text (WKT) CRS definition,
    PROJ.4 declarations, or the name of a .prj file containing a WKT CRS definition.

    ``null`` or ``none`` can be specified to unset an existing CRS.

.. option:: --metadata <KEY>=<VALUE>

    Add/update metadata item, at the dataset level.

.. option:: --unset-metadata <KEY>

    Remove metadata item, at the dataset level.

.. option:: --layer-metadata <KEY>=<VALUE>

    Add/update metadata item, at the layer level.

.. option:: --unset-layer-metadata <KEY>

    Remove metadata item, at the layer level.


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
   :title: Change the CRS of a GeoPackage file (without reprojecting it) and its geometry type

   .. code-block:: bash

        $ gdal vector edit --crs=EPSG:4326 --geometry-type=POLYGONZM in.gpkg out.gpkg --overwrite

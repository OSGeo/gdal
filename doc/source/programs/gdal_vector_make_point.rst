.. _gdal_vector_make_point:

================================================================================
``gdal vector make-point``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Create point geometries from attribute fields containing coordinates.

.. Index:: gdal vector make-point

Synopsis
--------

.. program-output:: gdal vector make-point --help-doc

Description
-----------

:program:`gdal vector make-point` creates point geometries from attribute fields containing coordinates.

Program-Specific Options
------------------------

.. option:: --dst-crs <CRS>

   Optional coordinate reference system to assign to the created points.
   The coordinate reference systems that can be passed are anything supported by the
   :cpp:func:`OGRSpatialReference::SetFromUserInput` call, which includes EPSG Projected,
   Geographic or Compound CRS (i.e. EPSG:4296), a well known text (WKT) CRS definition,
   PROJ.4 declarations, or the name of a .prj file containing a WKT CRS definition.

.. option:: --m <FIELD_NAME>

   Optional field name containing M coordinate values

.. option:: --x <FIELD_NAME>

   Field name containing X coordinate values

.. option:: --y <FIELD_NAME>

   Field name containing Y coordinate values

.. option:: --z <FIELD_NAME>

   Optional field name containing Z coordinate values

Standard Options
----------------

.. collapse:: Details

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



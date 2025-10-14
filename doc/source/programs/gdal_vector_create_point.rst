.. _gdal_vector_create_point:

================================================================================
``gdal vector create-point``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Create point geometries from attribute fields containing coordinates.

.. Index:: gdal vector create-point

Synopsis
--------

.. program-output:: gdal vector create-point --help-doc

Description
-----------

:program:`gdal vector create-point` creates point geometries from attribute fields containing coordinates.

Options
-------

.. option:: --x <FIELD_NAME>

   Field name containing X coordinate values

.. option:: --y <FIELD_NAME>

   Field name containing Y coordinate values

.. option:: --z <FIELD_NAME>

   Optional field name containing Z coordinate values

.. option:: --m <FIELD_NAME>

   Optional field name containing M coordinate values

.. option:: --dst-crs <CRS>

   Optional coordinate reference system to assign to the created points.
   The coordinate reference systems that can be passed are anything supported by the
   :cpp:func:`OGRSpatialReference::SetFromUserInput` call, which includes EPSG Projected,
   Geographic or Compound CRS (i.e. EPSG:4296), a well known text (WKT) CRS definition,
   PROJ.4 declarations, or the name of a .prj file containing a WKT CRS definition.

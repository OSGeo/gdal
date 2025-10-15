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

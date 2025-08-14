.. _gdal_vector_check_geometry:


================================================================================
``gdal vector check-geometry``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Checks that geometries are valid and simple according to the :term:`OGC` Simple Features standard.

.. Index:: gdal vector check-geometry

Synopsis
--------

.. program-output:: gdal vector check-geometry --help-doc

Description
-----------

:program:`gdal vector check-geometry` checks that individual elements of a dataset are valid and simple according to the :term:`OGC` Simple Features standard. For each invalid or non-simple feature, it will output a description and, in most cases, a point location of the error.

The following checks are performed, depending on the input geometry type:

- Polygons and MultiPolygons are checked for validity. A single point error point will be reported even if there are multiple causes of invalidity.
- LineStrings and MultiLineStrings are checked for simplicity. All self-intersection locations will be reported if GDAL is built using version 3.14 or later of the GEOS library. With earlier versions, self-intersection locations are not reported.
- GeometryCollections are checked that their individual elements are valid / simple. A single error point will be reported even if there are multiple causes of invalidity.
- Other geometry types are not checked.

Validity/simplicity checking is performed by the GEOS library and should be consistent with results of software such as PostGIS, QGIS, and shapely that also use that library. GEOS does not consider repeated points to be a cause of invalidity or non-simplicity.

.. warning::

   Curved geometries are linearized before converting to GEOS. Linearized geometries may be valid/simple where the original geometries are not,
   and vice-versa.

Standard options
++++++++++++++++

.. option:: --geometry-field

   Specify the name of the geometry field to test, for layers having multiple geometry fields. By default the first
   geometry field will be used.

.. option:: --include-valid

   Include features for valid/simple geometries in the output, maintaining 1:1 correspondence between input and output features.

.. include:: gdal_options/of_vector.rst

.. include:: gdal_options/co_vector.rst

.. include:: options/lco.rst

.. include:: gdal_options/overwrite.rst

Advanced options
++++++++++++++++

.. include:: gdal_options/oo.rst

.. include:: gdal_options/if.rst

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_vector_compatible_non_natively_streamable.rst

Examples
--------

.. example::
   :title: Print invalidity locations to console

   .. code-block:: console

       $ gdal vector check-geometry ne_10m_admin_0_countries.shp \
                --quiet \
                -f CSV \
                --lco GEOMETRY=AS_XY \
                --lco SEPARATOR=TAB \
                /vsistdout/
       # X	Y	error
       # 35.6210871060001	23.1392929140001	Ring Self-intersection


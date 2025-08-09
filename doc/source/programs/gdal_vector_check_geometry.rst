.. _gdal_vector_check_geometry:


================================================================================
``gdal vector check-geometry``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Checks the elements of a polygonal dataset for validity according to the :term:`OGC` Simple Features standard.

.. Index:: gdal vector check-geometry

Synopsis
--------

.. program-output:: gdal vector check-geometry --help-doc

Description
-----------

:program:`gdal vector check-geometry` checks the individual elements of a polygonal dataset for validity according to the :term:`OGC` Simple Features standard. For each invalid feature, it will output a description and, in most cases, a point location of the error. For features with multiple causes of invalidity, only a single feature will be output. The OGC validity concept is applied only to polygons and multipolygons; line and point geometries are always considered valid.

Validity checking is performed by the GEOS library and should be consistent with results of software such as PostGIS, QGIS, and shapely that also uses that library. GEOS does not consider repeated points to be a cause of invalidity.

.. warning::

   Curved geometries are linearized before converting to GEOS. Linearized geometries may be valid where the original geometries are not,
   and vice-versa.

Standard options
++++++++++++++++

.. option:: --geometry-field

   Specify the name of the geometry field to test, for layers having multiple geometry fields. By default the first
   geometry field will be used.

.. option:: --include-valid

   Include features for valid geometries in the output, maintaining 1:1 correspondence between input and output features.

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


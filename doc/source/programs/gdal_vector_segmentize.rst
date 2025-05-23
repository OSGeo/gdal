.. _gdal_vector_segmentize:

================================================================================
``gdal vector segmentize``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Segmentize geometries of a vector dataset.

.. Index:: gdal vector segmentize

Synopsis
--------

.. program-output:: gdal vector segmentize --help-doc

Description
-----------

:program:`gdal vector segmentize` ensures segments of LineStrings or rings
of Polygons are not longer than the specified max length, by inserting
intermediate points.

It runs the :cpp:func:`OGRGeometry::segmentize` operation.

It can also be used as a step of :ref:`gdal_vector_pipeline`.

Standard options
++++++++++++++++

.. include:: gdal_options/of_vector.rst

.. include:: gdal_options/co_vector.rst

.. include:: options/lco.rst

.. include:: gdal_options/overwrite.rst

.. include:: gdal_options/active_layer.rst

.. include:: gdal_options/active_geometry.rst

.. option:: --max-length <MAX-LENGTH>

    The specified value of this option is the maximum distance between two
    consecutive points of the output geometry before intermediate points are added.
    The unit of the distance is georeferenced units of the source layer.

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
   :title: Make sure that segments of geometries are no longer than one km (assuming the CRS is in meters)

   .. code-block:: bash

        $ gdal vector segmentize --max-length=1000 in.gpkg out.gpkg --overwrite

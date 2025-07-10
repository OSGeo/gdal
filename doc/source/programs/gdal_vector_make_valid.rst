.. _gdal_vector_make_valid:

================================================================================
``gdal vector make-valid``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Fix validity of geometries of a vector dataset

.. Index:: gdal vector make-valid

Synopsis
--------

.. program-output:: gdal vector make-valid --help-doc

Description
-----------

:program:`gdal vector make-valid` ensures that geometries are
valid regarding the rules of the Simple Features specification.

It runs the :cpp:func:`OGRGeometry::MakeValid` operation,
followed by :cpp:func:`OGRGeometryFactory::removeLowerDimensionSubGeoms`
(unless :option:`--keep-lower-dim` is set)

It can also be used as a step of :ref:`gdal_vector_pipeline`.

.. note:: This command requires a GDAL build against the GEOS library.

Standard options
++++++++++++++++

.. include:: gdal_options/of_vector.rst

.. include:: gdal_options/co_vector.rst

.. include:: options/lco.rst

.. include:: gdal_options/overwrite.rst

.. include:: gdal_options/active_layer.rst

.. include:: gdal_options/active_geometry.rst

.. option:: --method=linework|structure

   Algorithm to use when repairing invalid geometries.

   The default ``linework`` method combines all rings into a set of noded lines and
   then extracts valid polygons from that linework. This method keeps all input
   vertices.

   The ``structure`` method (only available with GEOS >= 3.10) first makes all
   rings valid then merges shells and subtracts holes from shells to generate
   valid result. It assumes that holes and shells are correctly categorized.

.. option:: --keep-lower-dim

   Keep components of lower dimension after MakeValid(). For example, MakeValid() may
   return a GeometryCollection of a Polygon and a LineString from a Polygon input.
   By default only the Polygon would be returned. Setting this option will return
   the GeometryCollection.

Advanced options
++++++++++++++++

.. include:: gdal_options/oo.rst

.. include:: gdal_options/if.rst

GDALG output (on-the-fly / streamed dataset)
--------------------------------------------

This program supports serializing the command line as a JSON file using the ``GDALG`` output format.
The resulting file can then be opened as a vector dataset using the
:ref:`vector.gdalg` driver, and apply the specified pipeline in a on-the-fly /
streamed way.

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_vector_compatible.rst

Examples
--------

.. example::
   :title: Basic use of make-valid

   .. code-block:: bash

        $ gdal vector make-valid in.gpkg out.gpkg --overwrite

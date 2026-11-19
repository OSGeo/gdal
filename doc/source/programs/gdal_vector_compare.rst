.. _gdal_vector_compare:

.. program:: gdal_vector_compare

================================================================================
``gdal vector compare``
================================================================================

.. versionadded:: 3.14

.. only:: html

    Compare two vector datasets.

.. Index:: gdal vector compare

Synopsis
--------

.. program-output:: gdal vector compare --help-doc

Description
-----------

:program:`gdal vector compare` compares two GDAL-supported vector datasets and
reports the differences. In addition to reporting differences to the
standard output, the program will also return the difference count in its
exit value.

By convention, the first dataset specified as a positional argument, or through
:option:`--reference`, is assumed to be the reference (or "golden") dataset. The second
dataset specified as a positional argument, or through :option:`--input`, is the
dataset to compare to the reference dataset.

Feature content, field definitions, layer and dataset metadata are checked.
Features are compared in the order in which they are presented by each layer.
If they need to be sorted, this must be done before calling this program, for
example by using :ref:`gdal_vector_sort`.

This program can also be used as the last step of a :ref:`vector pipeline <gdal_vector_pipeline>`.

The following options are available:

Program-Specific Options
------------------------

.. option:: --input <input-dataset>

    The dataset being compared to the reference dataset, referred to as the input
    dataset.

.. option:: --reference <reference-dataset>

    The dataset that is considered correct, referred to as the reference dataset.

.. option:: --lax-geometry

    Performs lax geometry comparison. In that mode, a geometry whose type is
    Point, LineString or Polygon is considered as equivalent to the single-part
    corresponding collection geometry type (MultiPoint, MultiLineString, MultiPolygon).

.. option:: --skip-all-optional

    Whether to skip all optional tests. This is is equivalent to specifying all other
    ``--skip-XXXX`` options.

.. option:: --skip-binary

    Whether to skip exact comparison of binary content.

.. option:: --skip-crs

    Whether to skip comparison of coordinate reference systems (CRS).

.. option:: --skip-metadata

    Whether to skip comparison of dataset and layer metadata.

.. option:: --skip-fid

    Whether to skip comparison of feature IDs.

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/of_vector.rst

    .. include:: gdal_options/input_layer_no_active_layer.rst

.. Return status code
.. ------------------

.. include:: return_code.rst

Examples
--------

.. example::
   :title: Comparing two vector datasets, ignoring feature ID differences

   .. code-block:: bash

       $ gdal vector compare --skip-fid poly.gpkg poly.shp
       0...10...20...30...40...50...60...70...80...90...100 - done.
       Layer poly: Field 'AREA' has width 0 in reference layer, but 12 in input layer
       Layer poly: Field 'AREA' has precision 0 in reference layer, but 3 in input layer
       Layer poly: Field 'EAS_ID' has width 0 in reference layer, but 11 in input layer

       $ echo $?
       3

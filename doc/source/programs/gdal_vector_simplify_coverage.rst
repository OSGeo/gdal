.. _gdal_vector_simplify_coverage:

================================================================================
``gdal vector simplify-coverage``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Simplify boundaries of a polygonal vector dataset.

.. Index:: gdal vector simplify-coverage



Synopsis
--------

.. program-output:: gdal vector simplify-coverage --help-doc

Description
-----------

:program:`gdal vector simplify-coverage` simplifies boundaries of a
polygonal dataset, such that shared boundaries are preserved without
introducing gaps or overlaps between features. Gaps or overlaps already present
in the input dataset will not be corrected.

This requires loading the entire dataset into memory at once. If preservation
of shared boundaries is not needed, :ref:`gdal_vector_simplify` provides
an alternative that can process geometries in a streaming manner.

Simplification is performed using the Visvalingam-Whyatt algorithm.

This command can also be used as a step of :ref:`gdal_vector_pipeline`.

.. only:: html

   .. figure:: ../../images/programs/gdal_vector_simplify_coverage.svg

   Polygon dataset before (left) and after (right) simplification with :program:`gdal vector simplify-coverage`.

.. note:: This command requires a GDAL build against the GEOS library (version 3.12 or greater).

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_vector_compatible_non_natively_streamable.rst

Program-Specific Options
------------------------

.. option:: --preserve-boundary

    Flag indicating whether to preserve (avoid simplifying) external boundaries.
    This can be useful when simplifying a portion of a larger dataset.

.. option:: --tolerance <TOLERANCE>

    Tolerance used for determining whether vertices should be removed.
    Specified in georeferenced units of the source layer.

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/active_layer.rst

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


Examples
--------

.. example::
   :title: Simplify geometries using a tolerance of one square meter (assuming the CRS is in meters)

   .. code-block:: bash

        $ gdal vector simplify-coverage --tolerance=1 in.gpkg out.gpkg --overwrite


.. example::
   :title: Using simplify-coverage as part of a vector pipeline

   This can be used to make the output geometries have a consistent type.

   .. code-block:: bash

       $ gdal vector pipeline ! read tl_2024_us_state.shp ! simplify-coverage --tolerance 2 ! set-geom-type --multi ! write out.gpkg --overwrite


.. below is an allow-list for spelling checker.

.. spelling:word-list::
        Visvalingam
        Whyatt


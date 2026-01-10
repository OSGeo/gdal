.. _gdal_vector_clean_coverage:

================================================================================
``gdal vector clean-coverage``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Adjust the boundaries of a polygonal dataset, removing gaps and overlaps.

.. Index:: gdal vector clean-coverage



Synopsis
--------

.. program-output:: gdal vector clean-coverage --help-doc

Description
-----------

:program:`gdal vector clean-coverage` modifies boundaries of a
polygonal dataset, such that gaps and overlaps between features are removed and
shared edges are defined using the same vertices. The resulting dataset will
form a polygonal coverage that can be used with :ref:`gdal_vector_simplify_coverage`.

This command can also be used as a step of :ref:`gdal_vector_pipeline`, although it
requires loading the entire dataset into memory at once.

.. note:: This command requires a GDAL build against the GEOS library (version 3.14 or greater).

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_vector_compatible_non_natively_streamable.rst

Program-Specific Options
------------------------

.. option:: --input-layer

   Specifies the name of the layer to process. By default, all layers will be processed.

.. option:: --maximum-gap-width <MAXIMUM-GAP-WIDTH>

    Defines the largest area that should be considered a "gap" and merged into
    an adjacent polygon. Gaps will be merged unless a circle with radius larger 
    than the specified tolerance can be inscribed within the gap. The default
    maximum gap width is zero, meaning that gaps are not closed.

    .. figure:: ../../images/programs/gdal_vector_clean_coverage_close_gaps.svg

       Polygon dataset before cleaning (left), after cleaning with default parameters (center),
       and after cleaning with ``--maximum-gap-width 1`` (right).

.. option:: --merge-strategy <MERGE-STRATEGY>

    Method by which overlaps or gaps should be added to adjacent polygons. Options include:
    - longest-border (default): add areas to the polygon with which the longest border is shared
    - max-area: add areas to the largest adjacent polygon
    - min-area: add areas to the smallest adjacent polygon
    - min-index: add areas to the adjacent polygon that was read first

    .. figure:: ../../images/programs/gdal_vector_clean_coverage_merge_max_area.svg

       Polygon dataset before cleaning (left), after cleaning with "longest-border" merge strategy (center) and ``--merge-strategy max-area`` (right).

.. option:: --output-layer

   Specifies the name of the layer to which features will be written.
   By default, the names of the output layers will be the same as the
   names of the input layers.
   
.. option:: --snapping-distance <SNAPPING-DISTANCE>

    Controls the node snapping step, when nearby vertices are snapped together.
    By default, an automatic snapping distance is determined based on an
    analysis of the input. Set to zero to turn off all snapping.

    .. figure:: ../../images/programs/gdal_vector_clean_coverage_snap_distance.svg

       Polygon dataset before cleaning (left), after cleaning with default snapping distance (center), and a more aggressive ``--snapping-distance 0.2`` (right). Note the movement in the
       upper-left corner of the polygon on the right.

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/active_layer.rst

    .. include:: gdal_options/append_vector.rst

    .. include:: gdal_options/co_vector.rst

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/lco.rst

    .. include:: gdal_options/oo.rst

    .. include:: gdal_options/of_vector.rst

    .. include:: gdal_options/output_oo.rst

    .. include:: gdal_options/overwrite.rst

    .. include:: gdal_options/overwrite_layer.rst

    .. include:: gdal_options/skip_errors.rst

    .. include:: gdal_options/update.rst

    .. include:: gdal_options/upsert.rst

Examples
--------

.. example::
   :title: Create and then simplify a polygonal coverage

   .. code-block:: bash

        $ gdal vector pipeline read ne_10m_admin_0_countries.shp ! \
                               make-valid ! \
                               clean-coverage ! \
                               simplify-coverage --tolerance 1 ! \
                               write countries.shp

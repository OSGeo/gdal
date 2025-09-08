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

Standard options
++++++++++++++++

.. option:: --maximum-gap-width <MAXIMUM-GAP-WIDTH>

    Defines the largest area that should be considered a "gap" and merged into
    an adjacent polygon. Gaps will be merged unless a circle with radius larger 
    than the specified tolerance can be inscribed within the gap.

.. option:: --merge-strategy <MERGE-STRATEGY>

    Method by which overlaps or gaps should be added to adjacent polygons. Options include:
    - longest-border (default): add areas to the polygon with which the longest border is shared
    - max-area: add areas to the largest adjacent polygon
    - min-area: add areas to the smallest adjacent polygon
    - min-index: add areas to the adjacent polygon that was read first

.. option:: --snapping-distance <SNAPPING-DISTANCE>

    Controls the node snapping step, when nearby vertices are snapped together.
    By default, an automatic snapping distance is determined based on an
    analysis of the input. Set to zero to turn off all snapping.

.. include:: gdal_options/active_layer.rst

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
   :title: Create and then simplify a polygonal coverage

   .. code-block:: bash

        $ gdal vector pipeline read ne_10m_admin_0_countries.shp ! \
                               make-valid ! \
                               clean-coverage ! \
                               simplify-coverage --tolerance 1 ! \
                               write countries.shp

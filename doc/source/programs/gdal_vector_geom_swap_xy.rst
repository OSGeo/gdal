.. _gdal_vector_geom_swap_xy:

================================================================================
``gdal vector geom swap-xy``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Swap X and Y coordinates of geometries of a vector dataset.

.. Index:: gdal vector geom swap-xy

Synopsis
--------

.. program-output:: gdal vector geom swap-xy --help-doc

Description
-----------

:program:`gdal vector geom swap-xy` swaps the X and Y coordinates of geometries,
typically to work around axis order issues of coordinate reference systems.

It can also be used as a step of :ref:`gdal_vector_pipeline`.

Standard options
++++++++++++++++

.. include:: gdal_options/of_vector.rst

.. include:: gdal_options/co_vector.rst

.. include:: options/lco.rst

.. include:: gdal_options/overwrite.rst

.. include:: gdal_options/active_layer.rst

.. include:: gdal_options/active_geometry.rst

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
   :title: Basic usage

   .. code-block:: bash

        $ gdal vector geom swap-xy in.gpkg out.gpkg --overwrite

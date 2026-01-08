.. _gdal_vector_swap_xy:

================================================================================
``gdal vector swap-xy``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Swap X and Y coordinates of geometries of a vector dataset.

.. Index:: gdal vector swap-xy

Synopsis
--------

.. program-output:: gdal vector swap-xy --help-doc

Description
-----------

:program:`gdal vector swap-xy` swaps the X and Y coordinates of geometries,
typically to work around axis order issues of coordinate reference systems.

It can also be used as a step of :ref:`gdal_vector_pipeline`.

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_vector_compatible.rst

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/active_geometry.rst

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
   :title: Basic usage

   .. code-block:: bash

        $ gdal vector swap-xy in.gpkg out.gpkg --overwrite

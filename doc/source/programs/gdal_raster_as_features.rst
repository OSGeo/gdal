.. _gdal_raster_as_features:

================================================================================
``gdal raster as-features``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Create features representing pixels of a raster

.. Index:: gdal raster as-features

Synopsis
--------

.. program-output:: gdal raster as-features --help-doc

Description
-----------

:program:`gdal raster as-features` creates features representing the pixels in a raster dataset.
Features may be created as polygons, points, or with no geometry at all.
Unlike :ref:`gdal_raster_polygonize`, adjacent pixels having the same values are not combined.

This algorithm can be part of a :ref:`gdal_pipeline` or :ref:`gdal_raster_pipeline`.

The following options are available:

.. option:: --geometry-type

   Specifies the geometry type of the created features. Options available are "none" (default), "point", or "polygon".

.. option:: --skip-nodata

   If set, no features will be emitted for pixels equal to the NoData value.

.. option:: --include-xy

   If set, ``CENTER_X`` and ``CENTER_Y`` fields will be added with the center coordinates of each pixel.

.. option:: --include-row-col

   If set, ``ROW`` and ``COL`` fields will be added with the cell positions.

.. option:: -b <band>

   Specifies the bands for which pixel values should be added as fields of the created features. By default, values will be added for all bands.

.. include:: gdal_options/of_vector.rst

.. include:: gdal_options/co.rst

.. include:: gdal_options/overwrite.rst


Examples
--------

.. example::
   :title: Create points at the center of pixels having a value less than 150

   .. code-block:: bash

       gdal pipeline read input.tif ! 
                reclassify -m "[-inf, 150)=1; DEFAULT=NO_DATA" !
                as-features --geometry-type point --skip-nodata ! 
                write out.shp

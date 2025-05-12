.. _gdal_raster_proximity:

================================================================================
``gdal raster proximity``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Produces a raster proximity map.

.. Index:: gdal raster proximity

Synopsis
--------

.. program-output:: gdal raster proximity --help-doc

Description
-----------

:program:`gdal raster proximity` generates a raster proximity map indicating the distance from
the center of each pixel to the center of the nearest pixel identified as a target pixel.
Target pixels are those in the source raster for which the raster pixel value is in the set of
target pixel values.

Standard options
++++++++++++++++

.. include:: gdal_options/of_raster_create_copy.rst

.. include:: gdal_options/co.rst
.. include:: gdal_options/overwrite.rst

.. include:: gdal_options/ot.rst

.. option:: -b, --band <BAND>

    Input band (1-based index)

.. option:: --target-values <TARGET-VALUES>

    A list of target pixel values in the source image to be considered target pixels.
    If not specified, all non-zero pixels will be considered target pixels.

.. option:: --max-distance <MAX-DISTANCE>

    Maximum distance. The nodata value will be used for pixels beyond this distance (default: 0)

.. option:: --fixed-buffer <FIXED-BUFFER>

    Fixed buffer value (instead of the actual distance) (default: 0)

.. options:: --nodata <NODATA>

    Nodata value for the output raster. If not specified, the nodata value of the input band will be used.

.. options:: --respect-input-nodata

    When set, nodata pixels in the input raster will be treated as nodata in the output raster.


Advanced options
++++++++++++++++

.. include:: gdal_options/oo.rst

.. include:: gdal_options/if.rst


Examples
--------

.. example::

   :title: Proximity map of the of a raster with max distance of 3 pixels

    .. code-block:: bash

        $ gdal raster proximity --max-distance 3  input.tif output.tif
.. _gdal_proximity:

================================================================================
gdal_proximity.py
================================================================================

.. only:: html

    Produces a raster proximity map.

.. Index:: gdal_proximity

Synopsis
--------

.. code-block::

    gdal_proximity.py <srcfile> <dstfile> [-srcband n] [-dstband n]
                      [-of format] [-co name=value]*
                      [-ot Byte/UInt16/UInt32/Float32/etc]
                      [-values n,n,n] [-distunits PIXEL/GEO]
                      [-maxdist n] [-nodata n] [-use_input_nodata YES/NO]
                      [-fixed-buf-val n]

Description
-----------

The :program:`gdal_proximity.py` script generates a raster proximity map indicating
the distance from the center of each pixel to the center of the nearest
pixel identified as a target pixel.  Target pixels are those in the source
raster for which the raster pixel value is in the set of target pixel values.

.. program:: gdal_proximity

.. option:: <srcfile>

    The source raster file used to identify target pixels.

.. option:: <dstfile>

    The destination raster file to which the proximity map will be written.
    It may be a pre-existing file of the same size as srcfile.
    If it does not exist it will be created.

.. option:: -srcband <n>

    Identifies the band in the source file to use (default is 1).

.. option:: -dstband <n>

    Identifies the band in the destination file to use (default is 1).

.. include:: options/of.rst

.. include:: options/co.rst

.. option:: -ot <type>

    Specify a data type supported by the driver, which may be one of the
    following: ``Byte``, ``UInt16``, ``Int16``, ``UInt32``, ``Int32``,
    ``Float32`` (default), or ``Float64``.

.. option:: -values <n>,<n>,<n>

    A list of target pixel values in the source image to be considered target
    pixels. If not specified, all non-zero pixels will be considered target pixels.

.. option:: -distunits PIXEL|GEO

    Indicate whether distances generated should be in pixel or georeferenced
    coordinates (default PIXEL).

.. option:: -maxdist <n>

    The maximum distance to be generated. The nodata value will be used for pixels
    beyond this distance. If a nodata value is not provided, the output band will be
    queried for its nodata value. If the output band does not have a nodata value,
    then the value 65535 will be used. Distance is interpreted in pixels unless
    -distunits GEO is specified.

.. option:: -nodata <n>

    Specify a nodata value to use for the destination proximity raster.

.. option:: -use_input_nodata YES/NO

    Indicate whether nodata pixels in the input raster should be nodata in the output raster (default NO).

.. option:: -fixed-buf-val <n>

    Specify a value to be applied to all pixels that are within the -maxdist of target pixels (including the target pixels) instead of a distance value.

.. _nearblack:

================================================================================
nearblack
================================================================================

.. only:: html

    Convert nearly black/white borders to black.

.. Index:: nearblack

Synopsis
--------

.. code-block::

    nearblack [-of format] [-white | [-color c1,c2,c3...cn]*] [-near dist] [-nb non_black_pixels]
              [-setalpha] [-setmask] [-o outfile] [-q]  [-co "NAME=VALUE"]* infile

Description
-----------

This utility will scan an image and try to set all pixels that are nearly or exactly
black, white or one or more custom colors around the collar to black or white. This
is often used to "fix up" lossy compressed air photos so that color pixels can be
treated as transparent when mosaicing. The output format must use lossless compression
if either alpha band or mask band is not set.

.. program:: nearblack

.. option:: -o <outfile>

    The name of the output file to be created.

.. option:: -of <format>

    Select the output format.
    Starting with GDAL 2.3, if not specified, the format is guessed from the extension (previously
    was ERDAS Imagine .img).
    Use the short format name (GTiff for GeoTIFF for example).

.. option:: -co `"NAME=VALUE"`

    Passes a creation option to the output format driver.  Multiple
    :option:`-co` options may be listed. See :ref:`raster_drivers` format
    specific documentation for legal creation options for each format.

    Only valid when creating a new file

.. option:: -white

    Search for nearly white (255) pixels instead of nearly black pixels.

.. option:: -color <c1,c2,c3...cn>

    Search for pixels near the specified color. May be specified multiple times.
    When -color is specified, the pixels that are considered as the collar are set to 0.

.. option:: -near <dist>

    Select how far from black, white or custom colors the pixel values can be
    and still considered near black, white or custom color.  Defaults to 15.

.. option:: -nb <non_black_pixels>

    number of non-black pixels that can be encountered before the giving up search inwards. Defaults to 2.

.. option:: -setalpha

    Adds an alpha band if the output file is specified and the input file has 3 bands,
    or sets the alpha band of the output file if it is specified and the input file has 4 bands,
    or sets the alpha band of the input file if it has 4 bands and no output file is specified.
    The alpha band is set to 0 in the image collar and to 255 elsewhere.

.. option:: -setmask

    Adds a mask band to the output file,
    or adds a mask band to the input file if it does not already have one and no output file is specified.
    The mask band is set to 0 in the image collar and to 255 elsewhere.

.. option:: -q

    Suppress progress monitor and other non-error output.

.. option:: <infile>

    The input file.  Any GDAL supported format, any number of bands, normally 8bit
    Byte bands.

The algorithm processes the image one scanline at a time.  A scan "in" is done
from either end setting pixels to black or white until at least
"non_black_pixels" pixels that are more than "dist" gray levels away from
black, white or custom colors have been encountered at which point the scan stops.  The nearly
black, white or custom color pixels are set to black or white. The algorithm also scans from
top to bottom and from bottom to top to identify indentations in the top or bottom.

The processing is all done in 8bit (Bytes).

If the output file is omitted, the processed results will be written back
to the input file - which must support update.

C API
-----

This utility is also callable from C with :cpp:func:`GDALNearblack`.

.. versionadded:: 2.1


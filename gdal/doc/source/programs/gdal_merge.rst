.. _gdal_merge:

================================================================================
gdal_merge.py
================================================================================

.. only:: html

    Mosaics a set of images.

.. Index:: gdal_merge

Synopsis
--------

.. code-block::

    gdal_merge.py [-o out_filename] [-of out_format] [-co NAME=VALUE]*
                  [-ps pixelsize_x pixelsize_y] [-tap] [-separate] [-q] [-v] [-pct]
                  [-ul_lr ulx uly lrx lry] [-init "value [value...]"]
                  [-n nodata_value] [-a_nodata output_nodata_value]
                  [-ot datatype] [-createonly] input_files

Description
-----------

This utility will automatically mosaic a set of images.  All the images must
be in the same coordinate system and have a matching number of bands, but
they may be overlapping, and at different resolutions. In areas of overlap,
the last image will be copied over earlier ones.

.. program:: gdal_merge

.. option:: -o <out_filename>

    The name of the output file,
    which will be created if it does not already exist (defaults to "out.tif").

.. include:: options/of.rst

.. include:: options/co.rst

.. include:: options/ot.rst

.. option:: -ps <pixelsize_x> <pixelsize_y>

    Pixel size to be used for the
    output file.  If not specified the resolution of the first input file will
    be used.

.. option:: -tap

    (target aligned pixels) align
    the coordinates of the extent of the output file to the values of the -tr,
    such that the aligned extent includes the minimum extent.

.. option:: -ul_lr <ulx> <uly> <lrx> <lry>

    The extents of the output file.
    If not specified the aggregate extents of all input files will be
    used.

.. option:: -v

    Generate verbose output of mosaicing operations as they are done.

.. option:: -separate

    Place each input file into a separate band.

.. option:: -pct

    Grab a pseudo-color table from the first input image, and use it for the output.
    Merging pseudo-colored images this way assumes that all input files use the same
    color table.

.. option:: -n <nodata_value>

    Ignore pixels from files being merged in with this pixel value.

.. option:: -a_nodata <output_nodata_value>

    Assign a specified nodata value to output bands.

.. option:: -init <"value(s)">

    Pre-initialize the output image bands with these values.  However, it is not
    marked as the nodata value in the output file.  If only one value is given, the
    same value is used in all the bands.

.. option:: -createonly

    The output file is created (and potentially pre-initialized) but no input
    image data is copied into it.

.. note::

    gdal_merge.py is a Python script, and will only work if GDAL was built
    with Python support.

Example
-------

Create an image with the pixels in all bands initialized to 255.

::

    % gdal_merge.py -init 255 -o out.tif in1.tif in2.tif


Create an RGB image that shows blue in pixels with no data. The first two bands
will be initialized to 0 and the third band will be initialized to 255.

::

    % gdal_merge.py -init "0 0 255" -o out.tif in1.tif in2.tif

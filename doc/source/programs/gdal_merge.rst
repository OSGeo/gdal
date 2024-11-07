.. _gdal_merge:

================================================================================
gdal_merge
================================================================================

.. only:: html

    Mosaics a set of images.

.. Index:: gdal_merge

Synopsis
--------

.. code-block::

    gdal_merge [--help] [--help-general]
                  [-o <out_filename>] [-of <out_format>] [-co <NAME>=<VALUE>]...
                  [-ps <pixelsize_x> <pixelsize_y>] [-tap] [-separate] [-q] [-v] [-pct]
                  [-ul_lr <ulx> <uly> <lrx> <lry>] [-init "<value>[ <value>]..."]
                  [-n <nodata_value>] [-a_nodata <output_nodata_value>]
                  [-ot <datatype>] [-createonly] <input_file> [<input_file>]...

Description
-----------

This utility will automatically mosaic a set of images.  All the images must
be in the same coordinate system and have a matching number of bands, but
they may be overlapping, and at different resolutions. In areas of overlap,
the last image will be copied over earlier ones. Nodata/transparency values
are considered on a band by band level, i.e. a nodata/transparent pixel on
one source band will not set a nodata/transparent value on all bands for the
target pixel in the resulting raster nor will it overwrite a valid pixel value.

.. note::

    gdal_merge is a Python utility, and is only available if GDAL Python bindings are available.

.. program:: gdal_merge

.. include:: options/help_and_help_general.rst

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
    Alignment means that xmin / resx, ymin / resy, xmax / resx and ymax / resy are integer values.

.. option:: -ul_lr <ulx> <uly> <lrx> <lry>

    The extents of the output file.
    If not specified the aggregate extents of all input files will be
    used.

.. option:: -q, -quiet

    Suppress progress messages.

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


Examples
--------

Creating an image with the pixels in all bands initialized to 255
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

::

    gdal_merge -init 255 -o out.tif in1.tif in2.tif


Creating an RGB image that shows blue in pixels with no data
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The first two bands will be initialized to 0 and the third band will be
initialized to 255.

::

    gdal_merge -init "0 0 255" -o out.tif in1.tif in2.tif


Passing a large list of files to :program:`gdal_merge`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

A large list of files can be passed to :program:`gdal_merge` by
listing them in a text file using:

.. code-block:: bash

   ls -1 *.tif > tiff_list.txt

on Linux, or

.. code-block:: doscon

   dir /b /s *.tif > tiff_list.txt

on Windows. The text file can then be passed to :program:`gdal_merge`
using `--optfile`:

::

   gdal_merge -o mosaic.tif --optfile tiff_list.txt

Creating an RGB image by merging 3 different greyscale bands
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Conduct "merging by stacking" with the :option:`-separate` flag. Given three
greyscale files that cover the same area, you can run:

.. code-block:: bash

   gdal_merge -separate 1.tif 2.tif 3.tif -o rgb.tif

This maps :file:`1.tif` to red, :file:`2.tif` to green and :file:`3.tif` to blue.

Specifying overlap precedence
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The last image in the input line comes out on top of the finished image stack.
You might also need to use :option:`-n` to note which value should not be
copied into the destination image if it is not already defined as nodata.


.. code-block:: bash

   gdal_merge -o merge.tif -n 0 image1.tif image2.tif image3.tif image4.tif

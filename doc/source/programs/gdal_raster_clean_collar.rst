.. _gdal_raster_clean_collar:

================================================================================
``gdal raster clean-collar``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Clean the collar of a raster dataset, removing noise.

.. Index:: gdal raster clean-collar

Synopsis
--------

.. program-output:: gdal raster clean-collar --help-doc

Description
-----------

:program:`gdal raster clean-collar` scans a (8-bit) image and sets all pixels that
are nearly or exactly black (the default), white or one or more custom colors around the collar
to black or white.
This is often used to "fix up" lossy compressed air photos so that color pixels can be
treated as transparent when mosaicing. The output format must use lossless compression
if either alpha band or mask band is not set.

Program-Specific Options
------------------------

.. option:: --add-alpha

    Adds an alpha band to the output file.
    The alpha band is set to 0 in the image collar and to 255 elsewhere.
    This option is mutually exclusive with :option:`--add-mask`.

.. option:: --add-mask

    Adds a mask band to the output file,
    The mask band is set to 0 in the image collar and to 255 elsewhere.
    This option is mutually exclusive with :option:`--add-alpha`.

.. option:: --algorithm floodfill|twopasses

    Selects the algorithm to apply.

    ``twopasses`` uses a top-to-bottom pass followed by a bottom-to-top pass.
    It may miss with concave areas.
    The algorithm processes the image one scanline at a time.  A scan "in" is done
    from either end setting pixels to black or white until at least
    "non_black_pixels" pixels that are more than "dist" gray levels away from
    black, white or custom colors have been encountered at which point the scan stops.  The nearly
    black, white or custom color pixels are set to black or white. The algorithm also scans from
    top to bottom and from bottom to top to identify indentations in the top or bottom.

    ``floodfill`` (default) uses the `Flood Fill <https://en.wikipedia.org/wiki/Flood_fill#Span_filling>`_
    algorithm and will work with concave areas. It requires creating a temporary
    dataset and is slower than ``twopasses``. When a non-zero value for :option:`--pixel-distance`
    is used, ``twopasses`` is actually called as an initial step of ``floodfill``.


.. option:: --color <c1>,<c2>,<c3>...<cn>|black|white

    Search for pixels near the specified color. May be specified multiple times.
    When this option is specified, the pixels that are considered as the collar are set to 0,
    unless only white is specified, in which case there are set to 255.

.. option:: --color-threshold <val>

    Select how far from black, white or custom colors the pixel values can be
    and still considered near black, white or custom color. Defaults to 15.

.. option:: --pixel-distance <val>

    Number of consecutive transparent pixels that can be encountered before the
    giving up search inwards. Said otherwise, the collar will be extended by
    this number of pixels.
    Defaults to 2.


Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/co.rst

    .. include:: gdal_options/of_raster_create.rst

    .. include:: gdal_options/overwrite.rst

    .. option:: --update

        If only an input dataset is specified, ask for it to be opened in update mode
        If an output dataset is specified, ask for it to be opened in update mode
        (this implies that it already exists).
        Note that updating an existing dataset may lead to file size increase if
        the dataset is compressed, and/or quality loss if lossy compression is used.


Examples
--------

.. example::
   :title: Edit in place a dataset using the default black color as the color of the collar

   .. code-block:: bash

        $ gdal raster clean-collar --update my.tif

.. example::
   :title: Create a new dataset, using black or white as the color of the collar, considering values in the [0,5] range as being considered black, and values in the [250,255] range to be white. It also adds an alpha band to the output dataset.

   .. code-block:: bash

        $ gdal raster clean-collar --add-alpha --color=black --color=white --color-threshold=5 in.tif out.tif

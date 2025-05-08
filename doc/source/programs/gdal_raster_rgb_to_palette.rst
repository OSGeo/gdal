.. _gdal_raster_rgb_to_palette:

================================================================================
``gdal raster rgb-to-palette``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Convert a RGB image into a pseudo-color / paletted image.

.. Index:: gdal raster rgb-to-palette

Synopsis
--------

.. program-output:: gdal raster rgb-to-palette --help-doc

Description
-----------

:program:`gdal raster rgb-to-palette` computes an optimal pseudo-color table
for a given RGB image using a median cut algorithm on a downsampled RGB histogram.
Then it converts the image into a pseudo-colored image using the color table.
This conversion utilizes Floyd-Steinberg dithering (error diffusion) to
maximize output image visual quality.

Standard options
++++++++++++++++

.. include:: gdal_options/of_raster_create_copy.rst

.. include:: gdal_options/co.rst

.. include:: gdal_options/overwrite.rst

.. option:: --color-count <COLOR-COUNT>

    Select the number of colors in the generated color table. Defaults to 256.
    Must be between 2 and 256.

Examples
--------

.. example:: Convert a TIFF file into a paletted PNG image

   .. code-block:: bash

        $ gdal raster rgb-to-palette input.tif output.png

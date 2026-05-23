:orphan:

.. _solution_format_conversion_raster:

Exercise solution for raster format conversion
==============================================

1. Selection of red, green, blue bands:

::

    $ gdal raster select TDR.tif TDR_rgb.tif --band 1,2,3 --creation-option TILED=YES

or

::

    $ gdal raster select TDR.tif TDR_rgb.tif --band red,green,blue --creation-option TILED=YES


2. Scaling to 8-bit with JPEG compressed tiled GeoTIFF output

::

    $ gdal raster scale TDR_rgb.tif TDR_rgb_byte_jpeg.tif \
        --output-data-type uint8  \
        --creation-option COMPRESS=JPEG \
        --creation-option TILED=YES

3. Improving the visual result by clamping the input range to 2 standard
   deviations of the mean.

   First let's collect statistics:

   ::

       $ gdal raster info --stats TDR_rgb_byte_jpeg.tif

   ::

       [ ... snip ... ]
       Band 1 Block=256x256 Type=UInt16, ColorInterp=Red
          Minimum=0.000, Maximum=17143.000, Mean=1439.120, StdDev=582.568
       [ ... snip ... ]
       Band 2 Block=256x256 Type=UInt16, ColorInterp=Green
          Minimum=0.000, Maximum=17660.000, Mean=1469.245, StdDev=543.003
       [ ... snip ... ]
       Band 3 Block=256x256 Type=UInt16, ColorInterp=Blue
          Minimum=0.000, Maximum=18510.000, Mean=1324.608, StdDev=482.921


   So roughly all bands have a mean around 1400 and a standard deviation of 500,
   so using [1400 - 2 * 500, 1400 + 2 * 500] should roughly remove the 5% outliers.

   ::

        $ gdal raster scale TDR_rgb.tif TDR_rgb_byte_jpeg_clamped.tif \
            --input-min 400 \
            --input-max 2400 \
            --output-data-type uint8  \
            --creation-option COMPRESS=JPEG \
            --creation-option TILED=YES

   .. note::

       We have not yet marked the nodata value as being 0, so above statistics
       are not fully correct. We'll do that later.

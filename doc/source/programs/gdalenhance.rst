.. _gdalenhance:

================================================================================
gdalenhance
================================================================================

.. only:: html

    Enhance raster images with LUT-based contrast enhancement.

.. Index:: gdalenhance

Synopsis
--------

.. code-block::

    gdalenhance [--help-general]
                [-of format] [-co "NAME=VALUE"]*
                [-ot {Byte/Int16/UInt16/UInt32/Int32/Float32/Float64/
                        CInt16/CInt32/CFloat32/CFloat64}]
                [-equalize]
                [-config filename]
                <src_raster> <dst_raster>

Description
-----------

The :program:`gdalenhance` utility enhances the contrast of raster 
images using the GDAL library. It reads the input_file and writes 
the output to output_file.

.. program:: gdalenhance

.. include:: options/help_and_help_general.rst

.. option:: -of format

    Select the output format. The default is GeoTIFF (GTiff).

.. include:: options/co.rst

.. include:: options/ot.rst

.. option:: -equalize

    Get source image histogram, and compute equalization luts from
    it, apply lut to the source image.
    
.. option:: -config <filename> 

    Apply custom LUT(look up table) to enhance the contrast of the image.
    The number of lines in the LUT file should be equal to the number of bands
    of the images.
    Exmaple of LUT file:

.. code-block::

    1:Band 0.0:ScaleMin 1.0:ScaleMax
    2:Band 0.0:ScaleMin 1.0:ScaleMax
    3:Band 0.0:ScaleMin 1.0:ScaleMax

Example
--------

.. code-block::

    gdalenhance -equalize rgb.tif rgb_equalize_enhance.tif

apply equalization histogram to enhance the contrast of the image.

.. code-block::

    gdalenhance -config enhance_config rgb.tif rgb_custom_enhance.tif

apply custom LUT (look up-table) to enhance the contrast of the image.
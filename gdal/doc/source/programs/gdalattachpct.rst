.. _gdalattachpct:

================================================================================
gdalattachpct
================================================================================

.. only:: html

    Attach a color table from one file to another file.

.. Index:: gdalattachpct

Synopsis
--------

.. code-block::

    gdalattachpct.py [-of format] palette_file source_file dest_file

Description
-----------

This utility will attach a color table file from an input raster file or a color table file to another raster.

.. program:: gdalattachpct

.. option:: -of <format>

    Select the output format. Starting with
    GDAL 2.3, if not specified, the format is guessed from the extension (previously
    was GTiff). Use the short format name.

.. option:: <palette_file>

    Extract the color table from <palette_file>.
    The<palette_file> must be either a raster file in a GDAL supported format with a palette
    or a color file in a supported format (txt, qml, qlr).

.. option:: <source_file>

    The input file.

.. option:: <dest_file>

    The output RGB file that will be created.

NOTE: gdalattachpct.py is a Python script, and will only work if GDAL was built with Python support.

.. _pct2rgb:

================================================================================
pct2rgb.py
================================================================================

.. only:: html

    Convert an 8bit paletted image to 24bit RGB.

.. Index:: pct2rgb

Synopsis
--------

.. code-block::

    pct2rgb.py [-of format] [-b band] [-rgba] source_file dest_file

Description
-----------

This utility will convert a pseudo-color band on the input file into an output
RGB file of the desired format.

.. program:: pct2rgb

.. option:: -of <format>

    Select the output format. Starting with
    GDAL 2.3, if not specified, the format is guessed from the extension (previously
    was GTiff). Use the short format name.

.. option:: -b <band>

    Band to convert to RGB, defaults to 1.

.. option:: -rgba

    Generate a RGBA file (instead of a RGB file by default).

.. option:: <source_file>

    The input file.

.. option:: <dest_file>

    The output RGB file that will be created.

NOTE: pct2rgb.py is a Python script, and will only work if GDAL was built
with Python support.

The '-expand rgb|rgba' option of :ref:`gdal_translate` obsoletes that utility.

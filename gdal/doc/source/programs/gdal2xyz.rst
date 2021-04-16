.. _gdal2xyz:

================================================================================
gdal2xyz
================================================================================

.. only:: html

    Translates a raster file into `xyz` format.

.. Index:: gdal2xyz

Synopsis
--------

.. code-block::

    gdal2xyz [-help]
        [-skip factor]
        [-srcwin xoff yoff xsize ysize]
        [-b band]* [-allbands]
        [-skipnodata]
        [-csv]
        [-srcnodata value] [-dstnodata value]
        src_dataset [dst_dataset]

Description
-----------

The :program:`gdal2xyz` utility can be used to translate a raster file into xyz format.
`gdal2xyz` can be used as an alternative to `gdal_translate of=xyz`, but supporting other options,
for example:

    * Select more then one band
    * Skip or replace nodata value
    * Return the output as numpy arrays.

.. program:: gdal2xyz

.. option:: -skip

    How many rows/cols to skip in each iteration.

.. option:: -srcwin <xoff> <yoff> <xsize> <ysize>

    Selects a subwindow from the source image for copying based on pixel/line location.

.. option:: -b, -band <band>

    Select band *band* from the input spectral bands for output.
    Bands are numbered from 1 in the order spectral bands are specified.
    Multiple **-b** switches may be used.
    When no -b switch is used, the first band will be used.
    In order to use all input bands set `-allbands` or `-b 0`.

.. option:: -allbands

    Select all input bands.

.. option:: -csv

    Use comma instead of space as a delimiter.

.. option:: -skipnodata

    Exclude the output lines with nodata value (as determined by srcnodata)

.. option:: -srcnodata

    The nodata value of the dataset (for skipping or replacing)
    Default (`None`) - Use the dataset nodata value;
    `Sequence`/`Number` - Use the given nodata value (per band or per dataset).

.. option:: -dstnodata

    Replace source nodata with a given nodata. Has an effect only if not setting `-skipnodata`.
    Default(`None`) - Use `srcnodata`, no replacement;
    `Sequence`/`Number` - Replace the `srcnodata` with the given nodata value (per band or per dataset).

.. option:: -h, --help

    Show help message and exit.

.. option:: <src_dataset>

    The source dataset name. It can be either file name, URL of data source or
    subdataset name for multi-dataset files.

.. option:: <dst_dataset>

    The destination file name.


Examples
--------

::

    gdal2xyz -b 1 -b 2 -dstnodata 0 input.tif output.txt


To create a text file in `xyz` format from the input file `input.tif`, including the first and second bands,
while replacing the dataset nodata values with zeros.

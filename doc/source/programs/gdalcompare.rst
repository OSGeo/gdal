.. _gdalcompare:

================================================================================
gdalcompare.py
================================================================================

.. only:: html

    Compare two images.

.. Index:: gdalcompare.py

Synopsis
--------

.. code-block::

    gdalcompare.py [--help] [--help-general]
                   [-dumpdiffs] [-skip_binary] [-skip_overviews]
                   [-skip_geolocation] [-skip_geotransform]
                   [-skip_metadata] [-skip_rpc] [-skip_srs]
                   [-sds] <golden_file> <new_file>


Description
-----------

The :program:`gdalcompare.py` script compares two GDAL supported datasets and
reports the differences. In addition to reporting differences to the
standard output the script will also return the difference count in its
exit value.

Image pixels, and various metadata are checked. There is also a byte by
byte comparison done which will count as one difference. So if it is
only important that the GDAL visible data is identical a difference
count of 1 (the binary difference) should be considered acceptable.

.. program:: gdalcompare

.. include:: options/help_and_help_general.rst

.. option:: -dumpdiffs

    .. versionadded:: 3.8

    Whether to output the difference in pixel content in a TIFF file in the
    current directory.

.. option:: -skip_binary

    .. versionadded:: 3.8

    Whether to skip exact comparison of binary content.

.. option:: -skip_overviews

    .. versionadded:: 3.8

    Whether to skip comparison of overviews.

.. option:: -skip_geolocation

    .. versionadded:: 3.8

    Whether to skip comparison of GEOLOCATION metadata domain.

.. option:: -skip_geotransform

    .. versionadded:: 3.8

    Whether to skip comparison of geotransform matrix.

.. option:: -skip_metadata

    .. versionadded:: 3.8

    Whether to skip comparison of metadata

.. option:: -skip_rpc

    .. versionadded:: 3.8

    Whether to skip comparison of Rational Polynomial Coefficients (RPC) metadata domain.

.. option:: -skip_srs

    .. versionadded:: 3.8

    Whether to skip comparison of spatial reference systems (SRS).

.. option:: -sds

    If this flag is passed the script will compare all subdatasets that
    are part of the dataset, otherwise subdatasets are ignored.

.. option:: <golden_file>

    The file that is considered correct, referred to as the golden file.

.. option:: <new_file>

    The file being compared to the golden file, referred to as the new
    file.

Note that the :program:`gdalcompare.py` script (like all the other scripts)
can also be called as a library from python code: `from osgeo_utils import gdalcompare`.
The primary entry point is `gdalcompare.compare_db()` which takes a golden
`gdal.Dataset` and a new `gdal.Dataset` as arguments and returns a
difference count (excluding the binary comparison). The
`gdalcompare.compare_sds()` entry point can be used to compare
subdatasets.

Examples
--------

.. code-block:: bash

    gdalcompare.py -dumpdiffs N.tiff S.tiff; echo $?
    Files differ at the binary level.
    Band 1 checksum difference:
      Golden: 36694
      New:    40645
      Pixels Differing: 1509
      Maximum Pixel Difference: 255.0
      Wrote Diffs to: 1.tif
    Differences Found: 2
    2

    gdalcompare.py N.tiff N.tiff; echo $?
    Differences Found: 0
    0


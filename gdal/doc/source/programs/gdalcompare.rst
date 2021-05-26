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

    gdalcompare.py [-sds] golden_file new_file

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

.. option:: -sds

    If this flag is passed the script will compare all subdatasets that
    are part of the dataset, otherwise subdatasets are ignored.

.. option:: <golden_file>

    The file that is considered correct, referred to as the golden file.

.. option:: <new_file>

    The file being compared to the golden file, referred to as the new
    file.

Note that the :program:`gdalcompare.py` script can also be called as a library from
python code though it is not typically in the python path for including.
The primary entry point is `gdalcompare.compare()` which takes a golden
`gdal.Dataset` and a new `gdal.Dataset` as arguments and returns a
difference count (excluding the binary comparison). The
`gdalcompare.compare_sds()` entry point can be used to compare
subdatasets.

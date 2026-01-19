.. _gdal_raster_compare:

================================================================================
``gdal raster compare``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Compare two raster datasets.

.. Index:: gdal raster compare

Synopsis
--------

.. program-output:: gdal raster compare --help-doc

Description
-----------

:program:`gdal raster compare` compares two GDAL supported datasets and
reports the differences. In addition to reporting differences to the
standard output, the program will also return the difference count in its
exit value.

As a convention, the first dataset specified as a positional argument, or through
:option:`--reference`, is assumed to be the reference/exact/golden dataset. The second
dataset specified as a positional argument, or through :option:`--input`, is the
dataset compared to the reference dataset.

Image pixels, and various metadata are checked. There is also a byte by
byte comparison done which will count as one difference. So, if it is
only important that the GDAL visible data is identical, a difference
count of 1 (the binary difference) should be considered acceptable, or you
may specify :option:`--skip-binary` to omit byte to byte comparison.

This program can also be used as the last step of a :ref:`raster pipeline <gdal_raster_pipeline>`.

The following options are available:

Program-Specific Options
------------------------

.. option:: --input <input-dataset>

    The dataset being compared to the reference dataset, referred to as the input
    dataset.

.. option:: --reference <reference-dataset>

    The dataset that is considered correct, referred to as the reference dataset.

.. option:: --skip-all-optional

    Whether to skip all optional tests. This is an alias to defining all other
    ``--skip-XXXX`` options.

    The remaining non-optional tests are:

    - checking dataset width, height and band count
    - checking band description, data type, color interpretation and nodata value
    - checking pixel content

.. option:: --skip-binary

    Whether to skip exact comparison of binary content.

.. option:: --skip-crs

    Whether to skip comparison of coordinate reference systems (CRS).

.. option:: --skip-geolocation

    Whether to skip comparison of GEOLOCATION metadata domain.

.. option:: --skip-geotransform

    Whether to skip comparison of geotransform matrix.

.. option:: --skip-metadata

    Whether to skip comparison of metadata

.. option:: --skip-overview

    Whether to skip comparison of overviews.

.. option:: --skip-rpc

    Whether to skip comparison of Rational Polynomial Coefficients (RPC) metadata domain.

.. option:: --skip-subdataset

    Whether to ignore comparison of all subdatasets that are part of the dataset.

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/oo.rst

Examples
--------

.. example::
   :title: Comparing two datasets that differ by their data types

   .. code-block:: bash

       $ gdal raster compare autotest/gcore/data/byte.tif autotest/gcore/data/uint16.tif
       Reference file has size 736 bytes, whereas input file has size 1136 bytes.
       Reference band 1 has data type Byte, but input band has data type UInt16

       $ echo $?
       2

.. example::
   :title: Comparing two datasets while values have different units

   .. code-block:: bash

       $ gdal raster pipeline read test_in_foot.tif ! calc --calc "X * 0.3048" ! compare --reference=reference_in_meter.tif

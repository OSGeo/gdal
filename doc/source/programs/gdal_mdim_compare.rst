.. _gdal_mdim_compare:

.. program:: gdal_mdim_compare

================================================================================
``gdal mdim compare``
================================================================================

.. versionadded:: 3.14

.. only:: html

    Compare two multidimensional datasets.

.. Index:: gdal mdim compare

Synopsis
--------

.. program-output:: gdal mdim compare --help-doc

Description
-----------

:program:`gdal mdim compare` compares two GDAL supported datasets and
reports the differences. In addition to reporting differences to the
standard output, the program will also return the difference count in its
exit value.

As a convention, the first dataset specified as a positional argument, or through
:option:`--reference`, is assumed to be the reference/exact/golden dataset. The second
dataset specified as a positional argument, or through :option:`--input`, is the
dataset compared to the reference dataset.

Currently only values of arrays are checked. There is also a byte by
byte comparison done which will increase the difference counter by 1. So, if it is
only important that the GDAL visible data is identical, a difference
count of 1 (the binary difference) should be considered acceptable, or you
may specify :option:`--skip-binary` to skip byte to byte comparison.

This program can also be used as the last step of a :ref:`mdim pipeline <gdal_mdim_pipeline>`.

The following options are available:

Program-Specific Options
------------------------

.. option:: --input <input-dataset>

    The dataset being compared to the reference dataset, referred to as the input
    dataset.

.. option:: --reference <reference-dataset>

    The dataset that is considered correct, referred to as the reference dataset.

.. option:: --array <name>

    Select one array, by name or full path.
    This option can be specified several times to operate on different arrays.

.. option:: --metric diff|RMSE|PSNR|all|none

    Comparison metric(s) to apply

    * ``diff`` (the default): compute the number of pixels that differ between
      the reference and the input array, and the maximum absolute value of the
      differences.

    * ``RMSD``: compute the `Root Mean Square Deviation <https://en.wikipedia.org/wiki/Root_mean_square_deviation>`__
      between the reference and the input array.
      This is the standard deviation of the array that would be the difference
      between both arrays. Its expressed in the unit of the values of the arrays.

     .. math::

           \mathrm{MSD} = \frac{1}{N}\sum_{i=1}^{N}(\mathrm{REF}_i - \mathrm{INPUT}_i)^2

     .. math::

           \mathrm{RMSD} = \sqrt{\mathrm{MSD}}

    * ``PSNR``: compute the `Peak Signal-to-Noise Ratio <https://en.wikipedia.org/wiki/Peak_signal-to-noise_ratio>`__
      between the reference and the input array. PSNR is related to RMSD, but
      expressed as a logarithmic quantity using the decibel scale, that is
      independent from the unit and intensity of the values of the arrays.

      .. math::

           \mathrm{PSNR} = 20 \log_{10}\left(\frac{\mathrm{MAX}_I}{\mathrm{RMSD}}\right)

      For arrays of integer data types, :math:`\mathrm{MAX}_I` is the maximum
      value allowed by the data type. For arrays of floating point data type,
      it is the difference between the maximum and minimum values of the pixels
      of the reference array.

    * ``all``: enable all above metrics.

    * ``none``: disable all above metrics.

.. option:: --skip-binary

    Whether to skip exact comparison of binary content.

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/oo.rst

.. Return status code
.. ------------------

.. include:: return_code.rst

Examples
--------

.. example::
   :title: Comparing two datasets

   .. code-block:: bash

       $ gdal mdim compare reference.nc modified.nc

.. _gdal_raster_neighbors:

================================================================================
``gdal raster neighbors``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Compute the value of each pixel from its neighbors (focal statistics).

.. Index:: gdal raster neighbors

Synopsis
--------

.. program-output:: gdal raster neighbors --help-doc

Description
-----------

:program:`gdal raster neighbors` applies a
`kernel (convolution matrix) <https://en.wikipedia.org/wiki/Kernel_(image_processing)>`__
and a function to compute the target pixel value from a neighbourhood of the source pixel value.

At the top edge, the values of the first row are replicated to virtually extend
the source window by a number of rows equal to the radius of the kernel. And
similarly for the bottom, left and right edges. This strategy may potentially
lead to unexpected results depending on the applied kernel.

For a given target cell, if the corresponding source value (the one at the center
of the kernel) is nodata, the resulting pixel is nodata. For other source values
in the neighborhood defined by the kernel, source nodata values are ignored.

This algorithm can be part of a :ref:`gdal_pipeline` or :ref:`gdal_raster_pipeline`.

Options
-------

The following options are available:

.. include:: gdal_options/of_raster_create_copy.rst

.. include:: gdal_options/co.rst

.. include:: gdal_options/overwrite.rst

.. option:: --band <BAND>

    Source band number (indexing starts at one)

.. option:: --kernel

    Convolution kernel to apply to source pixels in a neighborhood of each pixel.

    - either the name of a well-known kernel, among ``edge1``,
      ``edge2``, ``sharpen``, ``box_blur``, ``gaussian_blur_3x3``, ``gaussian_blur_5x5``
      or ``unsharp_masking_5x5``, corresponding to those
      `kernels (convolution matrix) <https://en.wikipedia.org/wiki/Kernel_(image_processing)#Details>`__,
      with the addition of:

      * ``one_3x3`` corresponding to a 3x3 kernel with all coefficients at one:

        .. math::
            \begin{align}
                \begin{bmatrix} 1 & 1 & 1\\ 1 & 1 & 1 \\ 1 & 1 & 1 \end{bmatrix}
            \end{align}

      * ``u`` corresponding to an horizontal derivative with coefficients:

        .. math::
            \begin{align}
                \begin{bmatrix} 0 & 0 & 0\\ -0.5 & 0 & 0.5 \\ 0 & 0 & 0 \end{bmatrix}
            \end{align}

      * ``v`` corresponding to a vertical derivative with coefficients:

        .. math::
            \begin{align}
                \begin{bmatrix} 0 & -0.5 & 0\\ 0 & 0 & 0 \\ 0 & 0.5 & 0 \end{bmatrix}
            \end{align}

    - or the values of the coefficients of the kernel as a square matrix of
      width and height N, where N is an odd number, as
      ``[[val00, val01, ..., val0N],[val10, val11, ..., val1N],...,[valN0, valN1, ..., valNN]]``.

      If :option:`--function` is set to ``mean``, this has the effect of
      adding the sum of the (contributing, i.e. non nodata) weighted source pixels
      and dividing it by the sum of the coefficients in the kernel.

    If  :option:`--kernel` is specified several times, there will be as many
    output bands as the number of values of the option.

.. option:: --function sum|mean|min|max|stddev|median|mode

    Function to apply to the weighted source pixels in the neighborhood defined by the kernel.
    Defaults to ``mean``, except when :option:`--kernel` is set to ``u`` or ``v``, in which
    case ``sum`` is used.

    - ``sum``: computes the sum of the value of contributing source pixels
       multiplied by the corresponding weight of the kernel

    - ``mean``: computes the average of the value of contributing source pixels
       multiplied by the corresponding weight of the kernel

    - ``min``: computes the minimum of the value of contributing source pixels
       multiplied by the corresponding weight of the kernel

    - ``max``: computes the maximum of the value of contributing source pixels
       multiplied by the corresponding weight of the kernel

    - ``stddev``: computes the standard deviation of the value of contributing source pixels
       multiplied by the corresponding weight of the kernel

    - ``median``: computes the median of the value of contributing source pixels
       multiplied by the corresponding weight of the kernel

    - ``mode`` (majority): computes the most frequent of the value of contributing source pixels
       multiplied by the corresponding weight of the kernel

.. option:: --nodata

    Set the NoData value for the output dataset. May be set to "none" to leave the NoData value undefined. If
    :option:`--nodata` is not specified, :program:`gdal raster neighbors` will use a NoData value from the first
    source dataset to have one.


.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_raster_compatible.rst

Examples
--------

.. example::
   :title: Compute the horizontal and vertical derivative of a single-band raster

   .. code-block:: bash

       gdal raster neighbors --kernel u --kernel v in.tif uv.tif


.. example::
   :title: Compute the average value around each pixel in a 3x3 neighborhood

   .. code-block:: bash

       gdal raster neighbors --kernel one_3x3 --function mean in.tif mean.tif


.. example::
   :title: Compute the maximum value around each pixel in a 3x3 neighborhood

   .. code-block:: bash

       gdal raster neighbors --kernel one_3x3 --function max in.tif max.tif


.. example::
   :title: Compute a sharpen filter of a single-band raster, by manually specifying the kernel coefficients.

   .. code-block:: bash

       gdal raster neighbors "--kernel=[[0,-1,0],[-1,5,-1],[0,-1,0]]" in.tif sharpen.tif

.. _gdal_grid_tut:

================================================================================
GDAL Grid Tutorial
================================================================================

Introduction to Gridding
------------------------

Gridding is a process of creating a regular grid (or call it a raster image)
from the scattered data. Typically you have a set of arbitrary scattered over
the region of survey measurements and you would like to convert them into the
regular grid for further processing and combining with other grids.

.. only:: latex

    .. image:: ../../images/grid/gridding.eps
        :alt:   Scattered data gridding

.. only:: not latex

    .. image:: ../../images/grid/gridding.png
        :alt:   Scattered data gridding

This problem can be solved using data interpolation or approximation
algorithms. But you are not limited by interpolation here. Sometimes you don't
need to interpolate your data but rather compute some statistics or data
metrics over the region. Statistics is valuable itself or could be used for
better choosing the interpolation algorithm and parameters.

That is what GDAL Grid API is about. It helps you to interpolate your data
(see `Interpolation of the Scattered Data`_) or compute data metrics (see
`Data Metrics Computation`_).

There are two ways of using this interface. Programmatically it is available
through the :cpp:func:`GDALGridCreate` C function; for end users there is a
:ref:`gdal_grid` utility. The rest of this document discusses details on algorithms
and their parameters implemented in GDAL Grid API.

Interpolation of the Scattered Data
-----------------------------------

Inverse Distance to a Power
+++++++++++++++++++++++++++

The Inverse Distance to a Power gridding method is a weighted average
interpolator. You should supply the input arrays with the scattered data
values including coordinates of every data point and output grid geometry. The
function will compute interpolated value for the given position in output
grid.

For every grid node the resulting value :math:`Z` will be calculated using
formula:

.. math::

    Z=\frac{\sum_{i=1}^n{\frac{Z_i}{r_i^p}}}{\sum_{i=1}^n{\frac{1}{r_i^p}}}

where:

- :math:`Z_i` is a known value at point :math:`i`,
- :math:`r_i` is a distance from the grid node to point :math:`i`,
- :math:`p` is a weighting power,
- :math:`n` is a number of points in `Search Ellipse`_.

The smoothing parameter :math:`s` is used as an additive term in the Euclidean distance calculation:

.. math::

    {r_i}=\sqrt{{r_{ix}}^2 + {r_{iy}}^2 + s^2}

where :math:`r_{ix}` and :math:`r_{iy}` are the horizontal and vertical
distances between the grid node to point :math:`i` respectively.

In this method the weighting factor :math:`w` is

.. math::

    w=\frac{1}{r^p}

See :cpp:class:`GDALGridInverseDistanceToAPowerOptions` for the list of
:cpp:func:`GDALGridCreate` parameters and :ref:`gdal_grid_invdist` for the list
of :ref:`gdal_grid` options.

Moving Average
++++++++++++++

The Moving Average is a simple data averaging algorithm. It uses a moving
window of elliptic form to search values and averages all data points within
the window. `Search Ellipse`_ can be rotated by
specified angle, the center of ellipse located at the grid node. Also the
minimum number of data points to average can be set, if there are not enough
points in window, the grid node considered empty and will be filled with
specified NODATA value.

Mathematically it can be expressed with the formula:

.. math::

     Z=\frac{\sum_{i=1}^n{Z_i}}{n}

where:

- :math:`Z` is a resulting value at the grid node,
- :math:`Z_i` is a known value at point :math:`i`,
- :math:`n` is a number of points in search `Search Ellipse`_.

See :cpp:class:`GDALGridMovingAverageOptions` for the list of :cpp:func:`GDALGridCreate`
parameters and  :ref:`gdal_grid_average` for the list of :ref:`gdal_grid` options.

Nearest Neighbor
++++++++++++++++

The Nearest Neighbor method doesn't perform any interpolation or smoothing, it
just takes the value of nearest point found in grid node search ellipse and
returns it as a result. If there are no points found, the specified NODATA
value will be returned.

See :cpp:class:`GDALGridNearestNeighborOptions` for the list of :cpp:func:`GDALGridCreate`
parameters and :ref:`gdal_grid_nearest` for the list of :ref:`gdal_grid` options.

Data Metrics Computation
------------------------

All the metrics have the same set controlling options. See the
:cpp:class:`GDALGridDataMetricsOptions`.

Minimum Data Value
++++++++++++++++++

Minimum value found in grid node `Search Ellipse`_.
If there are no points found, the specified NODATA value will be returned.

.. math::

     Z=\min{(Z_1,Z_2,\ldots,Z_n)}

where:

- :math:`Z` is a resulting value at the grid node,
- :math:`Z_i` is a known value at point :math:`i`,
- :math:`n` is a number of points in `Search Ellipse`_.

Maximum Data Value
++++++++++++++++++

Maximum value found in grid node `Search Ellipse`_.
If there are no points found, the specified NODATA value will be returned.

.. math::

     Z=\max{(Z_1,Z_2,\ldots,Z_n)}

where:

- :math:`Z` is a resulting value at the grid node,
- :math:`Z_i` is a known value at point :math:`i`,
- :math:`n` is a number of points in `Search Ellipse`_.

Data Range
++++++++++

A difference between the minimum and maximum values found in grid `Search Ellipse`_.
If there are no points found, the
specified NODATA value will be returned.

.. math::

     Z=\max{(Z_1,Z_2,\ldots,Z_n)}-\min{(Z_1,Z_2,\ldots,Z_n)}

where:

- :math:`Z` is a resulting value at the grid node,
- :math:`Z_i` is a known value at point :math:`i`,
- :math:`n` is a number of points in `Search Ellipse`_.

Search Ellipse
--------------

Search window in gridding algorithms specified in the form of rotated ellipse.
It is described by the three parameters:

- :math:`radius_1` is the first radius (:math:`x` axis if rotation angle is 0),
- :math:`radius_2` is the second radius (:math:`y` axis if rotation angle is 0),
- :math:`angle` is a search ellipse rotation angle (rotated counter clockwise).

.. only:: latex

    .. image:: ../../images/grid/ellipse.eps
        :alt:   Search ellipse

.. only:: not latex

    .. image:: ../../images/grid/ellipse.png
        :alt:   Search ellipse

Only points located inside the search ellipse (including its border line) will
be used for computation.

.. _gdal_raster_polygonize:

================================================================================
``gdal raster polygonize``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Create a polygon feature dataset from a raster band

.. Index:: gdal raster polygonize

Synopsis
--------

.. program-output:: gdal raster polygonize --help-doc

Description
-----------

:program:`gdal raster polygonize` creates vector polygons for all connected
regions of pixels in the raster sharing a common pixel value.
Each polygon is created with an attribute indicating the pixel value of that
polygon. A mask (either explicit, or implicit through nodata value or
band) associated to the selected band can be used to determine the which pixels
should be included in the processing.

The utility can create the output vector datasource if it does not already exist,
otherwise it may append to an existing one.

The utility is based on the ::cpp:func:`GDALPolygonize` function which has additional
details on the algorithm.

The following options are available:

Standard options
++++++++++++++++


.. include:: gdal_options/of_vector.rst

.. include:: gdal_options/co.rst

.. include:: options/lco.rst

.. include:: gdal_options/overwrite.rst

.. option:: --update

    Whether the output dataset must be opened in update mode. Implies that
    it already exists.

.. option:: --overwrite-layer

    Whether overwriting the existing output vector layer is allowed.

.. option:: --append

    Whether appending features to the existing output vector layer is allowed

.. option:: -b, --band <BAND>

    Picks a particular band to polygonize. Defaults to band 1.

.. option:: -l, --nln, --layer <LAYER>

    Provides a name for the output vector layer. Defaults to "polygonize".

.. option:: --attribute-name <ELEVATION-NAME>

    The name of the field to create (defaults to "DN").

.. option:: --connectedness 4|8

    Whether to use 4-connectedness or 8-connectedness. With 4-connectedness,
    pixels will be included in the same output feature if they share a side.
    With 8-connectedness, pixels will also be included in the same output
    feature if they share a corner. Default is 4.


Advanced options
++++++++++++++++

.. include:: gdal_options/oo.rst

.. include:: gdal_options/if.rst

Examples
--------

.. example::
   :title: Create a shapefile with polygons for the connected regions of band 1 of the input GeoTIFF.

    .. code-block:: bash

        gdal raster polygonize input.tif polygonize.shp

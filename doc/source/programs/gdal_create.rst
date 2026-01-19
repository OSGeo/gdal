.. _gdal_create:

================================================================================
gdal_create
================================================================================

.. only:: html

    .. versionadded:: 3.2.0

    Create a raster file (without source dataset).

.. Index:: gdal_create

Synopsis
--------

.. program-output:: gdal_create --help-doc

Description
-----------

The :program:`gdal_create` utility can be used to initialize a new raster file,
from its dimensions, band count and set various parameters, such as CRS,
geotransform, nodata value, metadata. It can be used also in special cases,
like creating a PDF file from a XML composition file.

.. tip:: Equivalent in new "gdal" command line interface:

    See :ref:`gdal_raster_create`.

.. program:: gdal_create

.. include:: options/help_and_help_general.rst

.. include:: options/ot.rst

.. include:: options/of.rst

.. option:: -outsize <xsize> <ysize>

    Set the size of the output file in pixels. First value is width. Second one
    is height.

.. option:: -bands <count>

    Number of bands. Defaults to 1 if -outsize is specified, or 0 otherwise.

.. option:: -burn <value>

    A fixed value to burn into a band. A list of :option:`-burn` options
    can be supplied, one per band (the first value will apply to the first band,
    the second one to the second band, etc.). If a single value is specified,
    it will apply to all bands.

.. option:: -a_srs <srs_def>

    Override the projection for the output file.  The<srs_def> may be any of
    the usual GDAL/OGR forms, complete WKT, PROJ.4, EPSG:n or a file containing
    the WKT. No reprojection is done.

.. option:: -a_ullr <ulx> <uly> <lrx> <lry>

    Assign the georeferenced bounds of the output file.

.. option:: -a_nodata <value>

    Assign a specified nodata value to output bands.

.. option:: -mo <META-TAG>=<VALUE>

    Passes a metadata key and value to set on the output dataset if possible.

.. include:: options/co.rst

.. option:: -q

    Suppress progress monitor and other non-error output.

.. option:: -if <input_dataset>

    .. versionadded:: 3.3

    Name of GDAL input dataset that serves as a template for default values of
    options -outsize, -bands, -ot, -a_srs, -a_ullr and -a_nodata.
    Note that the pixel values will *not* be copied.

.. option:: <out_dataset>

    The destination file name.

Examples
--------

.. example::
   :title: Initialize a new GeoTIFF file with a uniform value of 10

   .. code-block:: bash

      gdal_create -outsize 20 20 -a_srs EPSG:4326 -a_ullr 2 50 3 49 -burn 10 out.tif


.. example::
   :title: Create a PDF file from a XML composition file

   .. code-block:: bash

      gdal_create -co COMPOSITION_FILE=composition.xml out.pdf


.. example::
   :title: Initialize a blank GeoTIFF file from an input one

   .. code-block:: bash

      gdal_create -if prototype.tif output.tif


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

.. code-block::


    gdal_create [--help-general]
       [-of format]
       [-outsize xsize ysize]
       [-bands count]
       [-burn value]*
       [-ot {Byte/Int16/UInt16/UInt32/Int32/UInt64/Int64/Float32/Float64/
             CInt16/CInt32/CFloat32/CFloat64}] [-strict]
       [-a_srs srs_def] [-a_ullr ulx uly lrx lry] [-a_nodata value]
       [-mo "META-TAG=VALUE"]* [-q]
       [-co "NAME=VALUE"]*
       [-if input_dataset]
       out_dataset

Description
-----------

The :program:`gdal_create` utility can be used to initialize a new raster file,
from its dimensions, band count and set various parameters, such as CRS,
geotransform, nodata value, metadata. It can be used also in special cases,
like creating a PDF file from a XML composition file.

.. program:: gdal_create

.. include:: options/ot.rst

.. include:: options/of.rst

.. option:: -outsize <xsize> <ysize>

    Set the size of the output file in pixels. First figure is width. Second one
    is height.

.. option:: -bands <count>

    Number of bands. Defaults to 1 if -outsize is specified, or 0 otherwise.

.. option:: -burn <value>

    A fixed value to burn into a band for all objects.  A list of :option:`-burn` options
    can be supplied, one per band being written to.

.. option:: -a_srs <srs_def>

    Override the projection for the output file.  The<srs_def> may be any of
    the usual GDAL/OGR forms, complete WKT, PROJ.4, EPSG:n or a file containing
    the WKT. No reprojection is done.

.. option:: -a_ullr <ulx> <uly> <lrx> <lry>

    Assign the georeferenced bounds of the output file.

.. option:: -a_nodata <value>

    Assign a specified nodata value to output bands.

.. option:: -mo META-TAG=VALUE

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

- Initialize a new GeoTIFF file with a uniform value of 10

    ::

        gdal_create -outsize 20 20 -a_srs EPSG:4326 -a_ullr 2 50 3 49 -burn 10 out.tif


- Create a PDF file from a XML composition file:

    ::

        gdal_create -co COMPOSITION_FILE=composition.xml out.pdf



- Initialize a blank GeoTIFF file from an input one:

    ::

        gdal_create -if prototype.tif output.tif


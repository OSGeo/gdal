.. _gdal_raster_create:

================================================================================
``gdal raster create``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Create a new raster dataset.

.. Index:: gdal raster create

Synopsis
--------

.. program-output:: gdal raster create --help-doc

Description
-----------

:program:`gdal raster create` can be used to initialize a new raster file,
from its dimensions, band count, CRS, geotransform, nodata value and metadata.

The new file can also be initialized from a model input file with the optional
:option:`--like` option, copying its properties but not its pixel values.
By default, metadata and the overview structure are not copied from the model
input file, unless :option:`--copy-metadata` and :option:`--copy-overviews`
are specified.
Options :option:`--size`, :option:`--band-count`, :option:`--datatype`,
:option:`--nodata`, :option:`--crs`, :option:`--bbox`, :option:`--metadata`
can be used to override the values inherited from the model input file.

For GeoTIFF output, setting the ``SPARSE_OK`` creation option to ``YES``
can be useful to create a file of minimum size.`

:program:`gdal raster create` can be used also in special cases, like creating
a PDF file from a XML composition file.

Since GDAL 3.13, ``create`` can also be used as a step of :ref:`gdal_raster_pipeline`.

Program-Specific Options
------------------------

.. option:: --band-count <count>

    Number of bands. Defaults to 1.

.. option:: --bbox <xmin>,<ymin>,<xmax>,ymax>

    Sets the spatial bounding box, in CRS units.
    'x' is longitude values for geographic CRS and easting for projected CRS.
    'y' is latitude values for geographic CRS and northing for projected CRS.

.. option:: --burn <value>

    A fixed value to burn into a band. A list of :option:`--burn` options
    can be supplied, one per band (the first value will apply to the first band,
    the second one to the second band, etc.). If a single value is specified,
    it will apply to all bands.

.. option:: --copy-metadata

    Copy metadata from input dataset and raster bands.
    Requires :option:`--like` to be specified.

.. option:: --copy-overviews

    Create same overview levels as input dataset (but with empty content).
    Requires :option:`--like` to be specified.

.. option:: --crs <CRS>

    Set CRS.

    The coordinate reference systems that can be passed are anything supported by the
    :cpp:func:`OGRSpatialReference::SetFromUserInput` call, which includes EPSG Projected,
    Geographic or Compound CRS (i.e. EPSG:4296), a well known text (WKT) CRS definition,
    PROJ.4 declarations, or the name of a .prj file containing a WKT CRS definition.

    ``null`` or ``none`` can be specified to unset the existing CRS of the
    :option:`--like` dataset if it is set.

    Note that the spatial extent is also left unchanged.

.. option:: --like, --like <DATASET>

    Name of GDAL input dataset that serves as a template for default values of
    options :option:`--size`, :option:`--band-count`, :option:`--datatype`,
    :option:`--crs`, :option:`--bbox` and :option:`--nodata`.
    Note that the pixel values will *not* be copied.

.. option:: --metadata <KEY>=<VALUE>

    Adds a metadata item, at the dataset level.

.. option:: --nodata <value>

    Sets the nodata value.

    ``null`` or ``none`` can be specified to unset the existing nodata value of the
    :option:`--like` dataset if it is set.
    ``nan``, ``inf`` or ``-inf`` are also accepted for floating point rasters
    to respectively mean the special values not-a-number, positive infinity and
    minus infinity.

.. option:: --size <xsize>,<ysize>

    Set the size of the output file in pixels. First value is width. Second one
    is height.


Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/append_raster.rst

    .. include:: gdal_options/co.rst

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/oo.rst

    .. include:: gdal_options/ot.rst

    .. include:: gdal_options/of_raster_create.rst

    .. include:: gdal_options/overwrite.rst


Examples
--------

.. example::
   :title: Initialize a new GeoTIFF file with 3 bands and a uniform value of 10

   .. code-block:: bash

      gdal raster create --size=20,20 --band-count=3 --crs=EPSG:4326 --bbox=2,49,3,50 --burn 10 out.tif


.. example::
   :title: Create a PDF file from a XML composition file

   .. code-block:: bash

      gdal raster create --creation-option COMPOSITION_FILE=composition.xml out.pdf


.. example::
   :title: Initialize a blank GeoTIFF file from an input one

   .. code-block:: bash

      gdal raster create --like prototype.tif output.tif

.. _gdal_vector_rasterize:

================================================================================
``gdal vector rasterize``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Burns vector geometries into a raster

.. Index:: gdal vector rasterize

Synopsis
--------

.. program-output:: gdal vector rasterize --help-doc

Description
-----------

:program:`gdal vector rasterize` burns vector geometries into a raster.

Since GDAL 3.12, this algorithm can be part of a :ref:`gdal_pipeline`.

The following options are available:

Standard options
++++++++++++++++

.. include:: gdal_options/of_raster_create_copy.rst

.. include:: gdal_options/co.rst

.. option:: --band, -b <BAND>

    The band(s) to burn values into. May be repeated.

.. option:: --invert

    Invert rasterization. Burn the fixed burn value, or the burn value associated with the first feature into all parts of the image not inside the provided polygon.

    .. note::

        When the vector features contain a polygon nested within another polygon (like an island in a lake), GDAL must be built against GEOS to get correct results.

.. option:: --all-touched

    Enables the ALL_TOUCHED rasterization option.

    .. note::

       When this option is enabled, the order of the input features (lines or polygons) can affect the results. When two features touch each other, the last one (i.e. topmost) will determine the burned pixel value at the edge. You may wish to use the `--sql` option to reorder the features (ORDER BY) to achieve a more predictable result.

.. option:: --burn <BURN>

    Burn value. May be repeated.

.. option:: -a, --attribute-name <ATTRIBUTE-NAME>

    Attribute name.

.. option:: --3d

    Indicates that a burn value should be extracted from the "Z" values of the feature. Works with points and lines (linear interpolation along each segment). For polygons, works properly only if they are flat (same Z value for all vertices).

.. option:: --add

   Instead of burning a new value, this adds the new value to the existing raster, implies ``--update``. Suitable for heatmaps for instance.

.. option:: -l, --layer, --layer-name <LAYER-NAME>

    Indicates the layer(s) from the datasource that will be used for input features. May be specified multiple times, but at least one layer name or a -sql option must be specified (not both).

.. option:: --where <WHERE>

    An optional SQL WHERE style query expression to be applied to select features to burn in from the input layer(s).

.. option:: --sql <SQL>|@<filename>

    An SQL statement to be evaluated against the datasource to produce a virtual layer of features to be burned in.
    The @filename syntax can be used to indicate that the content is in the pointed filename.

.. include:: gdal_options/sql_dialect.rst

.. option:: --nodata <NODATA>

        Assign a specified nodata value to output bands.

.. option:: --init <INIT>

    Pre-initialize output bands with specified value. May be repeated.

.. option:: --crs <CRS>

    Override the projection for the output file. If not specified, the projection of the input vector file will be used if available. When using this option, no reprojection of features from the CRS of the input vector to the specified CRS of the output raster, so use only this option to correct an invalid source CRS. The ``<CRS>`` may be any of the usual GDAL/OGR forms, complete WKT, PROJ.4, EPSG:n or a file containing the WKT.

.. option:: --transformer-option <NAME>=<VALUE>

    set a transformer option suitable to pass to GDALCreateGenImgProjTransformer2(). This is used when converting geometries coordinates to target raster pixel space. For example this can be used to specify RPC related transformer options.

.. option:: --extent <xmin>,<ymin>,<xmax>,<ymax>

    Set georeferenced extents. The values must be expressed in georeferenced units. If not specified, the extent of the output file will be the extent of the vector layers.

.. option:: --resolution <xres>,<yres>

    Set target resolution. The values must be expressed in georeferenced units. Both must be positive values. Note that `--resolution` cannot be used with `--size`.

.. option:: --tap, --target-aligned-pixels

    (target aligned pixels) Align the coordinates of the extent of the output file to the values of the -tr, such that the aligned extent includes the minimum extent. Alignment means that xmin / resx, ymin / resy, xmax / resx and ymax / resy are integer values.

.. option:: --size <xsize>,<ysize>

    Set output file size in pixels and lines. Note that `--size` cannot be used with `--resolution`.

.. option:: --ot, --datatype, --output-data-type <OUTPUT-DATA-TYPE>

    Force the output bands to be of the indicated data type. Defaults to ``Float64``, unless the attribute field to burn is of type ``Int64``, in which case ``Int64`` is used for the output raster data type if the output driver supports it.

.. option:: --optimization <OPTIMIZATION>

    Force the algorithm used (results are identical). The raster mode is used in most cases and optimise read/write operations. The vector mode is useful with a decent amount of input features and optimise the CPU use. That mode have to be used with tiled images to be efficient. The auto mode (the default) will chose the algorithm based on input and output properties.

.. option:: --update

        Whether to open existing dataset in update mode.

.. option:: --overwrite

        Whether overwriting existing output is allowed.

Advanced options
++++++++++++++++

.. include:: gdal_options/oo.rst

.. include:: gdal_options/if.rst

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. versionadded:: 3.12

.. include:: gdal_cli_include/gdalg_raster_compatible_non_natively_streamable.rst


Examples
--------

.. example::
   :title: Burn a shapefile into a raster

    The following would burn all polygons from :file:`mask.shp` into the RGB TIFF file :file:`work.tif` with the color red (RGB = 255,0,0).

    .. code-block:: bash

        gdal vector rasterize -b 1,2,3 --burn 255,0,0 -l mask mask.shp work.tif

.. example:: Burn a shapefile into a raster using a specific where condition to select features
    :title: The following would burn all "class A" buildings into the output elevation file, pulling the top elevation from the ROOF_H attribute.

    .. code-block:: bash

        gdal vector rasterize -a ROOF_H --where "class='A'" -l footprints footprints.shp city_dem.tif

.. example::
    :title: The following would burn all polygons from :file:`footprint.shp` into a new 1000x1000 rgb TIFF as the color red.

    .. note::
        `-b` is not used; the order of the `--burn` options determines the bands of the output raster.

    .. code-block:: bash

        gdal vector rasterize --burn 255,0,0 --ot Byte --size 1000,1000 -l footprints footprints.shp mask.tif

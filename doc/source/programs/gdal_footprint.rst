.. _gdal_footprint:

================================================================================
gdal_footprint
================================================================================

.. only:: html

    .. versionadded:: 3.8.0

    Compute footprint of a raster.

.. Index:: gdal_footprint

Synopsis
--------

.. code-block::


    gdal_footprint [--help] [--help-general]
       [-b <band>]... [-combine_bands union|intersection]
       [-oo <NAME>=<VALUE>]... [-ovr <index>]
       [-srcnodata "<value>[ <value>]..."]
       [-t_cs pixel|georef] [-t_srs <srs_def>] [-split_polys]
       [-convex_hull] [-densify <value>] [-simplify <value>]
       [-min_ring_area <value>] [-max_points <value>|unlimited]
       [-of <ogr_format>] [-lyr_name <dst_layername>]
       [-location_field_name <field_name>] [-no_location]
       [-write_absolute_path]
       [-dsco <name>=<value>]... [-lco <name>=<value>]... [-overwrite] [-q]
       <src_filename> <dst_filename>


Description
-----------

The :program:`gdal_footprint` utility can be used to compute the footprint of
a raster file, taking into account nodata values (or more generally the mask
band attached to the raster bands), and generating polygons/multipolygons
corresponding to areas where pixels are valid, and write to an output vector file.

The :program:`nearblack` utility may be run as a pre-processing step to generate
proper mask bands.


.. program:: gdal_footprint

.. include:: options/help_and_help_general.rst

.. option:: -b <band>

    Band(s) of interest. Between 1 and the number of bands of the raster.
    May be specified multiple times. If not specified, all bands are taken
    into account. The way multiple bands are combined is controlled by
    :option:`-combine_bands`

.. option:: -combine_bands union|intersection

    Defines how the mask bands of the selected bands are combined to generate
    a single mask band, before being vectorized.
    The default value is ``union``: that is a pixel is valid if it is valid at least
    for one of the selected bands.
    ``intersection`` means that a pixel is valid only ifit is valid for all
    selected bands.

.. option:: -ovr <index>

   To specify which overview level of source file must be used, when overviews
   are available on the source raster. By default the full resolution level is
   used. The index is 0-based, that is 0 means the first overview level.
   This option is mutually exclusive with :option:`-srcnodata`.

.. option:: -srcnodata "<value>[ <value>]..."

    Set nodata values for input bands (different values can be supplied for each band).
    If a single value is specified, it applies to all selected bands.
    If more than one value is supplied, there should be as many values as the number
    of selected bands, and all values should be quoted to keep them
    together as a single operating system argument.
    If the option is not specified, the intrinsic mask band of each selected
    bands will be used.

.. option:: -t_cs pixel|georef

    Target coordinate system. By default if the input dataset is georeferenced,
    ``georef`` is implied, that is the footprint geometry will be expressed
    as coordinates in the CRS of the raster (or the one specified with :option:`-t_srs`).
    If specifying ``pixel``, the coordinates of the footprint geometry are
    column and line indices.

.. option:: -t_srs <srs_def>

    Target CRS of the output file.  The <srs_def> may be any of
    the usual GDAL/OGR forms, complete WKT, PROJ.4, EPSG:n or a file containing
    the WKT.
    Specifying this option implies -t_cs georef
    The footprint is reprojected from the CRS of the source raster to the
    specified CRS.

.. option:: -split_polys

    When specified, multipolygons are split as several features each with one
    single polygon.

.. option:: -convex_hull

    When specified, the convex hull of (multi)polygons is computed.

.. option:: -densify <value>

    The specified value of this option is the maximum distance between 2
    consecutive points of the output geometry.
    The unit of the distance is in pixels if :option:`-t_cs` equals ``pixel``,
    or otherwise in georeferenced units of the source raster.
    This option is applied before the reprojection implied by :option:`-t_srs`.

.. option:: -simplify <value>

    The specified value of this option is the tolerance used to merge
    consecutive points of the output geometry using the
    :cpp:func:`OGRGeometry::Simplify` method.
    The unit of the distance is in pixels if :option:`-t_cs` equals ``pixel``,
    or otherwise in georeferenced units of the target vector dataset.
    This option is applied after the reprojection implied by :option:`-t_srs`.

.. option:: -min_ring_area <value>

    Minimum value for the area of a ring
    The unit of the area is in square pixels if :option:`-t_cs` equals ``pixel``,
    or otherwise in georeferenced units of the target vector dataset.
    This option is applied after the reprojection implied by :option:`-t_srs`

.. option:: -max_points <value>|unlimited

    Maximum number of points of each output geometry (not counting the closing
    point of each ring, which is always identical to the first point).
    The default value is 100. ``unlimited`` can be used to remove that limitation.

.. option:: -q

    Suppress progress monitor and other non-error output.

.. option:: -oo <NAME>=<VALUE>

    Dataset open option (format specific)

.. option:: -of <ogr_format>

    Select the output format. Use the short format name. Guessed from the
    file extension if not specified

.. option:: -location_field_name <field_name>

    .. versionadded:: 3.9.0

    Specifies the name of the field in the resulting vector dataset where the
    path of the input dataset will be stored. The default field name is
    "location". To prevent writing the path of the input dataset, use
    :option:`-no_location`

.. option:: -no_location

    .. versionadded:: 3.9.0

    Turns off the writing of the path of the input dataset as a field in the
    output vector dataset.

.. option:: -write_absolute_path

    .. versionadded:: 3.9.0

    Enables writing the absolute path of the input dataset. By default, the
    filename is written in the location field exactly as specified on the
    command line.

.. option:: -lco <NAME>=<VALUE>

    Layer creation option (format specific)

.. option:: -dsco <NAME>=<VALUE>

    Dataset creation option (format specific)

.. option:: -lyr_name <value>

    Name of the target layer. ``footprint`` if not specified.

.. option:: -overwrite

    Overwrite the target layer if it exists.

.. option:: <src_filename>

    The source raster file name.

.. option:: <dst_filename>

    The destination vector file name. If the file and the output layer exist,
    the new footprint is appended to them, unless :option:`-overwrite` is used.


Post-vectorization geometric operations are applied in the following order:

* optional splitting (:option:`-split_polys`)
* optional densification (:option:`-densify`)
* optional reprojection (:option:`-t_srs`)
* optional filtering by minimum ring area (:option:`-min_ring_area`)
* optional application of convex hull (:option:`-convex_hull`)
* optional simplification (:option:`-simplify`)
* limitation of number of points (:option:`-max_points`)

C API
-----

This utility is also callable from C with :cpp:func:`GDALFootprint`.


Examples
--------

- Compute the footprint of a GeoTIFF file as a GeoJSON file using WGS 84
  longitude, latitude coordinates

    ::

        gdal_footprint -t_srs EPSG:4326 input.tif output.geojson

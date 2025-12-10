.. _gdal_raster_tile:

================================================================================
``gdal raster tile``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Generate tiles in separate files from a raster dataset.

.. Index:: gdal raster tile

Synopsis
--------

.. program-output:: gdal raster tile --help-doc

Description
-----------

:program:`gdal raster tile` generates a directory with small tiles and metadata,
following the OGC WMTS Specification. Simple web pages with viewers based on
OpenLayers and Leaflet are generated as well - so anybody can comfortably
explore your maps on-line and you do not need to install or configure any
special software (like MapServer) and the map displays very fast in the
web browser. You only need to upload the generated directory onto a web server.

It generates PNG files by default, but other output formats can be selected
(JPEG, WEBP, GTiff, etc.). Note that not all formats support non-Byte data types.
JPEG and WEBP only support Byte. PNG supports Byte and UInt16. GeoTiff can
support all data types.

It can also use a tiling scheme fully adapted to the input raster, in terms of
origin and resolution, when using the ``raster`` tiling scheme. In that scheme,
tiles at the maximum zoom level will have the same resolution as the raster.

Starting with GDAL 3.12, :program:`gdal raster tile` can be used as the last
step of a pipeline.

Program-Specific Options
------------------------

.. option:: --aux-xml

    Generate .aux.xml sidecar files when needed

.. option:: --add-alpha

    Whether to force adding an alpha channel. An alpha channel is added by default,
    unless the source dataset has a nodata value and the output format supports it.

.. option:: --copy-src-metadata

    Whether to copy metadata from source dataset into output tiles.


.. option:: --convention xyz|tms

    Tile numbering convention:

    - ``xyz`` (default): from top, as in OGC Web Map Tiling Specification (WMTS)

    - ``tms``: from bottom, as in OSGeo Tile Map Service (TMS) Specification.


.. option:: --dst-nodata <DSTNODATA>

    Destination nodata value.

.. option:: -i, --input <INPUT>

    Input raster dataset [required]

.. option:: --kml

    Generate Google Earth SuperOverlay metadata.
    Not compatible with tiling schemes with non-power-of-two zoom levels.

.. option:: --max-x <MAX-X>

    Maximum tile X coordinate, at maximum zoom level. Can be set to restrict
    the tiling process.

.. option:: --max-y <MAX-Y>

    Maximum tile Y coordinate, at maximum zoom level. Can be set to restrict
    the tiling process.

.. option:: --max-zoom <MAX-ZOOM>

    Maximum zoom level to generate. If not specified, this will be determined by
    comparing the resolution of the input dataset with the closest resolution in
    the list of tile matrix of the tile matrix set.

.. option:: --metadata <KEY>=<VALUE>

    Add metadata item to output tiles [may be repeated]


.. option:: --min-x <MIN-X>

    Minimum tile X coordinate, at maximum zoom level. Can be set to restrict
    the tiling process.

.. option:: --min-y <MIN-Y>

    Minimum tile Y coordinate, at maximum zoom level. Can be set to restrict
    the tiling process.

.. option:: --min-zoom <MIN-ZOOM>

    Minimum zoom level to generate. If not specified, equal to :option:`--max-zoom`.

.. option:: --no-alpha

    Disable the creation of an alpha channel.

.. option:: --no-intersection-ok

    Whether the dataset extent not intersecting the tile matrix is only a warning.
    Otherwise, by default, an error will be emitted if that occurs.

.. option:: -j, --num-threads <value>

   Number of jobs to run at once.
   Default: number of CPUs detected.

.. option:: -o, --output <OUTPUT>

    Output directory [required]

    The directory will be created if it does not exist, but its parent
    directory must exist.

.. option:: --overview-resampling nearest|bilinear|cubic|cubicspline|lanczos|average|rms|mode|min|max|med|q1|q3|sum

    Resampling method used to generate zoom levels lower than the maximum zoom
    level. Defaults to the value of :option:`--resampling` is not specified.

.. option:: --parallel-method thread|spawn|fork

   .. versionadded:: GDAL 3.12

   Parallelization method:

   - ``thread`` uses multi-threading, i.e. parallelized tasks are run within
     the same process.

   - ``spawn`` launches child :program:`gdal` sub-processes.
     ``spawn`` can achieve better "scaling", but requires the :program:`gdal` binary to be
     available. GDAL will try to locate it, but you can also set the ``GDAL_PATH``
     configuration option to point to the directory where the :program:`gdal` binary is
     located.

   - ``fork`` is a variant of ``spawn``, using the system call ``fork``, without
     executing the the :program:`gdal` binary. Such method is not available on
     Windows. On Unix systems where it is available, this method is not recommended
     to be used on multithreaded processes, especially the ones where other threads
     are doing GDAL operation, since it can potentially cause deadlocks.

   If :option:`--parallel-method` is not specified, GDAL will automatically
   decide which of the method is the most appropriate, opting for ``spawn`` if
   the :program:`gdal` binary can be located and a sufficient number of tiles per job
   are generated, and otherwise falling back to ``fork`` on Linux, MacOSX or FreeBSD
   (if no other thread is running), and otherwise to ``thread``.

.. option:: -r, --resampling nearest|bilinear|cubic|cubicspline|lanczos|average|rms|mode|min|max|med|q1|q3|sum

    Resampling method used to generate maximum zoom level, and also lower zoom
    levels if :option:`--overview-resampling` is not specified.
    Defaults to ``cubic``.

.. option:: --resume

    Generate only missing files. Can be used when interrupting a previous run
    to restart it.

.. option:: --skip-blank

    Do not generate fully blank/transparent tiles.

.. option:: --tile-size <PIXELS>

    Width and height of a tile, in pixels. Default is 256 for default tiling schemes.
    Setting it to a higher value enables generating higher DPI tile sets.

.. option:: --tiling-scheme <TILING-SCHEME>

    Tile cutting scheme. Defaults to WebMercatorQuad (also known as Google Maps compatible).

    The following base profiles are available:

    - ``raster``: CRS = CRS of raster, tile size = 256 pixels, raster coverage at all zoom levels.

    - ``WebMercatorQuad``: CRS = EPSG:3857 ("WGS 84 / Pseudo-Mercator"), tile size: 256 pixels, whole world coverage at zoom level 0, one single tile at zoom level 0.

    - ``WorldCRS84Quad``: CRS = EPSG:4326 ("WGS 84"), tile size: 256 pixels, whole world coverage at zoom level 0, two side-by-side tiles at zoom level 0

    - ``WorldMercatorWGS84Quad``: CRS = EPSG:3395 ("WGS 84 / World Mercator"), tile size: 256 pixels, whole world coverage at zoom level 0, one single tile at zoom level 0. This can be seen as a variant of WebMercatorQuad except that it is fully conformal on the WGS84 ellipsoid.

    - ``GoogleCRS84Quad``: CRS = EPSG:4326 ("WGS 84"), tile size: 256 pixels, whole world coverage at zoom level 0, one single tile at zoom level 0 whose first and last 64 lines are blank (the top origin of the tile is a pseudo latitude of 180 degree, and its bottom -180 degree).

    - ``PseudoTMS_GlobalMercator``: CRS = EPSG:3857 ("WGS 84 / Pseudo-Mercator"), tile size: 256 pixels, whole world coverage at zoom level 0, 2x2 tiles at zoom level 0. This is equivalent to ``WebMercatorQuad``, but with the zoom level shifted by one.

    - ``LINZAntarticaMapTileGrid``: LINZ Antarctic Map Tile Grid (Ross Sea Region). See :source_file:`gcore/data/tms_LINZAntarticaMapTileGrid.json`

    - ``APSTILE``: Alaska Polar Stereographic-based tiled coordinate reference system for the Arctic region. See :source_file:`gcore/data/tms_MapML_APSTILE.json`

    - ``CBMTILE``: Lambert Conformal Conic-based tiled coordinate reference system for Canada. See :source_file:`gcore/data/tms_MapML_CBMTILE.json`

    - ``NZTM2000``: LINZ NZTM2000 Map Tile Grid. See :source_file:`gcore/data/tms_NZTM2000.json`

    Additional tiling schemes are discovered from :file:`tms_XXXX.json` files placed in the GDAL data directory.

Advanced Resampling Options
+++++++++++++++++++++++++++

.. option:: --excluded-values=<EXCLUDED-VALUES>

  Comma-separated tuple of values (thus typically "R,G,B"), that are ignored
  as contributing source pixels during resampling. The number of values in
  the tuple must be the same as the number of bands, excluding the alpha band.
  Several tuples of excluded values may be specified using the "(R1,G1,B2),(R2,G2,B2)" syntax.
  Only taken into account for average resampling.
  This concept is a bit similar to nodata/alpha, but the main difference is
  that pixels matching one of the excluded value tuples are still considered
  as valid, when determining the target pixel validity/density.

.. versionadded:: 3.11.1

.. option:: --excluded-values-pct-threshold=<EXCLUDED-VALUES-PCT-THRESHOLD>

  Minimum percentage of source pixels that must be set at one of the --excluded-values to cause the excluded
  value, that is in majority among source pixels, to be used as the target pixel value. Default value is 50(%)

.. versionadded:: 3.11.1

.. option:: --nodata-values-pct-threshold=<NODATA-VALUES-PCT-THRESHOLD>

  Minimum percentage of source pixels that must be at nodata (or alpha=0 or any
  other way to express transparent pixel) to cause the target pixel value to
  be transparent. Default value is 100 (%), which means that a target pixel is
  transparent only if all contributing source pixels are transparent.
  Only taken into account for average resampling.

.. versionadded:: 3.11.1


Publication Options
+++++++++++++++++++

.. option:: --webviewer none|all|leaflet|openlayers|mapml|stac

    Web viewer to generate. Defaults to ``all``. Those web viewers are created
    at the root of the output directory.

    ``stac`` generates a :file:`stacta.json` file, following the
    Spatio-Temporal Asset Catalog Tiled Assets specification, and that can
    be opened by the :ref:`STACTA driver <raster.stacta>`.

.. option:: --url

    URL address where the generated tiles are going to be published.

.. option:: --title <TITLE>

    Title of the map.

.. option:: --copyright <COPYRIGHT>

    Copyright for the map.


MapML options
+++++++++++++

MapML webviewer can only be generated if :option:`--convention` is set to the
default value ``xyz``.

The following profiles are supported:

- ``WebMercatorQuad``: mapped to OSMTILE MapML tiling scheme
- ``GoogleCRS84Quad``: mapped to WGS84 MapML tiling scheme
- ``APSTILE``: from the tms_MapML_APSTILE.json data file
- ``CBMTILE``: from the tms_MapML_CBMTILE.json data file

The generated MapML file in the output directory is ``mapml.mapl``

Available options are:

.. option:: --mapml-template <filename>

    Filename of a template mapml file where variables will
    be substituted. If not specified, the generic
    template_tiles.mapml file from GDAL data resources
    will be used

The :option:`--url` option is also used to substitute ``${URL}`` in the template MapML file.


Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/co.rst

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/oo.rst

    .. option:: -f, --of, --format, --output-format <OUTPUT-FORMAT>

        Which output raster format to use. Allowed values may be given by
        ``gdal --formats | grep raster | grep rw | sort``

        Defaults to PNG.


Examples
--------

.. example::
   :title: Generate PNG tiles with WebMercatorQuad tiling scheme for zoom levels 2 to 5.

   .. code-block:: bash

      gdal raster tile --min-zoom=2 --max-zoom=5 input.tif output_folder

.. example::
   :title: Retile a raster using its origin and resolution

   .. code-block:: bash

      gdal raster tile --tiling-scheme raster input.tif output_folder

.. example::
   :title: Creating a tiled dataset, compatible of the Spatio-Temporal Asset Catalog Tiled Assets specification, using Cloud-Optimized GeoTIFF metatiles of dimension 4096x4096.

   .. code-block:: bash

      gdal raster tile --format COG --tile-size 4096 input.tif output_folder

.. example::
   :title: Mosaic on-the-fly several input files and tile that mosaic.

   .. code-block:: bash

      gdal raster pipeline ! mosaic input*.tif ! tile output_folder

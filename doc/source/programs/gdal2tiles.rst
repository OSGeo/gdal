.. _gdal2tiles:

================================================================================
gdal2tiles.py
================================================================================

.. only:: html

    Generates directory with TMS tiles, KMLs and simple web viewers.

.. Index:: gdal2tiles

Synopsis
--------

.. code-block::


    gdal2tiles.py [--help] [--help-general]
                  [-p <profile>] [-r resampling] [-s <srs>] [-z <zoom>]
                  [-e] [-a nodata] [-v] [-q] [-h] [-k] [-n] [-u <url>]
                  [-w <webviewer>] [-t <title>] [-c <copyright>]
                  [--processes=<NB_PROCESSES>] [--mpi] [--xyz]
                  [--tilesize=<PIXELS>] --tiledriver=<DRIVER> [--tmscompatible]
                  [--excluded-values=<EXCLUDED_VALUES>]
                  [--excluded-values-pct-threshold=<EXCLUDED_VALUES_PCT_THRESHOLD>]
                  [--nodata-values-pct-threshold=<NODATA_VALUES_PCT_THRESHOLD>]
                  [-g <googlekey] [-b <bingkey>] <input_file> [<output_dir>] [<COMMON_OPTIONS>]

Description
-----------

This utility generates a directory with small tiles and metadata, following
the OSGeo Tile Map Service Specification. Simple web pages with viewers based on
Google Maps, OpenLayers and Leaflet are generated as well - so anybody can comfortably
explore your maps on-line and you do not need to install or configure any
special software (like MapServer) and the map displays very fast in the
web browser. You only need to upload the generated directory onto a web server.

GDAL2Tiles also creates the necessary metadata for Google Earth (KML
SuperOverlay), in case the supplied map uses EPSG:4326 projection.

World files and embedded georeferencing is used during tile generation, but you
can publish a picture without proper georeferencing too.

.. note::

    Inputs with non-Byte data type (i.e. ``Int16``, ``UInt16``,...) will be clamped to
    the ``Byte`` data type, causing wrong results. To avoid this it is necessary to
    rescale input to the ``Byte`` data type using `gdal_translate` utility.

.. note::

    Config options of the input drivers may have an effect on the output of gdal2tiles. An example driver config option is GDAL_PDF_DPI, which can be found at :ref:`configoptions`


.. program:: gdal2tiles

.. include:: options/help_and_help_general.rst

.. option:: -p <PROFILE>, --profile=<PROFILE>

  Tile cutting profile (mercator, geodetic, raster) - default 'mercator' (Google Maps compatible).

  Starting with GDAL 3.2, additional profiles are available from tms_XXXX.json files
  placed in GDAL data directory (provided all zoom levels use same origin, tile dimensions,
  and resolution between consecutive zoom levels vary by a factor of two).

.. option:: -r <RESAMPLING>, --resampling=<RESAMPLING>

  Resampling method (average, near, bilinear, cubic, cubicspline, lanczos, antialias, mode, max, min, med, q1, q3) - default 'average'.

.. option:: -s <SRS>, --s_srs=<SRS>

  The spatial reference system used for the source input data.

.. option:: --xyz

  Generate XYZ tiles (OSM Slippy Map standard) instead of TMS.
  In the default mode (TMS), tiles at y=0 are the southern-most tiles, whereas
  in XYZ mode (used by OGC WMTS too), tiles at y=0 are the northern-most tiles.

  .. versionadded:: 3.1

.. option:: -d, --tmscompatible

  When using the geodetic profile, specifies the base resolution as 0.703125
  or 2 tiles at zoom level 0.

.. option:: -z <ZOOM>, --zoom=<ZOOM>

  Zoom levels to render (format:'2-5', '10-' or '10').

.. option:: -e, --resume

  Resume mode. Generate only missing files.

.. option:: -a <NODATA>, --srcnodata=<NODATA>

  Value in the input dataset considered as transparent. If the input dataset
  had already an associate nodata value, it is overridden by the specified value.

.. option:: -v, --verbose

  Generate verbose output of tile generation.

  Starting with GDAL 3.7, that verbose output is emitted through the
  ``logging.getLogger("gdal2tiles")`` object.

.. option:: -x, --exclude

  Exclude transparent tiles from result tileset.

.. option:: -q, --quiet

  Disable messages and status to stdout

  .. versionadded:: 2.1

.. option:: --processes=<NB_PROCESSES>

  Number of parallel processes to use for tiling, to speed-up the computation.

  .. versionadded:: 2.3

.. option:: --mpi

  Assume launched by mpiexec, enable MPI parallelism and ignore --processes.
  Requires working MPI environment and the MPI for Python (mpi4py) package.
  User should set GDAL_CACHEMAX to an appropriate cache size per process
  based on memory per node and the number of processes launched per node.

  .. versionadded:: 3.5

.. option:: --tilesize=<PIXELS>

  Width and height in pixel of a tile. Default is 256.

  .. versionadded:: 3.1

.. option:: --tiledriver=<DRIVER>

  Which output driver to use for the tiles, determines the file format of the tiles.
  Currently PNG, WEBP and JPEG (JPEG added in GDAL 3.9) are supported. Default is PNG.
  Additional configuration for the WEBP and JPEG drivers are documented below.

  .. versionadded:: 3.6

.. option:: --excluded-values=<EXCLUDED_VALUES>

  Comma-separated tuple of values (thus typically "R,G,B"), that are ignored
  as contributing source * pixels during resampling. The number of values in
  the tuple must be the same as the number of bands, excluding the alpha band.
  Several tuples of excluded values may be specified using the "(R1,G1,B2),(R2,G2,B2)" syntax.
  Only taken into account by Average currently.
  This concept is a bit similar to nodata/alpha, but the main difference is
  that pixels matching one of the excluded value tuples are still considered
  as valid, when determining the target pixel validity/density.

  .. versionadded:: 3.9

.. option:: --excluded-values-pct-threshold=EXCLUDED_VALUES_PCT_THRESHOLD

  Minimum percentage of source pixels that must be set at one of the --excluded-values to cause the excluded
  value, that is in majority among source pixels, to be used as the target pixel value. Default value is 50(%)

  .. versionadded:: 3.9

.. option:: --nodata-values-pct-threshold=<NODATA_VALUES_PCT_THRESHOLD>

  Minimum percentage of source pixels that must be at nodata (or alpha=0 or any
  other way to express transparent pixel) to cause the target pixel value to
  be transparent. Default value is 100 (%), which means that a target pixel is
  transparent only if all contributing source pixels are transparent.
  Only taken into account for average resampling.

  .. versionadded:: 3.9

.. option:: -h, --help

  Show help message and exit.

.. option:: --version

  Show program's version number and exit.


KML (Google Earth) options
++++++++++++++++++++++++++

Options for generated Google Earth SuperOverlay metadata

.. option:: -k, --force-kml

  Generate KML for Google Earth - default for 'geodetic' profile and 'raster' in EPSG:4326. For a dataset with different projection use with caution!

.. option:: -n, --no-kml

  Avoid automatic generation of KML files for EPSG:4326.

.. option:: -u <URL>, --url=<URL>

  URL address where the generated tiles are going to be published.


Web viewer options
++++++++++++++++++

Options for generated HTML viewers a la Google Maps

.. option:: -w <WEBVIEWER>, --webviewer=<WEBVIEWER>

  Web viewer to generate (all, google, openlayers, leaflet, mapml, none) - default 'all'.

.. option:: -t <TITLE>, --title=<TITLE>

  Title of the map.

.. option:: -c <COPYRIGHT>, --copyright=<COPYRIGHT>

  Copyright for the map.

.. option:: -g <GOOGLEKEY>, --googlekey=<GOOGLEKEY>

  Google Maps API key from http://code.google.com/apis/maps/signup.html.

.. option:: -b <BINGKEY>, --bingkey=<BINGKEY>

  Bing Maps API key from https://www.bingmapsportal.com/


.. note::

    gdal2tiles.py is a Python script that needs to be run against Python GDAL binding.

MapML options
+++++++++++++

MapML support is new to GDAL 3.2. When --webviewer=mapml is specified,
--xyz is implied, as well as --tmscompatible if --profile=geodetic.

The following profiles are supported:

- mercator: mapped to OSMTILE MapML tiling scheme
- geodetic: mapped to WGS84 MapML tiling scheme
- APSTILE: from the tms_MapML_APSTILE.json data file

The generated MapML file in the output directory is ``mapml.mapl``

Available options are:

.. option:: --mapml-template=<filename>

    Filename of a template mapml file where variables will
    be substituted. If not specified, the generic
    template_tiles.mapml file from GDAL data resources
    will be used

The --url option is also used to substitute ``${URL}`` in the template MapML file.

WEBP options
+++++++++++++

WEBP tiledriver support is new to GDAL 3.6. It is enabled by using --tiledriver=WEBP.


The following configuration options are available to further customize the webp output:

.. option:: --webp-quality=<QUALITY>

    QUALITY is a integer between 1-100. Default is 75.

.. option:: --webp-lossless

    Use WEBP lossless compression, default is lossy


.. note::

    GDAL :ref:`WEBP driver <raster.webp>` documentation can be consulted


JPEG options
+++++++++++++

JPEG tiledriver support is new to GDAL 3.9. It is enabled by using --tiledriver=JPEG.

Note that JPEG does not support transparency, hence edge tiles will display black
pixels in areas not covered by the source raster.

The following configuration options are available to further customize the webp output:

.. option:: ---jpeg-quality=JPEG_QUALITY

    QUALITY is a integer between 1-100. Default is 75.


Examples
--------

Basic example:

.. code-block::

  gdal2tiles.py --zoom=2-5 input.tif output_folder


MapML generation:

.. code-block::

  gdal2tiles.py --zoom=16-18 -w mapml -p APSTILE --url "https://example.com" input.tif output_folder


MPI example:

.. code-block::

  mpiexec -n $NB_PROCESSES gdal2tiles.py --mpi --config GDAL_CACHEMAX 500 --zoom=2-5 input.tif output_folder

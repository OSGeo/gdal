.. _gdal2tiles:

================================================================================
gdal2tiles
================================================================================

.. only:: html

    Generates directory with TMS tiles, KMLs and simple web viewers.

.. Index:: gdal2tiles

Synopsis
--------

.. code-block::


    gdal2tiles.py [-p profile] [-r resampling] [-s srs] [-z zoom]
                  [-e] [-a nodata] [-v] [-q] [-h] [-k] [-n] [-u url]
                  [-w webviewer] [-t title] [-c copyright]
                  [--processes=NB_PROCESSES] [--xyz]
                  --tilesize=PIXELS
                  [-g googlekey] [-b bingkey] input_file [output_dir]

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
    the ``Byte`` data type, causing wrong results. To awoid this it is necessary to
    rescale input to the ``Byte`` data type using `gdal_translate` utility.


.. program:: gdal_translate

.. option:: -p <PROFILE>, --profile=<PROFILE>

  Tile cutting profile (mercator, geodetic, raster) - default 'mercator' (Google Maps compatible).

.. option:: -r <RESAMPLING>, --resampling=<RESAMPLING>

  Resampling method (average, near, bilinear, cubic, cubicspline, lanczos, antialias, mode, max, min, med, q1, q3) - default 'average'.

.. option:: -s <SRS>, --s_srs=<SRS>

  The spatial reference system used for the source input data.

.. option:: --xyz

  Generate XYZ tiles (OSM Slippy Map standard) instead of TMS.
  Only for mercator profile.
  In the default mode (TMS), tiles at y=0 are the southern-most tiles, whereas
  in XYZ mode (used by OGC WMTS too), tiles at y=0 are the northern-most tiles.

  .. versionadded:: 3.1

.. option:: -z <ZOOM>, --zoom=<ZOOM>

  Zoom levels to render (format:'2-5' or '10').

.. option:: -e, --resume

  Resume mode. Generate only missing files.

.. option:: -a <NODATA>, --srcnodata=<NODATA>

  Value in the input dataset considered as transparent. If the input dataset
  had already an associate nodata value, it is overriden by the specified value.

.. option:: -v, --verbose

  Generate verbose output of tile generation.

.. option:: -q, --quiet

  Disable messages and status to stdout

  .. versionadded:: 2.1

.. option:: --processes=<NB_PROCESSES>

  Number of parallel processes to use for tiling, to speed-up the computation.

  .. versionadded:: 2.3

.. option:: --tilesize=<PIXELS>

  Width and height in pixel of a tile. Default is 256.

  .. versionadded:: 3.1

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

  Web viewer to generate (all, google, openlayers, leaflet, none) - default 'all'.

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


Examples
--------

Basic example:

.. code-block::

  gdal2tiles.py --zoom=2-5 input.tif output_folder

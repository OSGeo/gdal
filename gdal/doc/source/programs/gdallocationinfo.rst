.. _gdallocationinfo:

================================================================================
gdallocationinfo
================================================================================

.. only:: html

    Raster query tool

.. Index:: gdallocationinfo

Synopsis
--------

.. code-block::

    Usage: gdallocationinfo [--help-general] [-xml] [-lifonly] [-valonly]
                            [-b band]* [-overview overview_level]
                            [-l_srs srs_def] [-geoloc] [-wgs84]
                            [-oo NAME=VALUE]* srcfile [x y]

Description
-----------

The :program:`gdallocationinfo` utility provide a mechanism to query information about
a pixel given its location in one of a variety of coordinate systems.  Several
reporting options are provided.

.. program:: gdallocationinfo

.. option:: -xml

    The output report will be XML formatted for convenient post processing.

.. option:: -lifonly

    The only output is filenames production from the LocationInfo request
    against the database (i.e. for identifying impacted file from VRT).

.. option:: -valonly

    The only output is the pixel values of the selected pixel on each of
    the selected bands.

.. option:: -b <band>

    Selects a band to query.  Multiple bands can be listed.  By default all
    bands are queried.

.. option:: -overview <overview_level>

    Query the (overview_level)th overview (overview_level=1 is the 1st overview),
    instead of the base band. Note that the x,y location (if the coordinate system is
    pixel/line) must still be given with respect to the base band.

.. option:: -l_srs <srs_def>

    The coordinate system of the input x, y location.

.. option:: -geoloc

    Indicates input x,y points are in the georeferencing system of the image.

.. option:: -wgs84

    Indicates input x,y points are WGS84 long, lat.

.. option:: -oo NAME=VALUE

    Dataset open option (format specific)

.. option:: <srcfile>

    The source GDAL raster datasource name.

.. option:: <x>

    X location of target pixel.  By default the
    coordinate system is pixel/line unless -l_srs, -wgs84 or -geoloc supplied. 

.. option:: <y>

    Y location of target pixel.  By default the
    coordinate system is pixel/line unless -l_srs, -wgs84 or -geoloc supplied. 


This utility is intended to provide a variety of information about a
pixel.  Currently it reports:

- The location of the pixel in pixel/line space.
- The result of a LocationInfo metadata query against the datasource.
  This is implement for VRT files which will report the
  file(s) used to satisfy requests for that pixel, and by the
  :ref:`raster.mbtiles` driver
- The raster pixel value of that pixel for all or a subset of the bands.
- The unscaled pixel value if a Scale and/or Offset apply to the band.

The pixel selected is requested by x/y coordinate on the command line, or read
from stdin. More than one coordinate pair can be supplied when reading
coordinates from stdin. By default pixel/line coordinates are expected.
However with use of the :option:`-geoloc`, :option:`-wgs84`, or :option:`-l_srs` switches it is possible
to specify the location in other coordinate systems.

The default report is in a human readable text format.  It is possible to
instead request xml output with the -xml switch.

For scripting purposes, the -valonly and -lifonly switches are provided to
restrict output to the actual pixel values, or the LocationInfo files
identified for the pixel.

It is anticipated that additional reporting capabilities will be added to
gdallocationinfo in the future.

Examples
--------

Simple example reporting on pixel (256,256) on the file utm.tif.

::

    $ gdallocationinfo utm.tif 256 256
    Report:
    Location: (256P,256L)
    Band 1:
        Value: 115

Query a VRT file providing the location in WGS84, and getting the result in xml.

::

    $ gdallocationinfo -xml -wgs84 utm.vrt -117.5 33.75
    <Report pixel="217" line="282">
        <BandReport band="1">
            <LocationInfo>
            <File>utm.tif</File>
            </LocationInfo>
            <Value>16</Value>
        </BandReport>
    </Report>

Reading location from stdin.

::

    $ cat coordinates.txt
    443020 3748359
    441197 3749005
    443852 3747743
    
    $ cat coordinates.txt | gdallocationinfo -geoloc utmsmall.tif
    Report:
      Location: (38P,49L)
      Band 1:
        Value: 214
    Report:
      Location: (7P,38L)
      Band 1:
        Value: 107
    Report:
      Location: (52P,59L)
      Band 1:
        Value: 148

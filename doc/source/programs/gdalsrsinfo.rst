.. _gdalsrsinfo:

================================================================================
gdalsrsinfo
================================================================================

.. only:: html

    Lists info about a given SRS in number of formats (WKT, PROJ.4, etc.)

.. Index:: gdalsrsinfo

Synopsis
--------

.. code-block::

    Usage: gdalsrsinfo [--help] [--help-general]
                       [--single-line] [-V] [-e][-o <out_type>] <srs_def>

Description
-----------

The :program:`gdalsrsinfo` utility reports information about a given SRS from one of the following:

- The filename of a dataset supported by GDAL/OGR which contains SRS information
- Any of the usual GDAL/OGR forms (complete WKT, PROJ.4, EPSG:n or a file containing the SRS)


.. program:: gdalsrsinfo

.. include:: options/help_and_help_general.rst

.. option:: --single-line

    Print WKT on single line

.. option:: -V

    Validate SRS

.. option:: -e

    Search for EPSG number(s) corresponding to SRS

.. option:: -o <out_type>

    Output types:

    - ``default``: proj4 and wkt (default option)
    - ``all``: all options available
    - ``wkt_all``: all wkt options available
    - ``PROJJSON``: PROJJSON string (GDAL >= 3.1 and PROJ >= 6.2)
    - ``proj4``: PROJ.4 string
    - ``wkt1``: OGC WKT format (full)
    - ``wkt_simple``: OGC WKT 1 (simplified)
    - ``wkt_noct``: OGC WKT 1 (without OGC CT params)
    - ``wkt_esri``: ESRI WKT format
    - ``wkt``: Latest WKT version supported, currently wkt2_2019
    - ``wkt2``: Latest WKT2 version supported, currently wkt2_2019
    - ``wkt2_2015``: OGC WKT2:2015
    - ``wkt2_2019``: OGC WKT2:2019 (for GDAL < 3.6, use ``wkt2_2018``)
    - ``mapinfo``: Mapinfo style CoordSys format
    - ``xml``: XML format (GML based)

.. option:: <srs_def>

    may be the filename of a dataset supported by GDAL/OGR from which to extract SRS information
    OR any of the usual GDAL/OGR forms (complete WKT, PROJ.4, EPSG:n or a file containing the SRS)

Example
-------

.. example:: Default output

   .. command-output:: gdalsrsinfo EPSG:4326

.. example::
   :title: PROJ.4 output

   .. command-output:: gdalsrsinfo -o proj4 lcc_esri.prj
      :cwd: ../../../autotest/osr/data


   .. code-block:: console

    $ gdalsrsinfo -o proj4 landsat.tif
    PROJ.4 : '+proj=utm +zone=19 +south +datum=WGS84 +units=m +no_defs '

.. example::
   :title: WKT output, latest version

   .. command-output:: gdalsrsinfo  -o wkt "EPSG:32722"

.. example::
   :title: WKT output, all versions

   .. command-output:: gdalsrsinfo -o wkt_all "EPSG:4322"

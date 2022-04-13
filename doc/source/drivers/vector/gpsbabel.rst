.. _vector.gpsbabel:

GPSBabel
========

.. shortname:: GPSBabel

.. build_dependencies:: (read support needs GPX driver and libexpat) 

The GPSBabel driver for now that relies on the
`GPSBabel <http://www.gpsbabel.org>`__ utility to access various GPS
file formats.

The GPSBabel executable must be accessible through the PATH.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

Read support
------------

The driver needs the :ref:`GPX <vector.gpx>` driver to be fully
configured with read support (through Expat library) to be able to parse
the output of GPSBabel, as GPX is used as the intermediate pivot format.

The returned layers can be waypoints, routes, route_points, tracks,
track_points depending on the input data.

The syntax to specify an input datasource is :
*GPSBabel:gpsbabel_file_format[,gpsbabel_format_option]*:[features=[waypoints,][tracks,][routes]:]filename*
where :

-  *gpsbabel_file_format* is one of the `file
   formats <http://www.gpsbabel.org/capabilities.shtml>`__ handled by
   GPSBabel.
-  *gpsbabel_format_option* is any option handled by the specified
   GPSBabel format (refer to the documentation of each GPSBabel format)
-  *features=* can be used to modify the type of features that GPSBabel
   will import. waypoints matches the -w option of gpsbabel
   commandline, tracks matches -t and routes matches -r. This option
   can be used to require full data import from GPS receivers that are
   slow and for which GPSBabel would only fetch waypoints by default.
   See the documentation on `Route and Track
   modes <http://www.gpsbabel.org/htmldoc-1.3.6/Route_And_Track_Modes.html>`__
   for more details.
-  *filename* can be an actual on-disk file, a file handled through the
   GDAL virtual file API, or a special device handled by GPSBabel such
   as "usb:", "/dev/ttyS0", "COM1:", etc.. What is actually supported
   depends on the used GPSBabel format.

Alternatively, for a few selected GPSBabel formats, just specifying the
filename might be sufficient. The list includes for now :

-  garmin_txt
-  gtrnctr
-  gdb
-  magellan
-  mapsend
-  mapsource
-  nmea
-  osm
-  ozi
-  igc

The :decl_configoption:`USE_TEMPFILE` =YES configuration option can be used to create an
on-disk temporary GPX file instead of a in-memory one, when reading big
amount of data.

Write support
-------------

The driver relies on the GPX driver to create an intermediate file that
will be finally translated by GPSBabel to the desired GPSBabel format.
(The GPX driver does not need to be configured for read support for
GPSBabel write support.).

The support geometries, options and other creation issues are the ones
of the GPX driver. Please refer to its :ref:`documentation <vector.gpx>`
for more details.

The syntax to specify an output datasource is :
*GPSBabel:gpsbabel_file_format[,gpsbabel_format_option]*:filename* where
:

-  *gpsbabel_file_format* is one of the `file
   formats <http://www.gpsbabel.org/capabilities.shtml>`__ handled by
   GPSBabel.
-  *gpsbabel_format_option* is any option handled by the specified
   GPSBabel format (refer to the documentation of each GPSBabel format)

Alternatively, you can just pass a filename as output datasource name
and specify the dataset creation option
GPSBABEL_DRIVER=gpsbabel_file_format[,gpsbabel_format_option]\*

The :decl_configoption:`USE_TEMPFILE` =YES configuration option can be used to create an
on-disk temporary GPX file instead of a in-memory one, when writing big
amount of data.

Examples
~~~~~~~~

Reading the waypoints from a Garmin USB receiver :

::

   ogrinfo -ro -al GPSBabel:garmin:usb:

Converting a shapefile to Magellan Mapsend format :

::

   ogr2ogr -f GPSBabel GPSBabel:mapsend:out.mapsend in.shp

See Also
~~~~~~~~

-  `GPSBabel Home Page <http://www.gpsbabel.org>`__
-  `GPSBabel file
   formats <http://www.gpsbabel.org/capabilities.shtml>`__
-  :ref:`GPX driver page <vector.gpx>`

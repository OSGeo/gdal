.. _vector.gtm:

GTM - GPS TrackMaker
====================

.. shortname:: GTM

.. built_in_by_default::

.. deprecated_driver:: version_targeted_for_removal: 3.5
   env_variable: GDAL_ENABLE_DEPRECATED_DRIVER_GTM

`GPSTrackMaker <http://www.gpstm.com/>`__ is a program that is
compatible with more than 160 GPS models. It allows you to create your
own maps. It supports vector maps and images.

The OGR driver has support for reading and writing GTM 211 files (.gtm);
however, in this implementation we are not supporting images and routes.
Waypoints and tracks are supported.

Although GTM has support for many data, like NAD 1967, SAD 1969, and
others, the output file of the OGR driver will be using WGS 1984. And
the GTM driver will only read properly GTM files georeferenced as WGS
1984 (if not the case a warning will be issued).

The OGR driver supports just POINT, LINESTRING, and MULTILINESTRING.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Example
-------

The ogrinfo utility can be used to dump the content of a GTM datafile :

::

   ogrinfo -ro -al input.gtm

| 

Use of -sql option to remap field names to the ones allowed by the GTM
schema:

::

   ogr2ogr -f "GPSTrackMaker" output.gtm input.shp -sql "SELECT field1 AS name, field2 AS color, field3 AS type FROM input"

| 

Example for translation from PostGIS to GTM:

::

   ogr2ogr -f "GPSTrackMaker" output.gtm PG:"host=hostaddress user=username dbname=db password=mypassword" -sql "select filed1 as name, field2 as color, field3 as type, wkb_geometry from input" -nlt MULTILINESTRING

| 
| Note : You need to specify the layer type as POINT, LINESTRING, or
  MULTILINESTRING.

See Also
--------

-  `Home page for GPS TrackMaker Program <http://www.gpstm.com/>`__
-  `GTM 211 format
   documentation <http://www.gpstm.com/download/GTM211_format.pdf>`__

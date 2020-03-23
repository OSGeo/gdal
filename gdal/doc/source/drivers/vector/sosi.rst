.. _vector.sosi:

================================================================================
Norwegian SOSI Standard
================================================================================

.. shortname:: SOSI

.. build_dependencies:: FYBA library

This driver requires the FYBA library.

Open options
------------

Starting with GDAL 3.1, the following open options can be specified
(typically with the -oo name=value parameters of ogrinfo or ogr2ogr):

-  appendFieldsMap\ =(defaults is empty). 'Default is that all rows for equal field names will be appended in a feature, but with this parameter you select what field this should be valid for.'


Examples
~~~~~~~~

-  This example will convert a sosi file to a shape a file where all duplicate fields in a feature will be appended with a comma between. 

   ::

      ogr2ogr -t_srs EPSG:4258 test_poly.shp test_duplicate_fields.sos polygons

-  This example will convert a sosi file to a shape a file where only duplicates for BEITEBRUKERID and OPPHAV will appended with a comma between. 

   ::

      ogr2ogr -t_srs EPSG:4258  test_poly.shp test_duplicate_fields.sos polygons -oo appendFieldsMap="BEITEBRUKERID&OPPHAV"

-  This example will convert a sosi file to a shape a file where for BEITEBRUKERID and OPPHAV will be appended with a semicolon and comma between 

   ::

      ogr2ogr -t_srs EPSG:4258  test_poly.shp test_duplicate_fields.sos polygons -oo appendFieldsMap="BEITEBRUKERID:;&OPPHAV:,"

   

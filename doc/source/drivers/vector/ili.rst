.. _vector.ili:

"INTERLIS 1" and "INTERLIS 2" drivers
=====================================

.. shortname:: INTERLIS 1

.. shortname:: INTERLIS 2

.. build_dependencies:: Xerces

| OGR has support for INTERLIS reading and writing.
| `INTERLIS <http://www.interlis.ch/>`__ is a standard which has been
  especially composed in order to fulfill the requirements of modeling
  and the integration of geodata into contemporary and future geographic
  information systems. With the usage of unified, documented geodata and
  the flexible exchange possibilities the following advantage may occur:

-  the standardized documentation
-  the compatible data exchange
-  the comprehensive integration of geodata e.g. from different data
   owners.
-  the quality proofing
-  the long term data storage
-  the contract-proof security and the availability of the software

OGR supports INTERLIS 1 and INTERLIS 2 (2.2 and 2.3) with the following
limitations:

-  Curves in Interlis 1 area polygons are converted to line segments
-  Interlis 1 Surface geometries with non-numeric IDENT field are not
   included in the attribute layer
-  Embedded INTERLIS 2 structures and line attributes are not supported
-  Incremental transfer is not supported
-  Transfer id (TID) is used as feature id

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Model support
-------------

Data is read and written into transfer files which have different
formats in INTERLIS 1 (.itf) and INTERLIS 2 (.xtf). Models are passed in
IlisMeta format by using "a_filename.xtf,models.imd" as a connection
string.

IlisMeta files can be be generated with the ili2c compiler. Command line
example:

::

   java -jar ili2c.jar --ilidirs '%ILI_DIR;http://models.interlis.ch/;%JAR_DIR' -oIMD --out models.imd model1.ili [model2.ili ...]

Some possible transformations using :ref:`ogr2ogr`.

-  Interlis 1 -> Shape:

   ::

      ogr2ogr -f "ESRI Shapefile" shpdir ili-bsp.itf,Beispiel.imd

-  Interlis 2 -> Shape:

   ::

      ogr2ogr -f "ESRI Shapefile" shpdir RoadsExdm2ien.xml,RoadsExdm2ien.imd

   or without model:

   ::

      ogr2ogr -f "ESRI Shapefile" shpdir RoadsExdm2ien.xml

   Example with curves and multiple geometries:

   ::

      ogr2ogr --config OGR_STROKE_CURVE TRUE -SQL 'SELECT Rechtsstatus,publiziertAb,MetadatenGeobasisdaten,Eigentumsbeschraenkung,ZustaendigeStelle,Flaeche FROM "OeREBKRM09trsfr.Transferstruktur.Geometrie"' shpdir ch.bazl.sicherheitszonenplan.oereb_20131118.xtf,OeREBKRM09vs.imd OeREBKRM09trsfr.Transferstruktur.Geometrie

-  Shape -> Interlis 2:

   ::

      ogr2ogr -f "Interlis 2" LandCover.xml,RoadsExdm2ien.imd RoadsExdm2ben.Roads.LandCover.shp

-  Importing multiple Interlis 1 files into PostGIS:

   ::

      ogr2ogr -f PostgreSQL PG:dbname=warmerda av_fixpunkte_ohne_LFPNachfuehrung.itf,av.imd -lco OVERWRITE=yes
      ogr2ogr -f PostgreSQL PG:dbname=warmerda av_fixpunkte_mit_LFPNachfuehrung.itf,av.imd -append

Arc interpolation
~~~~~~~~~~~~~~~~~

| Converting INTERLIS arc geometries to line segments can be forced by
  setting the configuration variable :decl_configoption:`OGR_STROKE_CURVE` to TRUE.
| The approximation of arcs as linestrings is done by splitting the arcs
  into subarcs of no more than a threshold angle. This angle is the
  :decl_configoption:`OGR_ARC_STEPSIZE`. This defaults to one degree, but may be overridden
  by setting the configuration variable :decl_configoption:`OGR_ARC_STEPSIZE`.

Other Notes
-----------

-  `ogrtools <https://github.com/sourcepole/ogrtools>`__ library
   includes extensions for the OGR Interlis driver
-  Development of the OGR INTERLIS driver was supported by `Swiss
   Federal Administration <http://www.kogis.ch/>`__, `Canton
   Solothurn <http://www.sogis.ch/>`__ and `Canton
   Thurgovia <http://www.geoinformation.tg.ch/>`__.

.. _vector.ili:

"INTERLIS 1" and "INTERLIS 2" drivers
=====================================

.. shortname:: INTERLIS 1

.. shortname:: INTERLIS 2

.. build_dependencies:: Xerces

| OGR has support for INTERLIS reading.
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

OGR supports INTERLIS 1 and INTERLIS 2 (2.2 and newer) with the following
limitations:

-  Curves in Interlis 1 area polygons are converted to line segments
-  Interlis 1 Surface geometries with non-numeric IDENT field are not
   included in the attribute layer
-  Embedded INTERLIS 2 structures and line attributes are not supported
-  Incremental transfer is not supported
-  Transfer id (TID) is used as feature id

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Dataset open options
--------------------

|about-open-options|
The following open options are available:

-  .. oo:: MODEL

      Path to IlisMeta model file.

Model support
-------------

Data is read from transfer files which have different
formats in INTERLIS 1 (.itf) and INTERLIS 2 (.xtf). Models are passed in
IlisMeta format with the open option ``MODEL`` or by using "a_filename.xtf,models.imd" as a connection
string.

IlisMeta files can be be generated with the ili2c compiler. Command line
example:

::

   java -jar ili2c.jar --ilidirs '%ILI_DIR;http://models.interlis.ch/;%JAR_DIR' -oIMD --out models.imd model1.ili [model2.ili ...]

or for newer models:

::

   java -jar ili2c.jar --ilidirs '%ILI_DIR;http://models.interlis.ch/;%JAR_DIR' -oIMD16 --out models.imd model1.ili [model2.ili ...]

Some possible transformations using :ref:`ogr2ogr`.

-  Interlis 1 -> Shape:

   ::

      gdal vector convert --oo MODEL=Beispiel.imd -f "ESRI Shapefile" ili-bsp.itf shpdir

-  Interlis 2 -> GeoPackage:

   ::

      gdal vector convert --oo MODEL=KGKCGC_FPDS2_V1_1.imd fpds2_v1_1.xtf fpds2_v1_1.gpkg

   or without model:

   ::

      gdal vector convert -f "ESRI Shapefile" RoadsExdm2ien.xml shpdir

   Example with curves and multiple geometries:

   ::

      gdal vector sql --oo MODEL=OeREBKRM09vs.imd --config OGR_STROKE_CURVE=TRUE --sql 'SELECT Rechtsstatus,publiziertAb,MetadatenGeobasisdaten,Eigentumsbeschraenkung,ZustaendigeStelle,Flaeche FROM "OeREBKRM09trsfr.Transferstruktur.Geometrie"' -f "ESRI Shapefile" ch.bazl.sicherheitszonenplan.oereb_20131118.xtf shpdir

-  Importing multiple Interlis 1 files into PostGIS:

   ::

      gdal vector convert --oo MODEL=av.imd --overwrite-layer av_fixpunkte_ohne_LFPNachfuehrung.itf PG:dbname=warmerda
      gdal vector convert --oo MODEL=av.imd --append av_fixpunkte_mit_LFPNachfuehrung.itf PG:dbname=warmerda

Arc interpolation
~~~~~~~~~~~~~~~~~

| Converting INTERLIS arc geometries to line segments can be forced by
  setting the configuration variable :config:`OGR_STROKE_CURVE` to TRUE.
| The approximation of arcs as linestrings is done by splitting the arcs
  into subarcs of no more than a threshold angle. This angle is the
  :config:`OGR_ARC_STEPSIZE`. This defaults to one degree, but may be overridden
  by setting the configuration variable :config:`OGR_ARC_STEPSIZE`.

Other Notes
-----------

-  Development of the OGR INTERLIS driver was supported by `Swiss
   Federal Administration <http://www.kogis.ch/>`__, `Canton
   Solothurn <http://www.sogis.ch/>`__ and `Canton
   Thurgovia <http://www.geoinformation.tg.ch/>`__.

.. _vector.oapif:

OGC API - Features
==================

.. versionadded:: 2.3

.. shortname:: OAPIF

.. build_dependencies:: libcurl

This driver can connect to a OGC API - Features service. It assumes that the
service supports OpenAPI 3.0/JSON/GeoJSON encoding for respectively API
description, feature collection metadata and feature collection data.

.. note::

    In versions prior to GDAL 3.1, this driver was called the WFS3 driver, and
    only supported draft versions of the specification.

Driver capabilities
-------------------

.. supports_georeferencing::

Dataset name syntax
-------------------

The syntax to open a OGC API - Features datasource is :
*OAPIF:http://path/to/OAPIF/endpoint*

where endpoint is the landing page or a the path to collections/{id}.

Layer schema
------------

OGR needs a fixed schema per layer, but OGC API - Features Core doesn't impose
fixed schema. So the driver will retrieve the first page of features (10
features) and establish a schema from this.

Filtering
---------

The driver will forward any spatial filter set with SetSpatialFilter()
to the server. In OGC API - Features Core, only a subset of attributes allowed by
the server can be queried for equalities, potentially combined with a
AND logical operator. More complex requests will be partly or completely
evaluated on client-side.

Rectangular spatial filtering is forward to the server as well.

Open options
------------

The following options are available:

-  **URL**\ =url: URL to the OGC API - Features server landing page, or to a given collection.
   Required when using the "OAPIF:" string as the connection string.
-  **PAGE_SIZE**\ =integer: Number of features to retrieve per request.
   Defaults to 10. Minimum is 1, maximum 10000.
-  **USERPWD**: May be supplied with *userid:password* to pass a userid
   and password to the remote server.
-  **IGNORE_SCHEMA**\ = YES/NO. (GDAL >= 3.1) Set to YES to ignore the XML
   Schema or JSON schema that may be offered by the server.

Examples
--------

-  Listing the types of a OGC API - Features server :

   ::

      $ ogrinfo OAPIF:https://www.ldproxy.nrw.de/rest/services/kataster

      INFO: Open of `OAPIF:https://www.ldproxy.nrw.de/rest/services/kataster'
            using driver `OAPIF' successful.
      1: flurstueck (Multi Polygon)
      2: gebaeudebauwerk (Multi Polygon)
      3: verwaltungseinheit (Multi Polygon)

-  Listing the summary information of a OGC API - Features layer :

   ::

      $ ogrinfo -al -so OAPIF:https://www.ldproxy.nrw.de/rest/services/kataster flurstueck

      Layer name: flurstueck
      Metadata:
        TITLE=Flurstück
      Geometry: Multi Polygon
      Feature Count: 9308456
      Extent: (5.612726, 50.237351) - (9.589634, 52.528630)
      Layer SRS WKT:
      GEOGCS["WGS 84",
          DATUM["WGS_1984",
              SPHEROID["WGS 84",6378137,298.257223563,
                  AUTHORITY["EPSG","7030"]],
              AUTHORITY["EPSG","6326"]],
          PRIMEM["Greenwich",0,
              AUTHORITY["EPSG","8901"]],
          UNIT["degree",0.0174532925199433,
              AUTHORITY["EPSG","9122"]],
          AUTHORITY["EPSG","4326"]]
      id: String (0.0)
      aktualit: Date (0.0)
      flaeche: Real (0.0)
      flstkennz: String (0.0)
      land: String (0.0)
      gemarkung: String (0.0)
      flur: String (0.0)
      flurstnr: String (0.0)
      gmdschl: String (0.0)
      regbezirk: String (0.0)
      kreis: String (0.0)
      gemeinde: String (0.0)
      lagebeztxt: String (0.0)
      tntxt: String (0.0)

-  Filtering on a property (depending on if the server exposes filtering capabilities of the properties, part or totally of the filter might be evaluated on client side)

   ::


      $ ogrinfo OAPIF:https://www.ldproxy.nrw.de/rest/services/kataster flurstueck -al -q -where "flur = '028'"
      Layer name: flurstueck
      Metadata:
        TITLE=Flurstück
      OGRFeature(flurstueck):1
        id (String) = DENW19AL0000geMFFL
        aktualit (Date) = 2017/04/26
        flaeche (Real) = 1739
        flstkennz (String) = 05297001600193______
        land (String) = Nordrhein-Westfalen
        gemarkung (String) = Wünnenberg
        flur (String) = 016
        flurstnr (String) = 193
        gmdschl (String) = 05774040
        regbezirk (String) = Detmold
        kreis (String) = Paderborn
        gemeinde (String) = Bad Wünnenberg
        lagebeztxt (String) = Bleiwäscher Straße
        tntxt (String) = Platz / Parkplatz;1739
        MULTIPOLYGON (((8.71191 51.491084,8.7123 51.491067,8.712385 51.491645,8.712014 51.491666,8.711993 51.491603,8.71196 51.491396,8.711953 51.491352,8.71191 51.491084)))

      [...]

-  Spatial filtering

   ::

      $ ogrinfo OAPIF:https://www.ldproxy.nrw.de/rest/services/kataster flurstueck -al -q -spat 8.7 51.4 8.8 51.5

      Layer name: flurstueck
      Metadata:
        TITLE=Flurstück
      OGRFeature(flurstueck):1
        id (String) = DENW19AL0000ht7LFL
        aktualit (Date) = 2013/02/19
        flaeche (Real) = 18
        flstkennz (String) = 05292602900206______
        land (String) = Nordrhein-Westfalen
        gemarkung (String) = Fürstenberg
        flur (String) = 029
        flurstnr (String) = 206
        gmdschl (String) = 05774040
        regbezirk (String) = Detmold
        kreis (String) = Paderborn
        gemeinde (String) = Bad Wünnenberg
        lagebeztxt (String) = Karpke
        tntxt (String) = Fließgewässer / Bach;18
        MULTIPOLYGON (((8.768521 51.494915,8.768535 51.494882,8.768569 51.494908,8.768563 51.494925,8.768521 51.494915)))
      [...]

See Also
--------

-  `"OGC API - Features - Part 1: Core" Standard
   <http://docs.opengeospatial.org/is/17-069r3/17-069r3.html>`__
-  :ref:`WFS (1.0,1.1,2.0) driver documentation <vector.wfs>`

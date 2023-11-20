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
fixed schema.
The driver will use the XML schema or JSON schema pointed by the "describedby"
relationship of a collection, if it exists.
The driver will also retrieve the first page of features (using the
selected page) and establish a schema from this.


Filtering
---------

The driver will forward any spatial filter set with SetSpatialFilter()
to the server. In OGC API - Features Core, only a subset of attributes allowed by
the server can be queried for equalities, potentially combined with a
AND logical operator. More complex requests will be partly or completely
evaluated on client-side.

Rectangular spatial filtering is forward to the server as well.

CRS support
-----------

Starting with GDAL 3.7, the driver supports the
`OGC API - Features - Part 2: Coordinate Reference Systems by Reference <https://docs.ogc.org/is/18-058/18-058.html>`__
extension. If a server reports a storageCRS property, that property will be
used to set the CRS of the OGR layer. Otherwise the default will be OGC:CRS84
(WGS84 longitude, latitude).
As most all OGR drivers, the OAPIF driver will report the SRS and geometries,
and expect spatial filters, in the "GIS-friendly" order,
with longitude/easting first (X component), latitude/northing second (Y component),
potentially overriding the axis order of the authority.

The CRS of layers can also be controlled with the :oo:`CRS` or :oo:`PREFERRED_CRS` open
options documented below.

Open options
------------

The following options are available:

-  .. oo:: URL

      URL to the OGC API - Features server landing page, or to a given collection.
      Required when using the "OAPIF:" string as the connection string.

-  .. oo:: PAGE_SIZE
      :choices: <integer>
      :default: 1000

      Number of features to retrieve per request.
      Minimum is 1. If not set, an attempt to determine the maximum
      allowed size will be done by examining the API schema.

-  .. oo:: INITIAL_REQUEST_PAGE_SIZE
      :choices: <integer>
      :default: 20

      Number of features to retrieve during the initial request done
      in order to retrieve information about the features.
      Minimum is 1.
      Maximum is the value of the :oo:`PAGE_SIZE` option.
      If not set the default (20) will be used.

-  .. oo:: USERPWD

      May be supplied with *userid:password* to pass a userid
      and password to the remote server.

-  .. oo:: IGNORE_SCHEMA
      :choices: YES, NO
      :since: 3.1

       Set to YES to ignore the XML
       Schema or JSON schema that may be offered by the server.

-  .. oo:: CRS
      :since: 3.7

      Set to a CRS identifier, e.g ``EPSG:3067``
      or ``http://www.opengis.net/def/crs/EPSG/0/3067``, to use as the layer CRS.
      That CRS must be listed in the lists of CRS supported by the layers of the
      dataset, otherwise layers not listing it cannot be opened.

-  .. oo:: PREFERRED_CRS
      :since: 3.7

      Identical to the :oo:`CRS` option, except
      that if a layer does not list the PREFERRED_CRS in its list of supported CRS,
      the default CRS (storageCRS when present, otherwise EPSG:4326) will be used.
      :oo:`CRS` and :oo:`PREFERRED_CRS` option are mutually exclusive.

-  .. oo:: SERVER_FEATURE_AXIS_ORDER
      :choices: AUTHORITY_COMPLIANT, GIS_FRIENDLY
      :default: AUTHORITY_COMPLIANT

      This option can be set to GIS_FRIENDLY if axis order issue are noticed in
      features received from the server, indicating that the server does not return
      them in the axis order mandated by the CRS authority, but in a more traditional
      "GIS friendly" order, with longitude/easting first, latitude/northing second.
      Do not set this option unless actual problems arise.

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
-  `"OGC API - Features - Part 2: Coordinate Reference Systems by Reference" Standard
   <https://docs.ogc.org/is/18-058/18-058.html>`__
-  :ref:`WFS (1.0,1.1,2.0) driver documentation <vector.wfs>`

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

Starting with GDAL 3.10, specifying the ``-if OAPIF`` option to command line utilities
accepting it, or ``OAPIF`` as the only value of the ``papszAllowedDrivers`` of
:cpp:func:`GDALOpenEx`, also forces the driver to recognize the passed
URL without the ``OAPIF:`` prefix.

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

|about-open-options|
The following open options are available:

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

      $ ogrinfo OAPIF:https://ogc-api.nrw.de/inspire-us-feuerwehr

      INFO: Open of `OAPIF:https://ogc-api.nrw.de/inspire-us-feuerwehr'
            using driver `OAPIF' successful.
      1: governmentalservice (title: Feuerwehrleitstellen) (Point)

-  Listing the summary information of a OGC API - Features layer :

   ::

      $ ogrinfo OAPIF:https://ogc-api.nrw.de/inspire-us-feuerwehr governmentalservice -al -so

      INFO: Open of `OAPIF:https://ogc-api.nrw.de/inspire-us-feuerwehr'
            using driver `OAPIF' successful.

      Layer name: governmentalservice
      Metadata:
        DESCRIPTION=Staatliche Verwaltungs- und Sozialdienste wie öffentliche Verwaltung, Katastrophenschutz, Schulen und Krankenhäuser, die von öffentlichen oder privaten Einrichtungen erbracht werden, soweit sie in den Anwendungsbereich der Richtlinie 2007/2/EG fallen. Dieser Datensatz enthält Informationen zu Feuerwehrleitstellen.
        TITLE=Feuerwehrleitstellen
      Geometry: Point
      Feature Count: 52
      Extent: (6.020720, 50.654901) - (9.199363, 52.300806)
      Layer SRS WKT:
      GEOGCRS["WGS 84",
          DATUM["World Geodetic System 1984",
              ELLIPSOID["WGS 84",6378137,298.257223563,
                  LENGTHUNIT["metre",1]]],
          PRIMEM["Greenwich",0,
              ANGLEUNIT["degree",0.0174532925199433]],
          CS[ellipsoidal,2],
              AXIS["geodetic latitude (Lat)",north,
                  ORDER[1],
                  ANGLEUNIT["degree",0.0174532925199433]],
              AXIS["geodetic longitude (Lon)",east,
                  ORDER[2],
                  ANGLEUNIT["degree",0.0174532925199433]],
          ID["EPSG",4326]]
      Data axis to CRS axis mapping: 2,1
      id: String (0.0)
      name: String (0.0)
      inspireId: String (0.0)
      serviceType.title: String (0.0)
      serviceType.href: String (0.0)
      areaOfResponsibility.1.title: String (0.0)
      areaOfResponsibility.1.href: String (0.0)
      pointOfContact.address.thoroughfare: String (0.0)
      pointOfContact.address.locatorDesignator: String (0.0)
      pointOfContact.address.postCode: String (0.0)
      pointOfContact.address.adminUnit: String (0.0)
      pointOfContact.address.text: String (0.0)
      pointOfContact.telephoneVoice: String (0.0)
      pointOfContact.telephoneFacsimile: String (0.0)
      pointOfContact.telephoneFacsimileEmergency: String (0.0)
      inDistrict.title: String (0.0)
      inDistrict.href: String (0.0)
      inDistrictFreeTown.title: String (0.0)
      inDistrictFreeTown.href: String (0.0)
      inGovernmentalDistrict.title: String (0.0)
      inGovernmentalDistrict.href: String (0.0)

-  Filtering on a property (depending on if the server exposes filtering capabilities of the properties, part or totally of the filter might be evaluated on client side)

   ::

      $ ogrinfo OAPIF:https://ogc-api.nrw.de/inspire-us-feuerwehr governmentalservice -al -q -where "name = 'Schwelm'"

      Layer name: governmentalservice
      Metadata:
        DESCRIPTION=Staatliche Verwaltungs- und Sozialdienste wie öffentliche Verwaltung, Katastrophenschutz, Schulen und Krankenhäuser, die von öffentlichen oder privaten Einrichtungen erbracht werden, soweit sie in den Anwendungsbereich der Richtlinie 2007/2/EG fallen. Dieser Datensatz enthält Informationen zu Feuerwehrleitstellen.
        TITLE=Feuerwehrleitstellen
      OGRFeature(governmentalservice):1
        id (String) = LtS01
        name (String) = Schwelm
        inspireId (String) = https://geodaten.nrw.de/id/inspire-us-feuerwehr/governmentalservice/LtS01
        serviceType.title (String) = Brandschutzdienst
        serviceType.href (String) = http://inspire.ec.europa.eu/codelist/ServiceTypeValue/fireProtectionService
        areaOfResponsibility.1.title (String) = Breckerfeld
        areaOfResponsibility.1.href (String) = https://registry.gdi-de.org/id/de.nw.inspire.au.basis-dlm/AdministrativeUnit_05954004
        pointOfContact.address.thoroughfare (String) = Hauptstr.
        pointOfContact.address.locatorDesignator (String) = 92
        pointOfContact.address.postCode (String) = 58332
        pointOfContact.address.adminUnit (String) = Schwelm
        pointOfContact.address.text (String) = Hauptstr. 92, 58332 Schwelm
        pointOfContact.telephoneVoice (String) = +49233644400
        pointOfContact.telephoneFacsimile (String) = +4923364440400
        pointOfContact.telephoneFacsimileEmergency (String) = +49233644407100
        inDistrict.title (String) = Ennepe-Ruhr
        inDistrict.href (String) = Ennepe-Ruhr
        inGovernmentalDistrict.title (String) = Arnsberg
        inGovernmentalDistrict.href (String) = https://registry.gdi-de.org/id/de.nw.inspire.au.basis-dlm/AdministrativeUnit_059
        POINT (7.29854802787082 51.2855116825595)


-  Spatial filtering

   ::

      $ ogrinfo OAPIF:https://ogc-api.nrw.de/inspire-us-feuerwehr governmentalservice -al -q -spat 7.1 51.2 7.2 51.5

      Layer name: governmentalservice
      Metadata:
        DESCRIPTION=Staatliche Verwaltungs- und Sozialdienste wie öffentliche Verwaltung, Katastrophenschutz, Schulen und Krankenhäuser, die von öffentlichen oder privaten Einrichtungen erbracht werden, soweit sie in den Anwendungsbereich der Richtlinie 2007/2/EG fallen. Dieser Datensatz enthält Informationen zu Feuerwehrleitstellen.
        TITLE=Feuerwehrleitstellen
      OGRFeature(governmentalservice):1
        id (String) = LtS33
        name (String) = Wuppertal-Solingen
        inspireId (String) = https://geodaten.nrw.de/id/inspire-us-feuerwehr/governmentalservice/LtS33
        serviceType.title (String) = Brandschutzdienst
        serviceType.href (String) = http://inspire.ec.europa.eu/codelist/ServiceTypeValue/fireProtectionService
        areaOfResponsibility.1.title (String) = Wuppertal
        areaOfResponsibility.1.href (String) = https://registry.gdi-de.org/id/de.nw.inspire.au.basis-dlm/AdministrativeUnit_05124000
        pointOfContact.address.thoroughfare (String) = August-Bebel-Str.
        pointOfContact.address.locatorDesignator (String) = 55
        pointOfContact.address.postCode (String) = 42109
        pointOfContact.address.adminUnit (String) = Wuppertal
        pointOfContact.address.text (String) = August-Bebel-Str. 55, 42109 Wuppertal
        pointOfContact.telephoneVoice (String) = +492025631111
        pointOfContact.telephoneFacsimile (String) = +49202445331
        pointOfContact.telephoneFacsimileEmergency (String) = 112
        inDistrictFreeTown.title (String) = Wuppertal
        inDistrictFreeTown.href (String) = Wuppertal
        inGovernmentalDistrict.title (String) = Düsseldorf
        inGovernmentalDistrict.href (String) = https://registry.gdi-de.org/id/de.nw.inspire.au.basis-dlm/AdministrativeUnit_051
        POINT (7.13806554104892 51.2674471939457)

See Also
--------

-  `"OGC API - Features - Part 1: Core" Standard
   <http://docs.opengeospatial.org/is/17-069r3/17-069r3.html>`__
-  `"OGC API - Features - Part 2: Coordinate Reference Systems by Reference" Standard
   <https://docs.ogc.org/is/18-058/18-058.html>`__
-  :ref:`WFS (1.0,1.1,2.0) driver documentation <vector.wfs>`

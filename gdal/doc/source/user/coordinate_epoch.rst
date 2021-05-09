.. _coordinate_epoch:

================================================================================
Coordinate epoch support
================================================================================

.. versionadded:: 3.4

Dynamic CRS and coordinate epoch
--------------------------------

This document is intended to document the support for coordinate epoch, linked
to dynamic CRS.

In a dynamic CRS, coordinates of a point on the surface of the Earth may
change with time. To be unambiguous the coordinates must always be qualified
with the epoch at which they are valid. The coordinate epoch is not necessarily
the epoch at which the observation was collected.

Examples of dynamic CRS are ``WGS 84 (G1762)``, ``ITRF2014``, ``ATRF2014``.

The generic EPSG:4326 WGS 84 CRS is also considered dynamic, although it is
not recommended to use it due to being based on a datum ensemble whose positional
accuracy is 2 metres, but prefer one of its realizations, such as WGS 84 (G1762)

The :cpp:func:`OGRSpatialReference::IsDynamic` method can be used to test if
a CRS is a dynamic one.

The :cpp:func:`OGRSpatialReference::SetCoordinateEpoch` and
:cpp:func:`OGRSpatialReference::GetCoordinateEpoch` methods can be used to
set/retrieve a coordinate epoch associated with a CRS. The coordinate epoch is
expressed as a decimal year (e.g. 2021.3).
Pedantically the coordinate epoch of an observation belongs to the
observation, and not to the CRS, however it is often more practical to
bind it to the CRS.

Support in raster and vector formats
------------------------------------

At time of writing, no formats handled by GDAL/OGR have a standardized way of
encoding a coordinate epoch. We consequently have made choices how to encode it,
with the aim of being as much as possible backward compatible with existing
readers. Those encodings might change if corresponding official specifications
evolve to take this concept into account.
The coordinate epoch is only written when attached to the SRS of the layer/dataset
that is created.

FlatGeoBuf
++++++++++

The coordinate epoch is encoded as a WKT:2019 string using the ``EPOCH`` subnode of the
`COORDINATEMETADATA <http://docs.opengeospatial.org/is/18-010r7/18-010r7.html#130>`__
construct, set in the ``Crs.wkt`` header field of the FlatGeoBuf file.

::

    COORDINATEMETADATA[
        GEOGCRS["WGS 84 (G1762)",
            DYNAMIC[FRAMEEPOCH[2005.0]],
            DATUM["World Geodetic System 1984 (G1762)",
              ELLIPSOID["WGS 84",6378137,298.257223563,LENGTHUNIT["metre",1.0]]
            ],
            CS[ellipsoidal,3],
              AXIS["(lat)",north,ANGLEUNIT["degree",0.0174532925199433]],
              AXIS["(lon)",east,ANGLEUNIT["degree",0.0174532925199433]],
              AXIS["ellipsoidal height (h)",up,LENGTHUNIT["metre",1.0]]
        ],
        EPOCH[2016.47]
    ]


.. note:: Such construct will not be understood by GDAL < 3.4, but if the CRS has
          an associated EPSG code, this will not cause issues in those older
          GDAL versions.

GeoJSON
+++++++

The coordinate epoch is encoded as a  ``coordinate_epoch`` member of type numeric
under the ``FeatureCollection`` object type.

.. code-block:: json

    {
        "type": "FeatureCollection",
        "crs": { "type": "name", "properties": { "name": "urn:ogc:def:crs:EPSG::9000" } },
        "coordinate_epoch": 2021.3,
        "features": [  ]
    }

GeoPackage vector/raster
++++++++++++++++++++++++

Each vector/raster table which has an associated coordinate epoch has a corresponding
row in the ``gpkg_metadata`` table with:

- ``md_standard_uri`` = `http://gdal.org`
- ``mime_type`` = `text/plain`
- ``metadata`` = `coordinate_epoch={coordinate_epoch}` where `{coordinate_epoch}` is the value of the coordinate epoch

and an associate row in the ``gpkg_metadata_reference`` table pointing to it,
according to the requirements of the `gpkg_metadata <http://www.geopackage.org/spec130/index.html#extension_metadata>`__
extension.


GeoTIFF
+++++++

The coordinate epoch is encoded as a new GeoTIFF GeoKey, ``CoordinateEpochGeoKey``
of code 5120 and type DOUBLE.

::

    Geotiff_Information:
       Version: 1
       Key_Revision: 1.0
       Tagged_Information:
          ModelTiepointTag (2,3):
             0                 0                 0
             440720            3751320           0
          ModelPixelScaleTag (1,3):
             60                60                0
          End_Of_Tags.
       Keyed_Information:
          GTModelTypeGeoKey (Short,1): ModelTypeProjected
          GTRasterTypeGeoKey (Short,1): RasterPixelIsArea
          GTCitationGeoKey (Ascii,22): "WGS 84 / UTM zone 11N"
          GeogCitationGeoKey (Ascii,7): "WGS 84"
          GeogAngularUnitsGeoKey (Short,1): Angular_Degree
          ProjectedCSTypeGeoKey (Short,1): PCS_WGS84_UTM_zone_11N
          ProjLinearUnitsGeoKey (Short,1): Linear_Meter
          Unknown-5120 (Double,1): 2021.3
          End_Of_Keys.
       End_Of_Geotiff.


GML
+++

The coordinate epoch is encoded as a ``<!-- coordinateEpoch={coordinate_epoch} -->``
XML comment appended after the definition of geometry field in the .xsd XML Schema.
It must also be accompanied with a ``<!-- srsName="..." -->`` comment.

.. code-block:: xml

    <xs:element name="geometryProperty" type="gml:SurfacePropertyType" nillable="true"
        minOccurs="0" maxOccurs="1"/>
        <!-- restricted to Polygon -->
        <!-- srsName="urn:ogc:def:crs:EPSG::9000" -->
        <!-- coordinateEpoch=2021.3 -->


JPEG2000
++++++++

GeoJP2
******

GeoJP2 boxes use the above mentioned GeoTIFF encoding.

GMLJP2
******

GMLJP2 (v1) (resp. GMLJP2 (v2)) boxes encode the coordinate epoch as a
``<!-- coordinateEpoch={coordinate_epoch} -->`` XML comment, as a child of
gml:FeatureCollection/gml:boundedBy (resp.
gmljp2:GMLJP2CoverageCollection/gmljp2:featureMember/gmljp2:GMLJP2RectifiedGridCoverage/gml:boundedBy)

Example with GMLJP2 (v1):

.. code-block:: xml

    <gml:FeatureCollection
       xmlns:gml="http://www.opengis.net/gml"
       xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
       xsi:schemaLocation="http://www.opengis.net/gml http://schemas.opengis.net/gml/3.1.1/profiles/gmlJP2Profile/1.0.0/gmlJP2Profile.xsd">
      <gml:boundedBy>
        <!-- coordinateEpoch=2021.3 -->
        <gml:Envelope srsName="urn:ogc:def:crs:EPSG::32611">
          <gml:lowerCorner>440720 3750120</gml:lowerCorner>
          <gml:upperCorner>441920 3751320</gml:upperCorner>
        </gml:Envelope>
      </gml:boundedBy>
      <!-- snip -->
    </gml:FeatureCollection>


Example with GMLJP2 (v2):

.. code-block:: xml

    <gmljp2:GMLJP2CoverageCollection gml:id="ID_GMLJP2_0"
         xmlns:gml="http://www.opengis.net/gml/3.2"
         xmlns:gmlcov="http://www.opengis.net/gmlcov/1.0"
         xmlns:gmljp2="http://www.opengis.net/gmljp2/2.0"
         xmlns:swe="http://www.opengis.net/swe/2.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://www.opengis.net/gmljp2/2.0 http://schemas.opengis.net/gmljp2/2.0/gmljp2.xsd">
      <!-- snip -->
      <gmljp2:featureMember>
       <gmljp2:GMLJP2RectifiedGridCoverage gml:id="RGC_1_ID_GMLJP2_0">
         <gml:boundedBy>
           <!-- coordinate epoch: 2021.3 -->
           <gml:Envelope srsDimension="2" srsName="http://www.opengis.net/def/crs/EPSG/0/32611">
             <gml:lowerCorner>440720 3750120</gml:lowerCorner>
             <gml:upperCorner>441920 3751320</gml:upperCorner>
           </gml:Envelope>
         </gml:boundedBy>
         <!-- snip -->
       </gmljp2:GMLJP2RectifiedGridCoverage>
      </gmljp2:featureMember>
    </gmljp2:GMLJP2CoverageCollection>


KML
+++

The coordinate epoch is encoded as a ``<!-- coordinateEpoch={coordinate_epoch} -->``
XML comment appended after the top ``kml`` node.

.. code-block:: xml

    <kml xmlns="http://www.opengis.net/kml/2.2">
        <!-- coordinateEpoch=2021.3 -->
    </kml>


Persistent Auxiliary Metadata (.aux.xml)
++++++++++++++++++++++++++++++++++++++++

The coordinate epoch is encoded as ``coordinateEpoch`` attribute of the ``SRS``
element.

.. code-block:: xml

    <PAMDataset>
      <SRS dataAxisToSRSAxisMapping="1,2" coordinateEpoch="2021.3">PROJCS["WGS 84 / UTM zone 11N",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],AUTHORITY["EPSG","32611"]]</SRS>
      <!-- snip -->
    </PAMDataset>

Shapefile
+++++++++

The coordinate epoch is encoded as a WKT:2019 string using the ``EPOCH`` subnode of the
`COORDINATEMETADATA <http://docs.opengeospatial.org/is/18-010r7/18-010r7.html#130>`__
construct, and put in a sidecar file of extension ``wkt2``. This file has
precedence over the ``prj`` sidecar file.

::

    COORDINATEMETADATA[
        GEOGCRS["WGS 84 (G1762)",
            DYNAMIC[FRAMEEPOCH[2005.0]],
            DATUM["World Geodetic System 1984 (G1762)",
              ELLIPSOID["WGS 84",6378137,298.257223563,LENGTHUNIT["metre",1.0]]
            ],
            CS[ellipsoidal,3],
              AXIS["(lat)",north,ANGLEUNIT["degree",0.0174532925199433]],
              AXIS["(lon)",east,ANGLEUNIT["degree",0.0174532925199433]],
              AXIS["ellipsoidal height (h)",up,LENGTHUNIT["metre",1.0]]
        ],
        EPOCH[2016.47]
    ]


GDAL VRT
++++++++

The coordinate epoch is encoded as ``coordinateEpoch`` attribute of the ``SRS``
element.

.. code-block:: xml

    <VRTDataset rasterXSize="20" rasterYSize="20">
      <SRS dataAxisToSRSAxisMapping="1,2" coordinateEpoch="2021.3">PROJCS["WGS 84 / UTM zone 11N",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],AUTHORITY["EPSG","32611"]]</SRS>
      <!-- snip -->
    </VRTDataset>



Support in utilities
--------------------

:program:`gdalinfo` and :program:`ogrinfo` report the coordinate epoch, when
attached to a dataset/layer SRS.

:program:`gdal_translate` and :program:`ogr2ogr` have a ``-a_coord_epoch`` option to be used
together with ``-a_srs``, and otherwise preserve the coordinate epoch in the output SRS
from the source SRS when no SRS related options are specified.

:program:`gdalwarp` preserves the coordinate epoch in the output SRS when appropriate.


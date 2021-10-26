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
accuracy is 2 meters, but prefer one of its realizations, such as WGS 84 (G1762)

The :cpp:func:`OGRSpatialReference::IsDynamic` method can be used to test if
a CRS is a dynamic one.

The :cpp:func:`OGRSpatialReference::SetCoordinateEpoch` and
:cpp:func:`OGRSpatialReference::GetCoordinateEpoch` methods can be used to
set/retrieve a coordinate epoch associated with a CRS. The coordinate epoch is
expressed as a decimal year (e.g. 2021.3).

Formally, the coordinate epoch of an observation belongs to the
observation.  However, almost all formats do not allow for storing
per-observation epoch, and typical usage is a set of observations with
the same epoch.  Therefore we store the epoch as property of the CRS,
and the meaning of this is as if that epoch value were part of every
observation.  This choice eases processing, storage and format
complexity for most usage.  For now, this means that a dataset where
points have different epochs cannot be handled.

For vector formats, per-geometry coordinate epoch could also make sense, but as
most formats only support a per-layer CRS, we also for now limit support of
coordinate epoch at the layer level. The coordinate transformation mechanics
itself can support per-vertex coordinate epoch.

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

GeoPackage vector/raster
++++++++++++++++++++++++

Each vector/raster table which has an associated coordinate epoch encodes it
in the ``epoch`` column of the ``gpkg_spatial_ref_sys`` table, using an extended
version of the CRS WKT extension (https://github.com/opengeospatial/geopackage/pull/600).

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
          CoordinateEpochGeoKey (Double,1): 2021.3
          End_Of_Keys.
       End_Of_Geotiff.


JPEG2000
++++++++

GeoJP2 boxes use the above mentioned GeoTIFF encoding.


Persistent Auxiliary Metadata (.aux.xml)
++++++++++++++++++++++++++++++++++++++++

The coordinate epoch is encoded as ``coordinateEpoch`` attribute of the ``SRS``
element.

.. code-block:: xml

    <PAMDataset>
      <SRS dataAxisToSRSAxisMapping="1,2" coordinateEpoch="2021.3">PROJCS["WGS 84 / UTM zone 11N",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],AUTHORITY["EPSG","32611"]]</SRS>
      <!-- snip -->
    </PAMDataset>

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

:program:`gdalwarp` and :program:`ogr2ogr` have a ``-s_coord_epoch`` option to be used together with ``-s_srs``
(resp. ``-t_coord_epoch`` option to be used together with ``-t_srs``) to override/set the
coordinate epoch of the source (resp. target) CRS. ``-s_coord_epoch`` and
``-t_coord_epoch`` are currently mutually exclusive, due to lack of support for
transformations between two dynamic CRS.

:program:`gdalwarp` preserves the coordinate epoch in the output SRS when appropriate.


Support in coordinate transformation
------------------------------------

The :cpp:class:`OGRCoordinateTransformation` class can perform time-dependent
transformations between a static and dynamic CRS based on the coordinate epoch
passed per vertex.

It can also take into account the coordinate epoch associated with a dynamic
CRS, when doing time-dependent transformations between a static and dynamic CRS.
The :decl_configoption:`OGR_CT_USE_SRS_COORDINATE_EPOCH` configuration option
can be set to ``NO`` to disable using the coordinate epoch associated with the
source or target CRS.

If a per-vertex time is specified, it overrides the one associated with the CRS.

Note that dynamic CRS to dynamic CRS transformations are not supported currently.



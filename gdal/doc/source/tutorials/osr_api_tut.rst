.. _osr_api_tut:

================================================================================
OGR Coordinate Reference Systems and Coordinate Transformation tutorial
================================================================================

.. highlight:: cpp

.. toctree::
   :hidden:

   wktproblems


Introduction
------------

The :cpp:class:`OGRSpatialReference` and :cpp:class:`OGRCoordinateTransformation` classes provide
respectively services to represent coordinate reference systems (known as CRS
or SRS, such as typically a projected CRS associating a map projection with a geodetic
datums) and to transform between them.  These services are loosely modeled on the
OpenGIS Coordinate Transformations specification, and rely on the
Well Known Text (WKT) format (in its various versions: OGC WKT 1, ESRI WKT,
WKT2:2015 and WKT2:2018) for describing coordinate systems.

References and applicable standards
-----------------------------------

- `PROJ documentation <https://proj4.org>`_: projection methods and coordinate operations
- `ISO:19111 and WKT standards <https://proj4.org/development/reference/cpp/cpp_general.html#standards>`_
- `GeoTIFF Projections Transform List <http://geotiff.maptools.org/proj_list>`_: understanding formulations of projections in WKT for GeoTIFF
- `EPSG Geodesy web page <http://www.epsg.org>`_ is also a useful resource

Defining a Geographic Coordinate Reference System
-------------------------------------------------

CRS are encapsulated in the :cpp:class:`OGRSpatialReference` class. There
are a number of ways of initializing an OGRSpatialReference object to a
valid coordinate reference system.  There are two primary kinds of CRS.
The first is geographic (positions are measured in long/lat) and the second
is projected (such as UTM - positions are measured in meters or feet).

A Geographic CRS contains information on the datum (which implies
an spheroid described by a semi-major axis, and inverse flattening), prime
meridian (normally Greenwich), and an angular units type which is normally
degrees.  The following code initializes a geographic CRS
on supplying all this information along with a user visible name for the
geographic CRS.

.. code-block::

    OGRSpatialReference oSRS;

    oSRS.SetGeogCS( "My geographic CRS",
                    "World Geodetic System 1984",
                    "My WGS84 Spheroid",
                    SRS_WGS84_SEMIMAJOR, SRS_WGS84_INVFLATTENING,
                    "Greenwich", 0.0,
                    "degree", SRS_UA_DEGREE_CONV );

.. note::

    The abbreviation CS in :cpp:func:`OGRSpatialReference::SetGeogCS` is not appropriate according to current
    geodesic terminology, and should be understood as CRS

Of these values, the names "My geographic CRS", "My WGS84
Spheroid", "Greenwich" and "degree" are not keys, but are used for display
to the user.  However, the datum name "World Geodetic System 1984" is used as a key to identify
the datum, and should be set to a known value from the EPSG registry, so that
appropriate datum transformations can be done during coordinate operations.
The list of valid geodetic datum can be seen in the 3rd column of the
`geodetic_datum.sql <https://github.com/OSGeo/PROJ/blob/master/data/sql/geodetic_datum.sql>`_
file.

.. note::

    In WKT 1, space characters in datum names are normally replaced by underscore.
    And WGS_1984 is used as an alias of "World Geodetic System 1984"

The OGRSpatialReference has built in support for a few well known CRS,
which include "NAD27", "NAD83", "WGS72" and "WGS84" which can be
defined in a single call to :cpp:func:`OGRSpatialReference::SetWellKnownGeogCS`.

.. code-block::

    oSRS.SetWellKnownGeogCS( "WGS84" );


.. note::

    The abbreviation CS in SetWellKnownGeogCS() is not appropriate according to current
    geodesic terminology, and should be understood as CRS

Furthermore, any geographic CRS in the EPSG database can
be set by its GCS code number if the EPSG database is available.

.. code-block::

    oSRS.SetWellKnownGeogCS( "EPSG:4326" );

For serialization, and transmission of projection definitions to other
packages, the OpenGIS Well Known Text format for coordinate systems is
used.  An OGRSpatialReference can be initialized from WKT, or
converted back into WKT. As of GDAL 3.0, the default format for WKT export
is still OGC WKT 1.

.. code-block::

    char *pszWKT = NULL;
    oSRS.SetWellKnownGeogCS( "WGS84" );
    oSRS.exportToWkt( &pszWKT );
    printf( "%s\n", pszWKT );
    CPLFree(pszWKT);

outputs:

::

    GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,
    AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,
    AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,
    AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],
    AUTHORITY["EPSG","4326"]]

or in more readable form:

::

    GEOGCS["WGS 84",
        DATUM["WGS_1984",
            SPHEROID["WGS 84",6378137,298.257223563,
                AUTHORITY["EPSG","7030"]],
            AUTHORITY["EPSG","6326"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9122"]],
        AXIS["Latitude",NORTH],
        AXIS["Longitude",EAST],
        AUTHORITY["EPSG","4326"]]

Starting with GDAL 3.0, the :cpp:func:`OGRSpatialReference::exportToWkt` method accepts options,

.. code-block::

        char *pszWKT = nullptr;
        oSRS.SetWellKnownGeogCS( "WGS84" );
        const char* apszOptions[] = { "FORMAT=WKT2_2018", "MULTILINE=YES", nullptr };
        oSRS.exportToWkt( &pszWKT, apszOptions );
        printf( "%s\n", pszWKT );
        CPLFree(pszWKT);

::

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

This method with options is available in C as the :cpp:func:`OSRExportToWktEx` function.

The :cpp:func:`OGRSpatialReference::importFromWkt` method can be used to set an
OGRSpatialReference from a WKT CRS definition.

CRS and axis order
------------------

One "detail" that has been omitted in previous sections is the topic of the
order of coordinate axis in a CRS. A Geographic CRS is, according to ISO:19111
modeling, made of two main components: a geodetic datum and a
`coordinate system <http://docs.opengeospatial.org/as/18-005r4/18-005r4.html#42>`_.
For 2D geographic CRS, the coordinate system axes are the longitude and the latitude,
and the values along those axes are expressed generally in degree (ancient French-based CRS may use grad).

The order in which they are specified, that is latitude first, longitude second,
or the reverse, is a constant matter of confusion and vary depending on conventions
used by geodetic authorities, GIS user, file format and protocol specifications, etc...
This is the source of various interoperability issues.

Before GDAL 3.0, the :cpp:class:`OGRSpatialReference` class did not honour the axis order mandated by the authority
defining a CRS and consequently stripped axis order information from the WKT string when the
order was latitude first, longitude second. Coordinate transformations using the
OGRCoordinateTransformation class also assumed that geographic coordinates
passed or returned by the Transform() method of this class used the longitude,
latitude order.

Starting with GDAL 3.0, the axis order mandated by the authority defining a CRS
is by default honoured by the OGRCoordinateTransformation class, and always exported
in WKT1. Consequently CRS created with the "EPSG:4326" or "WGS84" strings use
the latitude first, longitude second axis order.

In order to help migration from code bases still using coordinates with the
longitude, latitude order, it is possible to attach a metadata information to
a OGRSpatialReference instance, to specify that for the purpose of coordinate
transformations, the order of values effectively passed or returned, will be
longitude, latitude. For that, the following must be called

.. code-block::

    oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

The argument passed to :cpp:func:`OGRSpatialReference::SetAxisMappingStrategy` is the
data axis to CRS axis mapping strategy.

- :c:macro:`OAMS_TRADITIONAL_GIS_ORDER` means that for geographic CRS with lat/long order, the data will still be long/lat ordered. Similarly for a projected CRS with northing/easting order, the data will still be easting/northing ordered.
- :c:macro:`OAMS_AUTHORITY_COMPLIANT` means that the data axis will be identical to the CRS axis. This is the fdefault value when instantiating OGRSpatialReference.
- :c:macro:`OAMS_CUSTOM` means that the data axis are customly defined with SetDataAxisToSRSAxisMapping().

What has been discussed in this section for the particular case of Geographic
CRS also applies to Projected CRS. While most of them use Easting first, Northing
second convention, some defined in the EPSG registry use the reverse convention.

Another way to keep using the Traditional GIS order for some specific well known CRS is to
calling to :cpp:func:`OGRSpatialReference::SetWellKnownGeogCS` with
"CRS27", "CRS83" or "CRS84" instead of "NAD27", "NAD83" and "WGS84" respectively.

.. code-block::

    oSRS.SetWellKnownGeogCS( "CRS84" );

Defining a Projected CRS
------------------------

A projected CRS (such as UTM, Lambert Conformal Conic, etc)
requires and underlying geographic CRS as well as a definition
for the projection transform used to translate between linear positions
(in meters or feet) and angular long/lat positions.  The following code
defines a UTM zone 17 projected CRS with an underlying
geographic CRS (datum) of WGS84.

.. code-block::

    OGRSpatialReference oSRS;

    oSRS.SetProjCS( "UTM 17 (WGS84) in northern hemisphere." );
    oSRS.SetWellKnownGeogCS( "WGS84" );
    oSRS.SetUTM( 17, TRUE );

Calling :cpp:func:`OGRSpatialReference::SetProjCS` sets a user
name for the projected CRS and establishes that the system
is projected.  The :cpp:func:`OGRSpatialReference::SetWellKnownGeogCS` associates a geographic coordinate
system, and the :cpp:func:`OGRSpatialReference::SetUTM` call sets detailed projection transformation
parameters.  At this time the above order is important in order to
create a valid definition, but in the future the object will automatically
reorder the internal representation as needed to remain valid.

.. caution::

    For now, be careful of the order of steps defining an OGRSpatialReference!

The above definition would give a WKT version that looks something like
the following.  Note that the UTM 17 was expanded into the details
transverse mercator definition of the UTM zone.

::

    PROJCS["UTM 17 (WGS84) in northern hemisphere.",
        GEOGCS["WGS 84",
            DATUM["WGS_1984",
                SPHEROID["WGS 84",6378137,298.257223563,
                    AUTHORITY["EPSG",7030]],
                TOWGS84[0,0,0,0,0,0,0],
                AUTHORITY["EPSG",6326]],
            PRIMEM["Greenwich",0,AUTHORITY["EPSG",8901]],
            UNIT["DMSH",0.0174532925199433,AUTHORITY["EPSG",9108]],
            AXIS["Lat",NORTH],
            AXIS["Long",EAST],
            AUTHORITY["EPSG",4326]],
        PROJECTION["Transverse_Mercator"],
        PARAMETER["latitude_of_origin",0],
        PARAMETER["central_meridian",-81],
        PARAMETER["scale_factor",0.9996],
        PARAMETER["false_easting",500000],
        PARAMETER["false_northing",0]]

There are methods for many projection methods including :cpp:func:`OGRSpatialReference::SetTM` (Transverse
Mercator), :cpp:func:`OGRSpatialReference::SetLCC` (Lambert Conformal Conic), and :cpp:func:`OGRSpatialReference::SetMercator`.

Querying Coordinate Reference System
------------------------------------

Once an OGRSpatialReference has been established, various information about
it can be queried.  It can be established if it is a projected or
geographic CRS using the :cpp:func:`OGRSpatialReference::IsProjected` and
:cpp:func:`OGRSpatialReference::IsGeographic` methods.  The :cpp:func:`OGRSpatialReference::GetSemiMajor`, :cpp:func:`OGRSpatialReference::GetSemiMinor` and
:cpp:func:`OGRSpatialReference::GetInvFlattening` methods can be used to get
information about the spheroid.  The :cpp:func:`OGRSpatialReference::GetAttrValue`
method can be used to get the PROJCS, GEOGCS, DATUM, SPHEROID, and PROJECTION
names strings.  The :cpp:func:`OGRSpatialReference::GetProjParm` method can be used to
get the projection parameters.  The :cpp:func:`OGRSpatialReference::GetLinearUnits`
method can be used to fetch the linear units type, and translation to meters.

Note that the names of the projection method and parameters is the one of
WKT 1.

The following code demonstrates use
of :cpp:func:`OGRSpatialReference::GetAttrValue` to get the projection, and :cpp:func:`OGRSpatialReference::GetProjParm` to get projection
parameters.  The GetAttrValue() method searches for the first "value"
node associated with the named entry in the WKT text representation.
The #define'ed constants for projection parameters (such as
SRS_PP_CENTRAL_MERIDIAN) should be used when fetching projection parameter
with GetProjParm(). The code for the Set methods of the various projections
in ogrspatialreference.cpp can be consulted to find which parameters apply to
which projections.

.. code-block::

    const char *pszProjection = poSRS->GetAttrValue("PROJECTION");

    if( pszProjection == NULL )
    {
        if( poSRS->IsGeographic() )
            sprintf( szProj4+strlen(szProj4), "+proj=longlat " );
        else
            sprintf( szProj4+strlen(szProj4), "unknown " );
    }
    else if( EQUAL(pszProjection,SRS_PT_CYLINDRICAL_EQUAL_AREA) )
    {
        sprintf( szProj4+strlen(szProj4),
        "+proj=cea +lon_0=%.9f +lat_ts=%.9f +x_0=%.3f +y_0=%.3f ",
                poSRS->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                poSRS->GetProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0),
                poSRS->GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                poSRS->GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }
    ...


Coordinate Transformation
-------------------------

The :cpp:class:`OGRCoordinateTransformation` class is used for translating positions
between different CRS.  New transformation objects are
created using :cpp:func:`OGRCreateCoordinateTransformation`, and then the
:cpp:func:`OGRCoordinateTransformation::Transform` method can be used to convert
points between CRS.

.. code-block::

    OGRSpatialReference oSourceSRS, oTargetSRS;
    OGRCoordinateTransformation *poCT;
    double x, y;

    oSourceSRS.importFromEPSG( atoi(papszArgv[i+1]) );
    oTargetSRS.importFromEPSG( atoi(papszArgv[i+2]) );

    poCT = OGRCreateCoordinateTransformation( &oSourceSRS,
                                              &oTargetSRS );
    x = atof( papszArgv[i+3] );
    y = atof( papszArgv[i+4] );

    if( poCT == NULL || !poCT->Transform( 1, &x, &y ) )
        printf( "Transformation failed.\n" );
    else
    {
        printf( "(%f,%f) -> (%f,%f)\n",
                atof( papszArgv[i+3] ),
                atof( papszArgv[i+4] ),
                x, y );
    }


There are a couple of points at which transformations can
fail.  First, OGRCreateCoordinateTransformation() may fail,
generally because the internals recognise that no transformation
between the indicated systems can be established, and will
return a NULL pointer.

The OGRCoordinateTransformation::Transform() method itself can
also fail.  This may be as a delayed result of one of the above
problems, or as a result of an operation being numerically
undefined for one or more of the passed in points.  The
Transform() function will return TRUE on success, or FALSE
if any of the points fail to transform.  The point array is
left in an indeterminate state on error.

Though not shown above, the coordinate transformation service can
take 3D points, and will adjust elevations for elevation differences
in spheroids, and datums. Elevations given on a geographic or projected CRS
are assumed to be ellipsoidal heights. When using a compound CRS made of a
horizontal CRS (geographic or projected) and a vertical CRS, elevations will
be related to a vertical datum (mean sea level, gravity based, etc.).

Starting with GDAL 3.0, a time value (generally as a vale in decimal years) can
also be specified for time-dependent coordinate operations.

The following example shows how to conveniently create a long/lat coordinate
system using the same geographic CRS as a projected coordinate
system, and using that to transform between projected coordinates and
long/lat. The returned coordinates will be in longitude, latitude order due to
the call to SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER)

.. code-block::

    OGRSpatialReference    oUTM, *poLongLat;
    OGRCoordinateTransformation *poTransform;

    oUTM.SetProjCS("UTM 17 / WGS84");
    oUTM.SetWellKnownGeogCS( "WGS84" );
    oUTM.SetUTM( 17 );

    poLongLat = oUTM.CloneGeogCS();
    poLongLat->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    poTransform = OGRCreateCoordinateTransformation( &oUTM, poLongLat );
    if( poTransform == NULL )
    {
        ...
    }

    ...

    if( !poTransform->Transform( nPoints, x, y, z ) )
    ...

Advanced Coordinate Transformation
----------------------------------

OGRCreateCoordinateTransformation() under-the-hood may determine several candidate
coordinate operations transforming from the source CRS to the target CRS. Those
candidate coordinate operations have each their own area of use. When Transform()
is invoked, it will determine the most appropriate coordinate operation based on
the coordinates of the point to transform and those area of use. For example,
there are several dozens of possible coordinate operations for the NAD27 to WGS84
transformation.

If a bounding box of the area of interest into which coordinates to transform
are located is known, it is possible to specify it to restrict the candidate
coordinate operations to consider:

.. code-block::

    OGRCoordinateTransformationOptions options;
    options.SetAreaOfInterest(-100,40,-99,41);
    poTransform = OGRCreateCoordinateTransformation( &oNAD27, &oWGS84, options );

For cases where a particular coordinate operation must be used, it is possible
to specify it as as a PROJ string (single step operation or multiple step string
starting with +proj=pipeline), a WKT2 string describing a CoordinateOperation,
or a urn:ogc:def:coordinateOperation:EPSG::XXXX URN

.. code-block::

    OGRCoordinateTransformationOptions options;

    // EPSG:8599, NAD27 to WGS 84 (46), 1.15 m, USA - Indiana
    options.SetCoordinateOperation(
        "+proj=pipeline +step +proj=axisswap +order=2,1 "
        "+step +proj=unitconvert +xy_in=deg +xy_out=rad "
        "+step +proj=hgridshift +grids=conus "
        "+step +proj=hgridshift +grids=inhpgn.gsb "
        "+step +proj=unitconvert +xy_in=rad +xy_out=deg +step "
        "+proj=axisswap +order=2,1", false );

    // or
    // options.SetCoordinateOperation(
    //      "urn:ogc:def:coordinateOperation:EPSG::8599", false);

    poTransform = OGRCreateCoordinateTransformation( &oNAD27, &oWGS84, options );


Alternate Interfaces
--------------------

A C interface to the coordinate system services is defined in
ogr_srs_api.h, and Python bindings are available via the osr.py module.
Methods are close analogs of the C++ methods but C and Python bindings
are missing for some C++ methods.

C bindings
++++++++++

.. code-block:: c

    typedef void *OGRSpatialReferenceH;
    typedef void *OGRCoordinateTransformationH;

    OGRSpatialReferenceH OSRNewSpatialReference( const char * );
    void    OSRDestroySpatialReference( OGRSpatialReferenceH );

    int     OSRReference( OGRSpatialReferenceH );
    int     OSRDereference( OGRSpatialReferenceH );

    void OSRSetAxisMappingStrategy( OGRSpatialReferenceH,
                                    OSRAxisMappingStrategy );

    OGRErr  OSRImportFromEPSG( OGRSpatialReferenceH, int );
    OGRErr  OSRImportFromWkt( OGRSpatialReferenceH, char ** );
    OGRErr  OSRExportToWkt( OGRSpatialReferenceH, char ** );
    OGRErr  OSRExportToWktEx( OGRSpatialReferenceH, char **,
                            const char* const* papszOptions );

    OGRErr  OSRSetAttrValue( OGRSpatialReferenceH hSRS, const char * pszNodePath,
                            const char * pszNewNodeValue );
    const char *OSRGetAttrValue( OGRSpatialReferenceH hSRS,
                                const char * pszName, int iChild);

    OGRErr  OSRSetLinearUnits( OGRSpatialReferenceH, const char *, double );
    double  OSRGetLinearUnits( OGRSpatialReferenceH, char ** );

    int     OSRIsGeographic( OGRSpatialReferenceH );
    int     OSRIsProjected( OGRSpatialReferenceH );
    int     OSRIsSameGeogCS( OGRSpatialReferenceH, OGRSpatialReferenceH );
    int     OSRIsSame( OGRSpatialReferenceH, OGRSpatialReferenceH );

    OGRErr  OSRSetProjCS( OGRSpatialReferenceH hSRS, const char * pszName );
    OGRErr  OSRSetWellKnownGeogCS( OGRSpatialReferenceH hSRS,
                                const char * pszName );

    OGRErr  OSRSetGeogCS( OGRSpatialReferenceH hSRS,
                        const char * pszGeogName,
                        const char * pszDatumName,
                        const char * pszEllipsoidName,
                        double dfSemiMajor, double dfInvFlattening,
                        const char * pszPMName ,
                        double dfPMOffset ,
                        const char * pszUnits,
                        double dfConvertToRadians );

    double  OSRGetSemiMajor( OGRSpatialReferenceH, OGRErr * );
    double  OSRGetSemiMinor( OGRSpatialReferenceH, OGRErr * );
    double  OSRGetInvFlattening( OGRSpatialReferenceH, OGRErr * );

    OGRErr  OSRSetAuthority( OGRSpatialReferenceH hSRS,
                            const char * pszTargetKey,
                            const char * pszAuthority,
                            int nCode );
    OGRErr  OSRSetProjParm( OGRSpatialReferenceH, const char *, double );
    double  OSRGetProjParm( OGRSpatialReferenceH hSRS,
                            const char * pszParamName,
                            double dfDefault,
                            OGRErr * );

    OGRErr  OSRSetUTM( OGRSpatialReferenceH hSRS, int nZone, int bNorth );
    int     OSRGetUTMZone( OGRSpatialReferenceH hSRS, int *pbNorth );

    OGRCoordinateTransformationH
    OCTNewCoordinateTransformation( OGRSpatialReferenceH hSourceSRS,
                                    OGRSpatialReferenceH hTargetSRS );

    void OCTDestroyCoordinateTransformation( OGRCoordinateTransformationH );

    int OCTTransform( OGRCoordinateTransformationH hCT,
                    int nCount, double *x, double *y, double *z );

    OGRCoordinateTransformationOptionsH OCTNewCoordinateTransformationOptions(;

    int OCTCoordinateTransformationOptionsSetOperation(
        OGRCoordinateTransformationOptionsH hOptions,
        const char* pszCO, int bReverseCO);

    int OCTCoordinateTransformationOptionsSetAreaOfInterest(
        OGRCoordinateTransformationOptionsH hOptions,
        double dfWestLongitudeDeg,
        double dfSouthLatitudeDeg,
        double dfEastLongitudeDeg,
        double dfNorthLatitudeDeg);

    void OCTDestroyCoordinateTransformationOptions(OGRCoordinateTransformationOptionsH);

    OGRCoordinateTransformationH
    OCTNewCoordinateTransformationEx( OGRSpatialReferenceH hSourceSRS,
                                    OGRSpatialReferenceH hTargetSRS,
                                    OGRCoordinateTransformationOptionsH hOptions );

Python bindings
+++++++++++++++

.. code-block:: python

    class osr.SpatialReference
        def __init__(self,obj=None):
        def SetAxisMappingStrategy( self, strategy ):
        def ImportFromWkt( self, wkt ):
        def ExportToWkt(self, options = None):
        def ImportFromEPSG(self,code):
        def IsGeographic(self):
        def IsProjected(self):
        def GetAttrValue(self, name, child = 0):
        def SetAttrValue(self, name, value):
        def SetWellKnownGeogCS(self, name):
        def SetProjCS(self, name = "unnamed" ):
        def IsSameGeogCS(self, other):
        def IsSame(self, other):
        def SetLinearUnits(self, units_name, to_meters ):
        def SetUTM(self, zone, is_north = 1):

    class CoordinateTransformation:
        def __init__(self,source,target):
        def TransformPoint(self, x, y, z = 0):
        def TransformPoints(self, points):

History and implementation considerations
-----------------------------------------

Before GDAL 3.0, the OGRSpatialReference class was strongly tied to OGC WKT (WKT 1)
format specified by `Coordinate Transformation Services (CT) specification (01-009) <http://portal.opengeospatial.org/files/?artifact_id=999>`_,
and the way
it was interpreted by GDAL, which various caveats detailed in
the :ref:`wktproblems` page.
The class was mostly containing a in-memory tree-like representation of WKT 1 strings.
The class used to directly implement import and export to OGC WKT 1, WKT-ESRI and PROJ.4
formats. Reprojection services were only available if GDAL had been build against
the PROJ library.

Starting with GDAL 3.0, the `PROJ <https://proj4.org>`_ >= 6.0 library
has become a required dependency of GDAL.
PROJ 6 has built-in support for OGC WKT 1, ESRI WKT, OGC WKT 2:2015
and OGC WKT 2:2018 representations. PROJ 6 also implements a C++ object class
hierarchy of the ISO-19111 / OGC Abstract Topic 2 "Referencing by coordinate" standard.
Consequently the OGRSpatialReference class has been modified to act mostly as
a wrapper on top of PROJ PJ* CRS objects, and tries to abstract away from
the OGC WKT 1 representation as much as possible. However, for backward compatibility,
some methods still expect arguments or return values that are specific of OGC WKT 1.
The design of th OGRSpatialReference class is also still monolithic. Users wanting
direct and fine grained access to CRS representations might want to directly use
the PROJ 6 C or C++ API.

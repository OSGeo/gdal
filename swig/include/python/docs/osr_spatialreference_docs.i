%feature("docstring") OSRSpatialReferenceShadow "
Python proxy of an :cpp:class:`OGRSpatialReference`.
";

%extend OSRSpatialReferenceShadow {

%feature("docstring") AutoIdentifyEPSG "

Add an EPSG authority code to the CRS
where an aspect of the coordinate system can be easily and safely
corresponded with an EPSG identifier.

See :cpp:func:`OGRSpatialReference::AutoIdentifyEPSG`.

Returns
-------
int
    :py:const:`OGRERR_NONE` or :py:const:`OGRERR_UNSUPPORTED_SRS`.

";

%feature("docstring") ExportToPrettyWkt "

Convert this SRS into a nicely formatted WKT 1 string for display to a
person.

See :cpp:func:`OGRSpatialReference::exportToPrettyWkt`.

Parameters
----------
simplify : bool, default = False

Returns
-------
str

";

%feature("docstring") ExportToProj4 "

Export this SRS to PROJ.4 legacy format.

.. warning::

   Use of this function is discouraged. See :cpp:func:`OGRSpatialReference::exportToProj4`.

Returns
-------
str

";

%feature("docstring") ExportToPROJJSON "

Export this SRS in `PROJJSON <https://proj.org/en/latest/specifications/projjson.html>`_ format.

See :cpp:func:`OGRSpatialReference::exportToPROJJSON`.

Parameters
----------
options : list/dict
    Options to control the format of the output. See :cpp:func:`OGRSpatialReference::ExportToPROJJSON`.

Returns
-------
str

";

%feature("docstring") ExportToWkt "

Export  this SRS into WKT 1 format.

See :cpp:func:`OGRSpatialReference::exportToWkt`.

Returns
-------
str

See Also
--------
:py:meth:`ExportToPrettyWkt`

";

%feature("docstring") GetAuthorityCode "

Get the authority code for a node.

See :cpp:func:`OGRSpatialReference::GetAuthorityCode`.

Parameters
----------
target_key : str
    the partial or complete path to the node to get an authority from
    (e.g., 'PROJCS', 'GEOGCS' or ``None`` to get an authority code
    on the root element)

Returns
-------
str or ``None`` on failure

Examples
--------
>>> srs = osr.SpatialReference()
>>> srs.ImportFromEPSG(4326)
0
>>> srs.GetAuthorityName('DATUM')
'EPSG'
>>> srs.GetAuthorityCode('DATUM')
'6326'
>>> srs.GetAuthorityCode(None)
'4326'

";

%feature("docstring") GetAngularUnits "

Fetch conversion between angular geographic coordinate system units and radians.

See :cpp:func:`OGRSpatialReference::GetAngularUnits`.

Returns
-------
float
    Value to multiply angular distances by to transform them to radians.

Examples
--------
>>> srs = osr.SpatialReference()
>>> srs.ImportFromEPSG(4326)
0
>>> srs.GetAngularUnits()
0.017453292519943295

";

%feature("docstring") GetAngularUnitsName "

Fetch angular geographic coordinate system units.

See :cpp:func:`OGRSpatialReference::GetAngularUnits`.

Returns
-------
str

Examples
--------
>>> srs = osr.SpatialReference()
>>> srs.ImportFromEPSG(4326)
0
>>> srs.GetAngularUnitsName()
'degree'

";

%feature("docstring") GetAreaOfUse "

Return the area of use of the SRS.

See :cpp:func:`OGRSpatialReference::GetAreaOfUse`.

Returns
-------
AreaOfUse
    object providing a description of the area of use as well as bounding parallels / meridians

Examples
--------

>>> vt_sp = osr.SpatialReference()
>>> vt_sp.ImportFromEPSG(5646)
0
>>> aou = vt_sp.GetAreaOfUse()
>>> aou.name
'United States (USA) - Vermont - counties of Addison; Bennington; Caledonia; Chittenden; Essex; Franklin; Grand Isle; Lamoille; Orange; Orleans; Rutland; Washington; Windham; Windsor.'
>>> aou.west_lon_degree, aou.south_lat_degree, aou.east_lon_degree, aou.north_lat_degree
(-73.44, 42.72, -71.5, 45.03)

";

%feature("docstring") GetAttrValue "

Fetch indicated attribute of named node.

See :cpp:func:`OGRSpatialReference::GetAttrValue`.

Parameters
----------
name : str
    tree node to look for (case insensitive)
child : int, default = 0
    0-indexed child of the node

Returns
-------
str

Examples
--------
>>> vt_sp = osr.SpatialReference()
>>> vt_sp.ImportFromEPSG(5646)
0
>>> vt_sp.GetAttrValue('UNIT', 0)
'US survey foot'

";

%feature("docstring") GetAuthorityName "

Get the authority name for a node.

See :cpp:func:`OGRSpatialReference::GetAuthorityName`.

Parameters
----------
target_key : str
    the partial or complete path to the node to get an authority from
    (e.g., 'PROJCS', 'GEOGCS' or ``None`` to get an authority name
    on the root element)

Returns
-------
str
";

%feature("docstring") GetAxesCount "

Return the number of axes of the coordinate system of the CRS.

See :cpp:func:`OGRSpatialReference::GetAxesCount`.

Returns
-------
int

Examples
--------
>>> srs = osr.SpatialReference()
>>> srs.ImportFromEPSG(4326)
0
>>> srs.GetAxesCount()
2
>>> srs.ImportFromEPSG(4979)
0
>>> srs.GetAxesCount()
3
";

%feature("docstring") GetAxisMappingStrategy "

Return the data axis to CRS axis mapping strategy:

- :py:const:`OAMS_TRADITIONAL_GIS_ORDER` means that for geographic CRS
  with lag/long order, the data will still be long/lat ordered. Similarly
  for a projected CRS with northing/easting order, the data will still be
  easting/northing ordered.
- :py:const:`OAMS_AUTHORITY_COMPLIANT` means that the data axis will be
  identical to the CRS axis.
- :py:const:`OAMS_CUSTOM` means that the ordering is defined with
  :py:meth:`SetDataAxisToSRSAxisMapping`.

See :cpp:func:`OGRSpatialReference::GetAxisMappingStrategy`.

Returns
-------
int

";

%feature("docstring") GetAxisName "

Fetch an axis description.

See :cpp:func:`OGRSpatialReference::GetAxis`.

Parameters
----------
target_key : str
    The portion of the coordinate system, either 'GEOGCS' or 'PROJCS'
iAxis : int
    The 0-based index of the axis to fetch

Returns
-------
str

Examples
--------
>>> srs = osr.SpatialReference()
>>> srs.ImportFromEPSG(4979)
0
>>> for i in range(3):
...     srs.GetAxisName('GEOGCS', i)
...
'Geodetic latitude'
'Geodetic longitude'
'Ellipsoidal height'

";

%feature("docstring") GetAxisOrientation "

Fetch an axis orientation.

See :cpp:func:`OGRSpatialReference::GetAxis`.

Parameters
----------
target_key : str
    The portion of the coordinate system, either 'GEOGCS' or 'PROJCS'
iAxis : int
    The 0-based index of the axis to fetch

Returns
-------
int

Examples
--------
>>> srs = osr.SpatialReference()
>>> srs.ImportFromEPSG(4979)
0
>>> srs.GetAxisOrientation('GEOGCS', 0) == osr.OAO_North
True
>>> srs.GetAxisOrientation('GEOGCS', 1) == osr.OAO_East
True
>>> srs.GetAxisOrientation('GEOGCS', 2) == osr.OAO_Up
True
";

%feature("docstring") GetCoordinateEpoch "

Return the coordinate epoch as a decimal year.

See :cpp:func:`OGRSpatialReference::GetCoordinateEpoch`.

Returns
-------
float
    coordinate epoch as a decimal year, or 0 if not set/relevant

";

%feature("docstring") GetDataAxisToSRSAxisMapping "

Return the data axis to SRS axis mapping.

See :cpp:func:`OGRSpatialReference::GetDataAxisToSRSAxisMapping`.

Returns
-------
tuple

";

%feature("docstring") GetInvFlattening "

Get the spheroid inverse flattening.

See :cpp:func:`OGRSpatialReference::GetInvFlattening`.

Returns
-------
float

Examples
--------
>>> srs = osr.SpatialReference()
>>> srs.ImportFromEPSG(4326) # WGS84
0
>>> srs.GetInvFlattening()
298.257223563
>>> srs.ImportFromEPSG(4269) # NAD83
0
>>> srs.GetInvFlattening()
298.257222101
";

%feature("docstring") GetLinearUnits "

Fetch the conversion between linear projection units and meters.

See :cpp:func:`OGRSpatialReference::GetLinearUnits`.

Returns
-------
float

Examples
--------
>>> vt_sp = osr.SpatialReference()
>>> vt_sp.ImportFromEPSG(5646)
0
>>> vt_sp.GetLinearUnits()
0.30480060960121924

";

%feature("docstring") GetLinearUnitsName "

Fetch the name of the linear projection units.

See :cpp:func:`OGRSpatialReference::GetLinearUnits`.

Returns
-------
str

Examples
--------
>>> vt_sp = osr.SpatialReference()
>>> vt_sp.ImportFromEPSG(5646)
0
>>> vt_sp.GetLinearUnitsName()
'US survey foot'

";

%feature("docstring") GetName "

Return the CRS name.

See :cpp:func:`OGRSpatialReference::GetName`.

Returns
-------
str

Examples
--------
>>> vt_sp = osr.SpatialReference()
>>> vt_sp.ImportFromEPSG(5646)
0
>>> vt_sp.GetName()
'NAD83 / Vermont (ftUS)'

";

%feature("docstring") GetNormProjParm "

Fetch a normalized projection parameter value.

This method is the same as :py:meth:`GetProjParm` except that the value of the
parameter is normalized into degrees or meters depending on whether it is
linear or angular.

See :cpp:func:`OGRSpatialReference::GetNormProjParm`.

Parameters
----------
name : str
    parameter name, available as constants prefixed with `SRS_PP`.
default_val : float, default = 0.0
    value to return if this parameter doesn't exist

Returns
-------
float

Examples
--------
>>> vt_sp = osr.SpatialReference()
>>> vt_sp.ImportFromEPSG(5646)
0
>>> vt_sp.GetProjParm(osr.SRS_PP_FALSE_EASTING)
1640416.6667
>>> vt_sp.GetNormProjParm(osr.SRS_PP_FALSE_EASTING)
500000.0000101601

";

%feature("docstring") GetProjParm "

Fetch a projection parameter value.

See :cpp:func:`OGRSpatialReference::GetProjParm`.

Parameters
----------
name : str
    parameter name, available as constants prefixed with `SRS_PP`.
default_val : float, default = 0.0
    value to return if this parameter doesn't exist

Returns
-------
float

Examples
--------
>>> vt_sp = osr.SpatialReference()
>>> vt_sp.ImportFromEPSG(5646)
0
>>> vt_sp.GetProjParm(osr.SRS_PP_FALSE_EASTING)
1640416.6667
>>> vt_sp.GetProjParm(osr.SRS_PP_FALSE_NORTHING)
0.0

";

%feature("docstring") GetSemiMajor "

Get spheroid semi major axis (in meters starting with GDAL 3.0)

See :cpp:func:`OGRSpatialReference::GetSemiMajor`.

Returns
-------
float
    semi-major axis, or :py:const:`SRS_WGS84_SEMIMAJOR` if it cannot be found.
";

%feature("docstring") GetSemiMinor "

Get spheroid semi minor axis.

See :cpp:func:`OGRSpatialReference::GetSemiMinor`.

Returns
-------
float
    semi-minor axis, or :py:const:`SRS_WGS84_SEMIMINOR` if it cannot be found.
";

%feature("docstring") GetTOWGS84 "

Fetch TOWGS84 parameter, if available.

See :cpp:func:`OGRSpatialReference::GetTOWGS84`.

Returns
-------
tuple

";

%feature("docstring") GetTargetLinearUnits "

Fetch linear units for a target.

See :cpp:func:`OGRSpatialReference::GetTargetLinearUnits`.

Parameters
----------
target_key : str
    key to look un, such as 'PROJCS' or 'VERT_CS'

Returns
-------
double

";

%feature("docstring") GetUTMZone "

Get UTM zone.

See :cpp:func:`OGRSpatialReference::GetUTMZone`.

Returns
-------
int
    UTM zone number. Negative in the southern hemisphere and positive in the northern hemisphere. If the SRS is not UTM, zero will be returned.

";

%feature("docstring") HasPointMotionOperation "

Check if a CRS has an associated point motion operation.

See :cpp:func:`OGRSpatialReference::HasPointMotionOperation`.

Returns
-------
bool

";

%feature("docstring") HasTOWGS84 "

Return whether the SRS has a TOWGS84 parameter.

See :cpp:func:`OGRSpatialReference::GetTOWGS84`.

Returns
-------
bool

";

%feature("docstring") ImportFromEPSG "

Initialize SRS based on EPSG geographic, projected or vertical CRS code.

Since GDAL 3.0, this method is identical to :py:meth:`ImportFromEPSGA`.

See :cpp:func:`OGRSpatialReference::importFromEPSG`.

Parameters
----------
arg : int
    EPSG code to search in PROJ database

Returns
-------
int
    :py:const:`OGRERR_NONE` on success, or an error code on failure

Examples
--------
>>> srs = osr.SpatialReference()
>>> srs.ImportFromEPSG(4326)
0

";

%feature("docstring") ImportFromEPSGA "

Initialize SRS based on EPSG geographic, projected or vertical CRS code.

Since GDAL 3.0, this method is identical to :py:meth:`ImportFromEPSG`.

See :cpp:func:`OGRSpatialReference::importFromEPSGA`.

Parameters
----------
arg : int
    EPSG code to search in PROJ database

Returns
-------
int
    :py:const:`OGRERR_NONE` on success, or an error code on failure

";

%feature("docstring") ImportFromProj4 "

Initialize SRS based on PROJ coordinate string.

See :cpp:func:`OGRSpatialReference::importFromProj4`.

Parameters
----------
ppszInput : str
    PROJ coordinate string

Returns
-------
int
    :py:const:`OGRERR_NONE` on success, or :py:const:`OGRERR_CORRUPT_DATA` on failure

Examples
--------
>>> srs = osr.SpatialReference()
>>> srs.ImportFromProj4('+proj=utm +zone=18 +datum=WGS84')
0
";

%feature("docstring") ImportFromUrl "

Initialize SRS based on a URL.

This method will download the spatial reference at a given URL and
feed it into :py:meth:`SetFromUserInput` for you.

See :cpp:func:`OGRSpatialReference::importFromUrl`.

Parameters
----------
url : str

Returns
-------
int
    :py:const:`OGRERR_NONE` on success, or an error code on failure

";

%feature("docstring") ImportFromWkt "

Import from WKT string.

See :cpp:func:`OGRSpatialReference::importFromWkt`.

Parameters
----------
ppszInput : str
    WKT string

Returns
-------
int
    :py:const:`OGRERR_NONE` if import succeeds, or :py:const:`OGRERR_CORRUPT_DATA` if it fails for any reason.

";

%feature("docstring") IsCompound "

Check if this CRS is a compound CRS.

See :cpp:func:`OGRSpatialReference::IsCompound`.

Returns
-------
int
    1 if the CRS is compound, 0 otherwise
";

%feature("docstring") IsDerivedGeographic "

Check if this CRS is a derived geographic CRS, such as a rotated long/lat grid.

See :cpp:func:`OGRSpatialReference::IsDerivedGeographic`.

Returns
-------
int
    1 if the CRS is derived geographic, 0 otherwise
";

%feature("docstring") IsDynamic "

Check if this CRS is a dynamic coordinate CRS.

See :cpp:func:`OGRSpatialReference::IsDynamic`.

Returns
-------
bool
";

%feature("docstring") IsGeocentric "

Check if this SRS is a geocentric coordinate system.

See :cpp:func:`OGRSpatialReference::IsGeocentric`.

Returns
-------
int
    1 if the SRS is geocentric, 0 otherwise
";

%feature("docstring") IsGeographic "

Check if this SRS is a geographic coordinate system.

See :cpp:func:`OGRSpatialReference::IsGeographic`.

Returns
-------
int
    1 if the SRS is geographic, 0 otherwise
";

%feature("docstring") IsLocal "

Check if this CRS is a local CRS.

See :cpp:func:`OGRSpatialReference::IsLocal`.

Returns
-------
int
    1 if the SRS is local, 0 otherwise
";

%feature("docstring") IsProjected "

Check if this SRS is a projected coordinate system.

See :cpp:func:`OGRSpatialReference::IsProjected`.

Returns
-------
int
    1 if the SRS is projected, 0 otherwise
";

%feature("docstring") IsSame "

Determine if two spatial references describe the same system.

See :cpp:func:`OGRSpatialReference::IsSame`.

Parameters
----------
rhs : SpatialReference
options : list/dict

Returns
-------
int
    1 if the spatial references describe the same system, 0 otherwise

";

%feature("docstring") IsSameGeogCS "

Determine if two spatial references share the same geographic coordinate system.

See :cpp:func:`OGRSpatialReference::IsSameGeogCS`.

Parameters
----------
rhs : SpatialReference
options : list/dict

Returns
-------
int
    1 if the spatial references have the same GeogCS, 0 otherwise

";

%feature("docstring") IsSameVertCS "

Determine if two spatial references share the same vertical coordinate system.

See :cpp:func:`OGRSpatialReference::IsSameVertCS`.

Parameters
----------
rhs : SpatialReference
options : list/dict

Returns
-------
int
    1 if the spatial references have the same VertCS, 0 otherwise

";


%feature("docstring") IsVertical "

Check if this is a vertical coordinate system.

See :cpp:func:`OGRSpatialReference::IsVertical`.

Returns
-------
int
    1 if the CRS is vertical, 0 otherwise

";

}


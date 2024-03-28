%extend OGRLayerShadow {
// File: ogrlayer_8cpp.xml
%feature("docstring")  Reference "
For more details: :cpp:func:`OGR_L_Reference`
";

%feature("docstring")  Dereference "
For more details: :cpp:func:`OGR_L_Dereference`
";

%feature("docstring")  GetRefCount "
For more details: :cpp:func:`OGR_L_GetRefCount`
";

%feature("docstring")  GetFeatureCount "
Fetch the feature count in this layer.

For more details: :cpp:func:`OGR_L_GetFeatureCount`

Parameters
-----------
force: int
    Flag indicating whether the count should be computed even if
    it is expensive.

Returns
--------
int:
    Feature count, -1 if count not known.
";

%feature("docstring")  GetExtent "
Fetch the extent of this layer.

For more details:

- :cpp:func:`OGR_L_GetExtent`
- :cpp:func:`OGR_L_GetExtentEx`

.. warning:: Check the return order of the bounds.

Parameters
-----------
force: int, default=False
    Flag indicating whether the extent should be computed even if
    it is expensive.
can_return_null: int, default=False
    Whether None can be returned in the response.
geom_field: int, default=0
    Ithe index of the geometry field on which to compute the extent.
    Can be iterated over using :py:func:`range` and :py:func:`GetGeomFieldCount`.

Returns
--------
minx: float
maxx: float
miny: float
maxy: float
";


%feature("docstring")  SetAttributeFilter "
Set a new attribute query.

For more details: :cpp:func:`OGR_L_SetAttributeFilter`

Parameters
-----------
filter_string: str
    query in restricted SQL WHERE format, or None to clear the
    current query.

Returns
--------
int:
    :py:const:`osgeo.ogr.OGRERR_NONE` if successfully installed,
    or an error code if the query expression is in error,
    or some other failure occurs.
";

%feature("docstring")  GetFeature "
Fetch a feature by its identifier.

For more details: :cpp:func:`OGR_L_GetFeature`

Use :py:func:`TestCapability` with (:py:const:`osgeo.ogr.OLCRandomRead`)
to establish if this layer supports efficient random access reading via
:py:func:`GetFeature`; However, the call should always work if the feature exists.

Sequential reads (with :py:func:`GetNextFeature`) are generally
considered interrupted by a :py:func:`GetFeature` call.

Parameters
-----------
fid: int
    The feature id of the feature to read.

Returns
--------
Feature:
    A new feature now owned by the caller, or None on failure.
    The returned feature should be deleted with :py:func:`Destroy`.
";

%feature("docstring")  SetNextByIndex "
Move read cursor to the nIndex'th feature in the current resultset.

For more details: :cpp:func:`OGR_L_SetNextByIndex`

Parameters
-----------
new_index: int
    The index indicating how many steps into the result set to seek.

Returns
--------
int:
    :py:const:`osgeo.ogr.OGRERR_NONE` on success or an error code.
";

%feature("docstring")  GetNextFeature "
Fetch the next available feature from this layer.

For more details: :cpp:func:`OGR_L_GetNextFeature`

Returns
--------
Feature:
    A feature or None if no more features are available.
";

%feature("docstring")  SetFeature "
Rewrite an existing feature.

For more details: :cpp:func:`OGR_L_SetFeature`

To set a feature, but create it if it doesn't exist see :py:meth:`.Layer.UpsertFeature`.

Parameters
-----------
feature: Feature
    The feature to write.

Returns
--------
int:
    :py:const:`osgeo.ogr.OGRERR_NONE` if the operation works,
    otherwise an appropriate error code
    (e.g :py:const:`osgeo.ogr.OGRERR_NON_EXISTING_FEATURE` if the
    feature does not exist).
";

%feature("docstring")  CreateFeature "
Create and write a new feature within a layer.

For more details: :cpp:func:`OGR_L_CreateFeature`

To create a feature, but set it if it exists see :py:meth:`.Layer.UpsertFeature`.

Parameters
-----------
feature: Feature
    The feature to write to disk.

Returns
--------
int:
    :py:const:`osgeo.ogr.OGRERR_NONE` on success.
";

%feature("docstring")  UpsertFeature "
Rewrite an existing feature or create a new feature within a layer.

For more details: :cpp:func:`OGR_L_UpsertFeature`

Parameters
-----------
feature: Feature
    The feature to write to disk.

Returns
--------
int:
    :py:const:`osgeo.ogr.OGRERR_NONE` on success.
";

%feature("docstring")  CreateField "
Create a new field on a layer.

For more details: :cpp:func:`OGR_L_CreateField`

Parameters
-----------
field_def: FieldDefn
    The field definition to write to disk.
approx_ok: bool, default=True
    If True, the field may be created in a slightly different
    form depending on the limitations of the format driver.

Returns
--------
int:
    :py:const:`osgeo.ogr.OGRERR_NONE` on success.
";

%feature("docstring")  DeleteField "
Delete an existing field on a layer.

For more details: :cpp:func:`OGR_L_DeleteField`

Parameters
-----------
iField: int
    index of the field to delete.

Returns
--------
int:
    :py:const:`osgeo.ogr.OGRERR_NONE` on success.
";

%feature("docstring")  ReorderFields "
Reorder all the fields of a layer.

For more details: :cpp:func:`OGR_L_ReorderFields`

Parameters
-----------
nList: list[int]
    A list of GetLayerDefn().GetFieldCount()
    elements which is a permutation of
    [0, GetLayerDefn().GetFieldCount()-1].

Returns
--------
int:
    :py:const:`osgeo.ogr.OGRERR_NONE` on success.
";

%feature("docstring")  ReorderField "
Reorder an existing field on a layer.

For more details: :cpp:func:`OGR_L_ReorderField`

Parameters
-----------
iOldFieldPos: int
    previous position of the field to move. Must be in the
    range [0,GetFieldCount()-1].
iNewFieldPos: int
    new position of the field to move. Must be in the range
    [0,GetFieldCount()-1].

Returns
--------
int:
    :py:const:`osgeo.ogr.OGRERR_NONE` on success.
";

%feature("docstring")  AlterFieldDefn "
Alter the definition of an existing field on a layer.

For more details: :cpp:func:`OGR_L_AlterFieldDefn`

Parameters
-----------
iField: int
    index of the field whose definition must be altered.
field_def: FieldDefn
    new field definition
nFlags: int
    Combination of
    :py:const:`osgeo.ogr.ALTER_NAME_FLAG`,
    :py:const:`osgeo.ogr.ALTER_TYPE_FLAG`,
    :py:const:`osgeo.ogr.ALTER_WIDTH_PRECISION_FLAG`,
    :py:const:`osgeo.ogr.ALTER_NULLABLE_FLAG` and
    :py:const:`osgeo.ogr.ALTER_DEFAULT_FLAG`
    to indicate which of the name and/or type and/or width and precision
    fields and/or nullability from the new field definition must be taken
    into account.

Returns
--------
int:
    :py:const:`osgeo.ogr.OGRERR_NONE` on success.
";

%feature("docstring")  CreateGeomField "
Create a new geometry field on a layer.

For more details: :cpp:func:`OGR_L_CreateGeomField`

Parameters
-----------
field_def: GeomFieldDefn
    The geometry field definition to write to disk.
approx_ok: bool, default=True
    If True, the field may be created in a slightly different
    form depending on the limitations of the format driver.

Returns
--------
int:
    :py:const:`osgeo.ogr.OGRERR_NONE` on success.
";

%feature("docstring")  StartTransaction "
For datasources which support transactions, this creates a transaction.

For more details: :cpp:func:`OGR_L_StartTransaction`

Returns
--------
int:
    :py:const:`osgeo.ogr.OGRERR_NONE` on success.
";

%feature("docstring")  CommitTransaction "
For datasources which support transactions, this commits a transaction.

For more details: :cpp:func:`OGR_L_CommitTransaction`

Returns
--------
int:
    :py:const:`osgeo.ogr.OGRERR_NONE` on success.
";

%feature("docstring")  RollbackTransaction "
Roll back a datasource to its state before the start of the current transaction.

For more details: :cpp:func:`OGR_L_RollbackTransaction`

Returns
--------
int:
    :py:const:`osgeo.ogr.OGRERR_NONE` on success.
";

%feature("docstring")  GetLayerDefn "
Fetch the schema information for this layer.

For more details: :cpp:func:`OGR_L_GetLayerDefn`

Returns
--------
FeatureDefn:
    The feature definition.
";

%feature("docstring")  FindFieldIndex "
Find the index of field in a layer.

For more details: :cpp:func:`OGR_L_FindFieldIndex`

Returns
--------
int:
    field index, or -1 if the field doesn't exist
";

%feature("docstring")  GetSpatialRef "
Fetch the spatial reference system for this layer.

For more details: :cpp:func:`OGR_L_GetSpatialRef`

Returns
--------
SpatialReference:
    spatial reference, or None if there isn't one.
";

%feature("docstring")  TestCapability "
Test if this layer supported the named capability.

For more details: :cpp:func:`OGR_L_TestCapability`

Parameters
-----------
cap: str
    The name of the capability to test. These can
    be found in the `osgeo.ogr` namespace. For example,
    :py:const:`osgeo.ogr.OLCRandomRead`.

Returns
--------
int:
    True if the layer has the requested capability, or False otherwise.
    Will return False for any unrecognized capabilities.
";

%feature("docstring")  GetSpatialFilter "
This function returns the current spatial filter for this layer.

For more details: :cpp:func:`OGR_L_GetSpatialFilter`

Returns
--------
Geometry:
    The spatial filter geometry.
";

%feature("docstring")  SetSpatialFilter "
Set a new spatial filter.

For more details:

- :cpp:func:`OGR_L_SetSpatialFilter`
- :cpp:func:`OGR_L_SetSpatialFilterEx`

Parameters
-----------
iGeomField: int, optional
    index of the geometry field on which the spatial filter operates.
filter: Geometry
    The geometry to use as a filtering region. None may
    be passed indicating that the current spatial filter should be
    cleared, but no new one instituted.
";


%feature("docstring")  SetSpatialFilterRect "
Set a new rectangular spatial filter.

For more details:

- :cpp:func:`OGR_L_SetSpatialFilterRect`
- :cpp:func:`OGR_L_SetSpatialFilterRectEx`

Parameters
-----------
iGeomField: int, optional
    index of the geometry field on which the spatial filter operates.
minx: float
    the minimum X coordinate for the rectangular region.
miny: float
    the minimum Y coordinate for the rectangular region.
maxx: float
    the maximum X coordinate for the rectangular region.
maxy: float
    the maximum Y coordinate for the rectangular region.
";


%feature("docstring")  ResetReading "
Reset feature reading to start on the first feature.

For more details: :cpp:func:`OGR_L_ResetReading`
";

%feature("docstring")  SyncToDisk "
Flush pending changes to disk.

For more details: :cpp:func:`OGR_L_SyncToDisk`

Returns
--------
int:
    :py:const:`osgeo.ogr.OGRERR_NONE` if no error occurs
    (even if nothing is done) or an error code.
";

%feature("docstring")  DeleteFeature "
Delete feature from layer.

For more details: :cpp:func:`OGR_L_DeleteFeature`

Parameters
-----------
fid: int
    The feature id to be deleted from the layer

Returns
--------
int:
    :py:const:`osgeo.ogr.OGRERR_NONE` if the operation works,
    otherwise an appropriate error code
    (e.g :py:const:`osgeo.ogr.OGRERR_NON_EXISTING_FEATURE`)
    if the feature does not exist.
";

%feature("docstring")  GetFeaturesRead "
For more details: :cpp:func:`OGR_L_GetFeaturesRead`
";

%feature("docstring")  GetFIDColumn "
This method returns the name of the underlying database column being
used as the FID column, or \'\' if not supported.

For more details: :cpp:func:`OGR_L_GetFIDColumn`

Returns
--------
str:
    fid column name.
";

%feature("docstring")  GetGeometryColumn "
This method returns the name of the underlying database column being
used as the geometry column, or \'\' if not supported.

For more details: :cpp:func:`OGR_L_GetGeometryColumn`

Returns
--------
str:
    geometry column name.
";

%feature("docstring")  GetStyleTable "
Get style table.

For more details: :cpp:func:`OGR_L_GetStyleTable`
";

%feature("docstring")  SetStyleTableDirectly "
Set style table (and take ownership)

For more details: :cpp:func:`OGR_L_SetStyleTableDirectly`
";

%feature("docstring")  SetStyleTable "
Set style table.

For more details: :cpp:func:`OGR_L_SetStyleTable`
";

%feature("docstring")  GetName "
Return the layer name.

For more details: :cpp:func:`OGR_L_GetName`

Returns
--------
str:
    The layer name
";

%feature("docstring")  GetGeomType "
Return the layer geometry type.

For more details: :cpp:func:`OGR_L_GetGeomType`

Returns
--------
int:
    The geometry type code. The types can be found with
    'osgeo.ogr.wkb' prefix. For example :py:const:`osgeo.ogr.wkbPolygon`.
";

%feature("docstring")  SetIgnoredFields "
Set which fields can be omitted when retrieving features from the
layer.

For more details: :cpp:func:`OGR_L_SetIgnoredFields`

Parameters
-----------
options: list[str]
    A list of field names.
    If an empty list is passed, the ignored list is cleared.

Returns
-------
int:
    :py:const:`osgeo.ogr.OGRERR_NONE` if all field names have been resolved
    (even if the driver does not support this method)
";


%feature("docstring")  Intersection "
Intersection of two layers.

For more details: :cpp:func:`OGR_L_Intersection`

Parameters
-----------
method_layer: Layer
    the method layer. Should not be None.
result_layer: Layer
    the layer where the features resulting from the
    operation are inserted. Should not be None.
options: list[str], optional
    List of options (empty list is allowed). For example [\"PROMOTE_TO_MULTI=YES\"].
callback: Callable, optional
    a GDALProgressFunc() compatible callback function for
    reporting progress or None.
callback_data:
    Argument to be passed to 'callback'. May be None.

Returns
-------
int:
    An error code if there was an error or the execution was interrupted,
    :py:const:`osgeo.ogr.OGRERR_NONE` otherwise.
";

%feature("docstring")  Union "
Union of two layers.

For more details: :cpp:func:`OGR_L_Union`

The first geometry field is always used.

Parameters
-----------
method_layer: Layer
    the method layer. Should not be None.
result_layer: Layer
    the layer where the features resulting from the
    operation are inserted. Should not be None.
options: list[str], optional
    List of options (empty list is allowed). For example [\"PROMOTE_TO_MULTI=YES\"].
callback: Callable, optional
    a GDALProgressFunc() compatible callback function for
    reporting progress or None.
callback_data:
    Argument to be passed to 'callback'. May be None.

Returns
-------
int:
    An error code if there was an error or the execution was interrupted,
    :py:const:`osgeo.ogr.OGRERR_NONE` otherwise.
";

%feature("docstring")  SymDifference "
Symmetrical difference of two layers.

For more details: :cpp:func:`OGR_L_SymDifference`

Parameters
-----------
method_layer: Layer
    the method layer. Should not be None.
result_layer: Layer
    the layer where the features resulting from the
    operation are inserted. Should not be None.
options: list[str], optional
    List of options (empty list is allowed). For example [\"PROMOTE_TO_MULTI=YES\"].
callback: Callable, optional
    a GDALProgressFunc() compatible callback function for
    reporting progress or None.
callback_data:
    Argument to be passed to 'callback'. May be None.

Returns
-------
int:
    An error code if there was an error or the execution was interrupted,
    :py:const:`osgeo.ogr.OGRERR_NONE` otherwise.
";

%feature("docstring")  Identity "
Identify the features of this layer with the ones from the identity layer.

For more details: :cpp:func:`OGR_L_Identity`

Parameters
-----------
method_layer: Layer
    the method layer. Should not be None.
result_layer: Layer
    the layer where the features resulting from the
    operation are inserted. Should not be None.
options: list[str], optional
    List of options (empty list is allowed). For example [\"PROMOTE_TO_MULTI=YES\"].
callback: Callable, optional
    a GDALProgressFunc() compatible callback function for
    reporting progress or None.
callback_data:
    Argument to be passed to 'callback'. May be None.

Returns
-------
int:
    An error code if there was an error or the execution was interrupted,
    :py:const:`osgeo.ogr.OGRERR_NONE` otherwise.
";

%feature("docstring")  Update "
Update this layer with features from the update layer.

For more details: :cpp:func:`OGR_L_Update`

Parameters
-----------
method_layer: Layer
    the method layer. Should not be None.
result_layer: Layer
    the layer where the features resulting from the
    operation are inserted. Should not be None.
options: list[str], optional
    List of options (empty list is allowed). For example [\"PROMOTE_TO_MULTI=YES\"].
callback: Callable, optional
    a GDALProgressFunc() compatible callback function for
    reporting progress or None.
callback_data:
    Argument to be passed to 'callback'. May be None.

Returns
-------
int:
    An error code if there was an error or the execution was interrupted,
    :py:const:`osgeo.ogr.OGRERR_NONE` otherwise.
";

%feature("docstring")  Clip "
Clip off areas that are not covered by the method layer.

For more details: :cpp:func:`OGR_L_Clip`

Parameters
-----------
method_layer: Layer
    the method layer. Should not be None.
result_layer: Layer
    the layer where the features resulting from the
    operation are inserted. Should not be None.
options: list[str], optional
    List of options (empty list is allowed). For example [\"PROMOTE_TO_MULTI=YES\"].
callback: Callable, optional
    a GDALProgressFunc() compatible callback function for
    reporting progress or None.
callback_data:
    Argument to be passed to 'callback'. May be None.

Returns
-------
int:
    An error code if there was an error or the execution was interrupted,
    :py:const:`osgeo.ogr.OGRERR_NONE` otherwise.
";

%feature("docstring")  Erase "
Remove areas that are covered by the method layer.

For more details: :cpp:func:`OGR_L_Erase`

Parameters
-----------
method_layer: Layer
    the method layer. Should not be None.
result_layer: Layer
    the layer where the features resulting from the
    operation are inserted. Should not be None.
options: list[str], optional
    List of options (empty list is allowed). For example [\"PROMOTE_TO_MULTI=YES\"].
callback: Callable, optional
    a GDALProgressFunc() compatible callback function for
    reporting progress or None.
callback_data:
    Argument to be passed to 'callback'. May be None.

Returns
-------
int:
    An error code if there was an error or the execution was interrupted,
    :py:const:`osgeo.ogr.OGRERR_NONE` otherwise.
";

%feature("docstring")  GetGeometryTypes "
Get actual geometry types found in features.

For more details: :cpp:func:`OGR_L_GetGeometryTypes`

Parameters
-----------
geom_field: int, optional
    index of the geometry field
flags: int, optional
    0, or a combination of :py:const:`osgeo.ogr.GGT_COUNT_NOT_NEEDED`,
    :py:const:`osgeo.ogr.GGT_STOP_IF_MIXED` and
    :py:const:`osgeo.ogr.GGT_GEOMCOLLECTIONZ_TINZ`
callback: Callable, optional
    a GDALProgressFunc() compatible callback function for
    cancellation or None.
callback_data:
    Argument to be passed to 'callback'. May be None.

Returns
-------
dict:
    A dictionary whose keys are :py:const:`osgeo.ogr.wkbXXXX` constants and
    values the corresponding number of geometries of that type in the layer.
";

%feature("docstring")  GetDataset "
Return the dataset associated with this layer.

For more details: :cpp:func:`OGR_L_GetDataset`

Returns
-------
Dataset:
    Dataset or None
";

}

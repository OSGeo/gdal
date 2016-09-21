%extend OGRLayerShadow {
// File: ogrlayer_8cpp.xml
%feature("docstring")  CPL_CVSID "CPL_CVSID(\"$Id: ogrlayer.cpp 33568
2016-02-26 21:01:54Z rouault $\") ";

%feature("docstring")  Reference "int OGR_L_Reference(OGRLayerH
hLayer) ";

%feature("docstring")  Dereference "int OGR_L_Dereference(OGRLayerH
hLayer) ";

%feature("docstring")  GetRefCount "int OGR_L_GetRefCount(OGRLayerH
hLayer) ";

%feature("docstring")  GetFeatureCount "GIntBig
OGR_L_GetFeatureCount(OGRLayerH hLayer, int bForce)

Fetch the feature count in this layer.

Returns the number of features in the layer. For dynamic databases the
count may not be exact. If bForce is FALSE, and it would be expensive
to establish the feature count a value of -1 may be returned
indicating that the count isn't know. If bForce is TRUE some
implementations will actually scan the entire layer once to count
objects.

The returned count takes the spatial filter into account.

Note that some implementations of this method may alter the read
cursor of the layer.

This function is the same as the CPP OGRLayer::GetFeatureCount().

Note: since GDAL 2.0, this method returns a GIntBig (previously a int)

Parameters:
-----------

hLayer:  handle to the layer that owned the features.

bForce:  Flag indicating whether the count should be computed even if
it is expensive.

feature count, -1 if count not known. ";

%feature("docstring")  GetExtent "OGRErr OGR_L_GetExtent(OGRLayerH
hLayer, OGREnvelope *psExtent, int bForce)

Fetch the extent of this layer.

Returns the extent (MBR) of the data in the layer. If bForce is FALSE,
and it would be expensive to establish the extent then OGRERR_FAILURE
will be returned indicating that the extent isn't know. If bForce is
TRUE then some implementations will actually scan the entire layer
once to compute the MBR of all the features in the layer.

Depending on the drivers, the returned extent may or may not take the
spatial filter into account. So it is safer to call OGR_L_GetExtent()
without setting a spatial filter.

Layers without any geometry may return OGRERR_FAILURE just indicating
that no meaningful extents could be collected.

Note that some implementations of this method may alter the read
cursor of the layer.

This function is the same as the C++ method OGRLayer::GetExtent().

Parameters:
-----------

hLayer:  handle to the layer from which to get extent.

psExtent:  the structure in which the extent value will be returned.

bForce:  Flag indicating whether the extent should be computed even if
it is expensive.

OGRERR_NONE on success, OGRERR_FAILURE if extent not known. ";

%feature("docstring")  GetExtentEx "OGRErr
OGR_L_GetExtentEx(OGRLayerH hLayer, int iGeomField, OGREnvelope
*psExtent, int bForce)

Fetch the extent of this layer, on the specified geometry field.

Returns the extent (MBR) of the data in the layer. If bForce is FALSE,
and it would be expensive to establish the extent then OGRERR_FAILURE
will be returned indicating that the extent isn't know. If bForce is
TRUE then some implementations will actually scan the entire layer
once to compute the MBR of all the features in the layer.

Depending on the drivers, the returned extent may or may not take the
spatial filter into account. So it is safer to call OGR_L_GetExtent()
without setting a spatial filter.

Layers without any geometry may return OGRERR_FAILURE just indicating
that no meaningful extents could be collected.

Note that some implementations of this method may alter the read
cursor of the layer.

This function is the same as the C++ method OGRLayer::GetExtent().

Parameters:
-----------

hLayer:  handle to the layer from which to get extent.

iGeomField:  the index of the geometry field on which to compute the
extent.

psExtent:  the structure in which the extent value will be returned.

bForce:  Flag indicating whether the extent should be computed even if
it is expensive.

OGRERR_NONE on success, OGRERR_FAILURE if extent not known. ";

%feature("docstring")  ContainGeomSpecialField "static int
ContainGeomSpecialField(swq_expr_node *expr, int nLayerFieldCount) ";

%feature("docstring")  SetAttributeFilter "OGRErr
OGR_L_SetAttributeFilter(OGRLayerH hLayer, const char *pszQuery)

Set a new attribute query.

This function sets the attribute query string to be used when fetching
features via the OGR_L_GetNextFeature() function. Only features for
which the query evaluates as true will be returned.

The query string should be in the format of an SQL WHERE clause. For
instance \"population > 1000000 and population < 5000000\" where
population is an attribute in the layer. The query format is a
restricted form of SQL WHERE clause as defined
\"eq_format=restricted_where\" about half way through this document:

http://ogdi.sourceforge.net/prop/6.2.CapabilitiesMetadata.html

Note that installing a query string will generally result in resetting
the current reading position (ala OGR_L_ResetReading()).

This function is the same as the C++ method
OGRLayer::SetAttributeFilter().

Parameters:
-----------

hLayer:  handle to the layer on which attribute query will be
executed.

pszQuery:  query in restricted SQL WHERE format, or NULL to clear the
current query.

OGRERR_NONE if successfully installed, or an error code if the query
expression is in error, or some other failure occurs. ";

%feature("docstring")  GetFeature "OGRFeatureH
OGR_L_GetFeature(OGRLayerH hLayer, GIntBig nFeatureId)

Fetch a feature by its identifier.

This function will attempt to read the identified feature. The nFID
value cannot be OGRNullFID. Success or failure of this operation is
unaffected by the spatial or attribute filters (and specialized
implementations in drivers should make sure that they do not take into
account spatial or attribute filters).

If this function returns a non-NULL feature, it is guaranteed that its
feature id ( OGR_F_GetFID()) will be the same as nFID.

Use OGR_L_TestCapability(OLCRandomRead) to establish if this layer
supports efficient random access reading via OGR_L_GetFeature();
however, the call should always work if the feature exists as a
fallback implementation just scans all the features in the layer
looking for the desired feature.

Sequential reads (with OGR_L_GetNextFeature()) are generally
considered interrupted by a OGR_L_GetFeature() call.

The returned feature should be free with OGR_F_Destroy().

This function is the same as the C++ method OGRLayer::GetFeature( ).

Parameters:
-----------

hLayer:  handle to the layer that owned the feature.

nFeatureId:  the feature id of the feature to read.

an handle to a feature now owned by the caller, or NULL on failure. ";

%feature("docstring")  SetNextByIndex "OGRErr
OGR_L_SetNextByIndex(OGRLayerH hLayer, GIntBig nIndex)

Move read cursor to the nIndex'th feature in the current resultset.

This method allows positioning of a layer such that the
GetNextFeature() call will read the requested feature, where nIndex is
an absolute index into the current result set. So, setting it to 3
would mean the next feature read with GetNextFeature() would have been
the 4th feature to have been read if sequential reading took place
from the beginning of the layer, including accounting for spatial and
attribute filters.

Only in rare circumstances is SetNextByIndex() efficiently
implemented. In all other cases the default implementation which calls
ResetReading() and then calls GetNextFeature() nIndex times is used.
To determine if fast seeking is available on the current layer use the
TestCapability() method with a value of OLCFastSetNextByIndex.

This method is the same as the C++ method OGRLayer::SetNextByIndex()

Parameters:
-----------

hLayer:  handle to the layer

nIndex:  the index indicating how many steps into the result set to
seek.

OGRERR_NONE on success or an error code. ";

%feature("docstring")  GetNextFeature "OGRFeatureH
OGR_L_GetNextFeature(OGRLayerH hLayer)

Fetch the next available feature from this layer.

The returned feature becomes the responsibility of the caller to
delete with OGR_F_Destroy(). It is critical that all features
associated with an OGRLayer (more specifically an OGRFeatureDefn) be
deleted before that layer/datasource is deleted.

Only features matching the current spatial filter (set with
SetSpatialFilter()) will be returned.

This function implements sequential access to the features of a layer.
The OGR_L_ResetReading() function can be used to start at the
beginning again.

Features returned by OGR_GetNextFeature() may or may not be affected
by concurrent modifications depending on drivers. A guaranteed way of
seeing modifications in effect is to call OGR_L_ResetReading() on
layers where OGR_GetNextFeature() has been called, before reading
again. Structural changes in layers (field addition, deletion, ...)
when a read is in progress may or may not be possible depending on
drivers. If a transaction is committed/aborted, the current sequential
reading may or may not be valid after that operation and a call to
OGR_L_ResetReading() might be needed.

This function is the same as the C++ method
OGRLayer::GetNextFeature().

Parameters:
-----------

hLayer:  handle to the layer from which feature are read.

an handle to a feature, or NULL if no more features are available. ";

%feature("docstring")  SetFeature "OGRErr OGR_L_SetFeature(OGRLayerH
hLayer, OGRFeatureH hFeat)

Rewrite an existing feature.

This function will write a feature to the layer, based on the feature
id within the OGRFeature.

Use OGR_L_TestCapability(OLCRandomWrite) to establish if this layer
supports random access writing via OGR_L_SetFeature().

This function is the same as the C++ method OGRLayer::SetFeature().

Parameters:
-----------

hLayer:  handle to the layer to write the feature.

hFeat:  the feature to write.

OGRERR_NONE if the operation works, otherwise an appropriate error
code (e.g OGRERR_NON_EXISTING_FEATURE if the feature does not exist).
";

%feature("docstring")  CreateFeature "OGRErr
OGR_L_CreateFeature(OGRLayerH hLayer, OGRFeatureH hFeat)

Create and write a new feature within a layer.

The passed feature is written to the layer as a new feature, rather
than overwriting an existing one. If the feature has a feature id
other than OGRNullFID, then the native implementation may use that as
the feature id of the new feature, but not necessarily. Upon
successful return the passed feature will have been updated with the
new feature id.

This function is the same as the C++ method OGRLayer::CreateFeature().

Parameters:
-----------

hLayer:  handle to the layer to write the feature to.

hFeat:  the handle of the feature to write to disk.

OGRERR_NONE on success. ";

%feature("docstring")  CreateField "OGRErr
OGR_L_CreateField(OGRLayerH hLayer, OGRFieldDefnH hField, int
bApproxOK)

Create a new field on a layer.

You must use this to create new fields on a real layer. Internally the
OGRFeatureDefn for the layer will be updated to reflect the new field.
Applications should never modify the OGRFeatureDefn used by a layer
directly.

This function should not be called while there are feature objects in
existence that were obtained or created with the previous layer
definition.

Not all drivers support this function. You can query a layer to check
if it supports it with the OLCCreateField capability. Some drivers may
only support this method while there are still no features in the
layer. When it is supported, the existing features of the backing
file/database should be updated accordingly.

Drivers may or may not support not-null constraints. If they support
creating fields with not-null constraints, this is generally before
creating any feature to the layer.

This function is the same as the C++ method OGRLayer::CreateField().

Parameters:
-----------

hLayer:  handle to the layer to write the field definition.

hField:  handle of the field definition to write to disk.

bApproxOK:  If TRUE, the field may be created in a slightly different
form depending on the limitations of the format driver.

OGRERR_NONE on success. ";

%feature("docstring")  DeleteField "OGRErr
OGR_L_DeleteField(OGRLayerH hLayer, int iField)

Create a new field on a layer.

You must use this to delete existing fields on a real layer.
Internally the OGRFeatureDefn for the layer will be updated to reflect
the deleted field. Applications should never modify the OGRFeatureDefn
used by a layer directly.

This function should not be called while there are feature objects in
existence that were obtained or created with the previous layer
definition.

Not all drivers support this function. You can query a layer to check
if it supports it with the OLCDeleteField capability. Some drivers may
only support this method while there are still no features in the
layer. When it is supported, the existing features of the backing
file/database should be updated accordingly.

This function is the same as the C++ method OGRLayer::DeleteField().

Parameters:
-----------

hLayer:  handle to the layer.

iField:  index of the field to delete.

OGRERR_NONE on success.

OGR 1.9.0 ";

%feature("docstring")  ReorderFields "OGRErr
OGR_L_ReorderFields(OGRLayerH hLayer, int *panMap)

Reorder all the fields of a layer.

You must use this to reorder existing fields on a real layer.
Internally the OGRFeatureDefn for the layer will be updated to reflect
the reordering of the fields. Applications should never modify the
OGRFeatureDefn used by a layer directly.

This function should not be called while there are feature objects in
existence that were obtained or created with the previous layer
definition.

panMap is such that,for each field definition at position i after
reordering, its position before reordering was panMap[i].

For example, let suppose the fields were \"0\",\"1\",\"2\",\"3\",\"4\"
initially. ReorderFields([0,2,3,1,4]) will reorder them as
\"0\",\"2\",\"3\",\"1\",\"4\".

Not all drivers support this function. You can query a layer to check
if it supports it with the OLCReorderFields capability. Some drivers
may only support this method while there are still no features in the
layer. When it is supported, the existing features of the backing
file/database should be updated accordingly.

This function is the same as the C++ method OGRLayer::ReorderFields().

Parameters:
-----------

hLayer:  handle to the layer.

panMap:  an array of GetLayerDefn()-> OGRFeatureDefn::GetFieldCount()
elements which is a permutation of [0, GetLayerDefn()->
OGRFeatureDefn::GetFieldCount()-1].

OGRERR_NONE on success.

OGR 1.9.0 ";

%feature("docstring")  ReorderField "OGRErr
OGR_L_ReorderField(OGRLayerH hLayer, int iOldFieldPos, int
iNewFieldPos)

Reorder an existing field on a layer.

This function is a convenience wrapper of OGR_L_ReorderFields()
dedicated to move a single field.

You must use this to reorder existing fields on a real layer.
Internally the OGRFeatureDefn for the layer will be updated to reflect
the reordering of the fields. Applications should never modify the
OGRFeatureDefn used by a layer directly.

This function should not be called while there are feature objects in
existence that were obtained or created with the previous layer
definition.

The field definition that was at initial position iOldFieldPos will be
moved at position iNewFieldPos, and elements between will be shuffled
accordingly.

For example, let suppose the fields were \"0\",\"1\",\"2\",\"3\",\"4\"
initially. ReorderField(1, 3) will reorder them as
\"0\",\"2\",\"3\",\"1\",\"4\".

Not all drivers support this function. You can query a layer to check
if it supports it with the OLCReorderFields capability. Some drivers
may only support this method while there are still no features in the
layer. When it is supported, the existing features of the backing
file/database should be updated accordingly.

This function is the same as the C++ method OGRLayer::ReorderField().

Parameters:
-----------

hLayer:  handle to the layer.

iOldFieldPos:  previous position of the field to move. Must be in the
range [0,GetFieldCount()-1].

iNewFieldPos:  new position of the field to move. Must be in the range
[0,GetFieldCount()-1].

OGRERR_NONE on success.

OGR 1.9.0 ";

%feature("docstring")  AlterFieldDefn "OGRErr
OGR_L_AlterFieldDefn(OGRLayerH hLayer, int iField, OGRFieldDefnH
hNewFieldDefn, int nFlags)

Alter the definition of an existing field on a layer.

You must use this to alter the definition of an existing field of a
real layer. Internally the OGRFeatureDefn for the layer will be
updated to reflect the altered field. Applications should never modify
the OGRFeatureDefn used by a layer directly.

This function should not be called while there are feature objects in
existence that were obtained or created with the previous layer
definition.

Not all drivers support this function. You can query a layer to check
if it supports it with the OLCAlterFieldDefn capability. Some drivers
may only support this method while there are still no features in the
layer. When it is supported, the existing features of the backing
file/database should be updated accordingly. Some drivers might also
not support all update flags.

This function is the same as the C++ method
OGRLayer::AlterFieldDefn().

Parameters:
-----------

hLayer:  handle to the layer.

iField:  index of the field whose definition must be altered.

hNewFieldDefn:  new field definition

nFlags:  combination of ALTER_NAME_FLAG, ALTER_TYPE_FLAG,
ALTER_WIDTH_PRECISION_FLAG, ALTER_NULLABLE_FLAG and ALTER_DEFAULT_FLAG
to indicate which of the name and/or type and/or width and precision
fields and/or nullability from the new field definition must be taken
into account.

OGRERR_NONE on success.

OGR 1.9.0 ";

%feature("docstring")  CreateGeomField "OGRErr
OGR_L_CreateGeomField(OGRLayerH hLayer, OGRGeomFieldDefnH hField, int
bApproxOK)

Create a new geometry field on a layer.

You must use this to create new geometry fields on a real layer.
Internally the OGRFeatureDefn for the layer will be updated to reflect
the new field. Applications should never modify the OGRFeatureDefn
used by a layer directly.

This function should not be called while there are feature objects in
existence that were obtained or created with the previous layer
definition.

Not all drivers support this function. You can query a layer to check
if it supports it with the OLCCreateField capability. Some drivers may
only support this method while there are still no features in the
layer. When it is supported, the existing features of the backing
file/database should be updated accordingly.

Drivers may or may not support not-null constraints. If they support
creating fields with not-null constraints, this is generally before
creating any feature to the layer.

This function is the same as the C++ method OGRLayer::CreateField().

Parameters:
-----------

hLayer:  handle to the layer to write the field definition.

hField:  handle of the geometry field definition to write to disk.

bApproxOK:  If TRUE, the field may be created in a slightly different
form depending on the limitations of the format driver.

OGRERR_NONE on success.

OGR 1.11 ";

%feature("docstring")  StartTransaction "OGRErr
OGR_L_StartTransaction(OGRLayerH hLayer)

For datasources which support transactions, StartTransaction creates a
transaction.

If starting the transaction fails, will return OGRERR_FAILURE.
Datasources which do not support transactions will always return
OGRERR_NONE.

Note: as of GDAL 2.0, use of this API is discouraged when the dataset
offers dataset level transaction with GDALDataset::StartTransaction().
The reason is that most drivers can only offer transactions at dataset
level, and not layer level. Very few drivers really support
transactions at layer scope.

This function is the same as the C++ method
OGRLayer::StartTransaction().

Parameters:
-----------

hLayer:  handle to the layer

OGRERR_NONE on success. ";

%feature("docstring")  CommitTransaction "OGRErr
OGR_L_CommitTransaction(OGRLayerH hLayer)

For datasources which support transactions, CommitTransaction commits
a transaction.

If no transaction is active, or the commit fails, will return
OGRERR_FAILURE. Datasources which do not support transactions will
always return OGRERR_NONE.

This function is the same as the C++ method
OGRLayer::CommitTransaction().

Parameters:
-----------

hLayer:  handle to the layer

OGRERR_NONE on success. ";

%feature("docstring")  RollbackTransaction "OGRErr
OGR_L_RollbackTransaction(OGRLayerH hLayer)

For datasources which support transactions, RollbackTransaction will
roll back a datasource to its state before the start of the current
transaction.

If no transaction is active, or the rollback fails, will return
OGRERR_FAILURE. Datasources which do not support transactions will
always return OGRERR_NONE.

This function is the same as the C++ method
OGRLayer::RollbackTransaction().

Parameters:
-----------

hLayer:  handle to the layer

OGRERR_NONE on success. ";

%feature("docstring")  GetLayerDefn "OGRFeatureDefnH
OGR_L_GetLayerDefn(OGRLayerH hLayer)

Fetch the schema information for this layer.

The returned handle to the OGRFeatureDefn is owned by the OGRLayer,
and should not be modified or freed by the application. It
encapsulates the attribute schema of the features of the layer.

This function is the same as the C++ method OGRLayer::GetLayerDefn().

Parameters:
-----------

hLayer:  handle to the layer to get the schema information.

an handle to the feature definition. ";

%feature("docstring")  FindFieldIndex "int
OGR_L_FindFieldIndex(OGRLayerH hLayer, const char *pszFieldName, int
bExactMatch)

Find the index of field in a layer.

The returned number is the index of the field in the layers, or -1 if
the field doesn't exist.

If bExactMatch is set to FALSE and the field doesn't exists in the
given form the driver might apply some changes to make it match, like
those it might do if the layer was created (eg. like LAUNDER in the
OCI driver).

This method is the same as the C++ method OGRLayer::FindFieldIndex().

field index, or -1 if the field doesn't exist ";

%feature("docstring")  GetSpatialRef "OGRSpatialReferenceH
OGR_L_GetSpatialRef(OGRLayerH hLayer)

Fetch the spatial reference system for this layer.

The returned object is owned by the OGRLayer and should not be
modified or freed by the application.

This function is the same as the C++ method OGRLayer::GetSpatialRef().

Parameters:
-----------

hLayer:  handle to the layer to get the spatial reference from.

spatial reference, or NULL if there isn't one. ";

%feature("docstring")  TestCapability "int
OGR_L_TestCapability(OGRLayerH hLayer, const char *pszCap)

Test if this layer supported the named capability.

The capability codes that can be tested are represented as strings,
but #defined constants exists to ensure correct spelling. Specific
layer types may implement class specific capabilities, but this can't
generally be discovered by the caller.

OLCRandomRead / \"RandomRead\": TRUE if the GetFeature() method is
implemented in an optimized way for this layer, as opposed to the
default implementation using ResetReading() and GetNextFeature() to
find the requested feature id.

OLCSequentialWrite / \"SequentialWrite\": TRUE if the CreateFeature()
method works for this layer. Note this means that this particular
layer is writable. The same OGRLayer class may returned FALSE for
other layer instances that are effectively read-only.

OLCRandomWrite / \"RandomWrite\": TRUE if the SetFeature() method is
operational on this layer. Note this means that this particular layer
is writable. The same OGRLayer class may returned FALSE for other
layer instances that are effectively read-only.

OLCFastSpatialFilter / \"FastSpatialFilter\": TRUE if this layer
implements spatial filtering efficiently. Layers that effectively read
all features, and test them with the OGRFeature intersection methods
should return FALSE. This can be used as a clue by the application
whether it should build and maintain its own spatial index for
features in this layer.

OLCFastFeatureCount / \"FastFeatureCount\": TRUE if this layer can
return a feature count (via OGR_L_GetFeatureCount()) efficiently, i.e.
without counting the features. In some cases this will return TRUE
until a spatial filter is installed after which it will return FALSE.

OLCFastGetExtent / \"FastGetExtent\": TRUE if this layer can return
its data extent (via OGR_L_GetExtent()) efficiently, i.e. without
scanning all the features. In some cases this will return TRUE until a
spatial filter is installed after which it will return FALSE.

OLCFastSetNextByIndex / \"FastSetNextByIndex\": TRUE if this layer can
perform the SetNextByIndex() call efficiently, otherwise FALSE.

OLCCreateField / \"CreateField\": TRUE if this layer can create new
fields on the current layer using CreateField(), otherwise FALSE.

OLCCreateGeomField / \"CreateGeomField\": (GDAL >= 1.11) TRUE if this
layer can create new geometry fields on the current layer using
CreateGeomField(), otherwise FALSE.

OLCDeleteField / \"DeleteField\": TRUE if this layer can delete
existing fields on the current layer using DeleteField(), otherwise
FALSE.

OLCReorderFields / \"ReorderFields\": TRUE if this layer can reorder
existing fields on the current layer using ReorderField() or
ReorderFields(), otherwise FALSE.

OLCAlterFieldDefn / \"AlterFieldDefn\": TRUE if this layer can alter
the definition of an existing field on the current layer using
AlterFieldDefn(), otherwise FALSE.

OLCDeleteFeature / \"DeleteFeature\": TRUE if the DeleteFeature()
method is supported on this layer, otherwise FALSE.

OLCStringsAsUTF8 / \"StringsAsUTF8\": TRUE if values of OFTString
fields are assured to be in UTF-8 format. If FALSE the encoding of
fields is uncertain, though it might still be UTF-8.

OLCTransactions / \"Transactions\": TRUE if the StartTransaction(),
CommitTransaction() and RollbackTransaction() methods work in a
meaningful way, otherwise FALSE.

OLCCurveGeometries / \"CurveGeometries\": TRUE if this layer supports
writing curve geometries or may return such geometries. (GDAL 2.0).

This function is the same as the C++ method
OGRLayer::TestCapability().

Parameters:
-----------

hLayer:  handle to the layer to get the capability from.

pszCap:  the name of the capability to test.

TRUE if the layer has the requested capability, or FALSE otherwise.
OGRLayers will return FALSE for any unrecognized capabilities. ";

%feature("docstring")  GetSpatialFilter "OGRGeometryH
OGR_L_GetSpatialFilter(OGRLayerH hLayer)

This function returns the current spatial filter for this layer.

The returned pointer is to an internally owned object, and should not
be altered or deleted by the caller.

This function is the same as the C++ method
OGRLayer::GetSpatialFilter().

Parameters:
-----------

hLayer:  handle to the layer to get the spatial filter from.

an handle to the spatial filter geometry. ";

%feature("docstring")  SetSpatialFilter "void
OGR_L_SetSpatialFilter(OGRLayerH hLayer, OGRGeometryH hGeom)

Set a new spatial filter.

This function set the geometry to be used as a spatial filter when
fetching features via the OGR_L_GetNextFeature() function. Only
features that geometrically intersect the filter geometry will be
returned.

Currently this test is may be inaccurately implemented, but it is
guaranteed that all features who's envelope (as returned by
OGR_G_GetEnvelope()) overlaps the envelope of the spatial filter will
be returned. This can result in more shapes being returned that should
strictly be the case.

This function makes an internal copy of the passed geometry. The
passed geometry remains the responsibility of the caller, and may be
safely destroyed.

For the time being the passed filter geometry should be in the same
SRS as the layer (as returned by OGR_L_GetSpatialRef()). In the future
this may be generalized.

This function is the same as the C++ method
OGRLayer::SetSpatialFilter.

Parameters:
-----------

hLayer:  handle to the layer on which to set the spatial filter.

hGeom:  handle to the geometry to use as a filtering region. NULL may
be passed indicating that the current spatial filter should be
cleared, but no new one instituted. ";

%feature("docstring")  SetSpatialFilterEx "void
OGR_L_SetSpatialFilterEx(OGRLayerH hLayer, int iGeomField,
OGRGeometryH hGeom)

Set a new spatial filter.

This function set the geometry to be used as a spatial filter when
fetching features via the OGR_L_GetNextFeature() function. Only
features that geometrically intersect the filter geometry will be
returned.

Currently this test is may be inaccurately implemented, but it is
guaranteed that all features who's envelope (as returned by
OGR_G_GetEnvelope()) overlaps the envelope of the spatial filter will
be returned. This can result in more shapes being returned that should
strictly be the case.

This function makes an internal copy of the passed geometry. The
passed geometry remains the responsibility of the caller, and may be
safely destroyed.

For the time being the passed filter geometry should be in the same
SRS as the geometry field definition it corresponds to (as returned by
GetLayerDefn()->OGRFeatureDefn::GetGeomFieldDefn(iGeomField)->GetSpati
alRef()). In the future this may be generalized.

Note that only the last spatial filter set is applied, even if several
successive calls are done with different iGeomField values.

This function is the same as the C++ method
OGRLayer::SetSpatialFilter.

Parameters:
-----------

hLayer:  handle to the layer on which to set the spatial filter.

iGeomField:  index of the geometry field on which the spatial filter
operates.

hGeom:  handle to the geometry to use as a filtering region. NULL may
be passed indicating that the current spatial filter should be
cleared, but no new one instituted.

GDAL 1.11 ";

%feature("docstring")  SetSpatialFilterRect "void
OGR_L_SetSpatialFilterRect(OGRLayerH hLayer, double dfMinX, double
dfMinY, double dfMaxX, double dfMaxY)

Set a new rectangular spatial filter.

This method set rectangle to be used as a spatial filter when fetching
features via the OGR_L_GetNextFeature() method. Only features that
geometrically intersect the given rectangle will be returned.

The x/y values should be in the same coordinate system as the layer as
a whole (as returned by OGRLayer::GetSpatialRef()). Internally this
method is normally implemented as creating a 5 vertex closed
rectangular polygon and passing it to OGRLayer::SetSpatialFilter(). It
exists as a convenience.

The only way to clear a spatial filter set with this method is to call
OGRLayer::SetSpatialFilter(NULL).

This method is the same as the C++ method
OGRLayer::SetSpatialFilterRect().

Parameters:
-----------

hLayer:  handle to the layer on which to set the spatial filter.

dfMinX:  the minimum X coordinate for the rectangular region.

dfMinY:  the minimum Y coordinate for the rectangular region.

dfMaxX:  the maximum X coordinate for the rectangular region.

dfMaxY:  the maximum Y coordinate for the rectangular region. ";

%feature("docstring")  SetSpatialFilterRectEx "void
OGR_L_SetSpatialFilterRectEx(OGRLayerH hLayer, int iGeomField, double
dfMinX, double dfMinY, double dfMaxX, double dfMaxY)

Set a new rectangular spatial filter.

This method set rectangle to be used as a spatial filter when fetching
features via the OGR_L_GetNextFeature() method. Only features that
geometrically intersect the given rectangle will be returned.

The x/y values should be in the same coordinate system as as the
geometry field definition it corresponds to (as returned by GetLayerDe
fn()->OGRFeatureDefn::GetGeomFieldDefn(iGeomField)->GetSpatialRef()).
Internally this method is normally implemented as creating a 5 vertex
closed rectangular polygon and passing it to
OGRLayer::SetSpatialFilter(). It exists as a convenience.

The only way to clear a spatial filter set with this method is to call
OGRLayer::SetSpatialFilter(NULL).

This method is the same as the C++ method
OGRLayer::SetSpatialFilterRect().

Parameters:
-----------

hLayer:  handle to the layer on which to set the spatial filter.

iGeomField:  index of the geometry field on which the spatial filter
operates.

dfMinX:  the minimum X coordinate for the rectangular region.

dfMinY:  the minimum Y coordinate for the rectangular region.

dfMaxX:  the maximum X coordinate for the rectangular region.

dfMaxY:  the maximum Y coordinate for the rectangular region.

GDAL 1.11 ";

%feature("docstring")  ResetReading "void
OGR_L_ResetReading(OGRLayerH hLayer)

Reset feature reading to start on the first feature.

This affects GetNextFeature().

This function is the same as the C++ method OGRLayer::ResetReading().

Parameters:
-----------

hLayer:  handle to the layer on which features are read. ";

%feature("docstring")  SyncToDisk "OGRErr OGR_L_SyncToDisk(OGRLayerH
hLayer)

Flush pending changes to disk.

This call is intended to force the layer to flush any pending writes
to disk, and leave the disk file in a consistent state. It would not
normally have any effect on read-only datasources.

Some layers do not implement this method, and will still return
OGRERR_NONE. The default implementation just returns OGRERR_NONE. An
error is only returned if an error occurs while attempting to flush to
disk.

In any event, you should always close any opened datasource with
OGR_DS_Destroy() that will ensure all data is correctly flushed.

This method is the same as the C++ method OGRLayer::SyncToDisk()

Parameters:
-----------

hLayer:  handle to the layer

OGRERR_NONE if no error occurs (even if nothing is done) or an error
code. ";

%feature("docstring")  DeleteFeature "OGRErr
OGR_L_DeleteFeature(OGRLayerH hLayer, GIntBig nFID)

Delete feature from layer.

The feature with the indicated feature id is deleted from the layer if
supported by the driver. Most drivers do not support feature deletion,
and will return OGRERR_UNSUPPORTED_OPERATION. The
OGR_L_TestCapability() function may be called with OLCDeleteFeature to
check if the driver supports feature deletion.

This method is the same as the C++ method OGRLayer::DeleteFeature().

Parameters:
-----------

hLayer:  handle to the layer

nFID:  the feature id to be deleted from the layer

OGRERR_NONE if the operation works, otherwise an appropriate error
code (e.g OGRERR_NON_EXISTING_FEATURE if the feature does not exist).
";

%feature("docstring")  GetFeaturesRead "GIntBig
OGR_L_GetFeaturesRead(OGRLayerH hLayer) ";

%feature("docstring")  GetFIDColumn "const char*
OGR_L_GetFIDColumn(OGRLayerH hLayer)

This method returns the name of the underlying database column being
used as the FID column, or \"\" if not supported.

This method is the same as the C++ method OGRLayer::GetFIDColumn()

Parameters:
-----------

hLayer:  handle to the layer

fid column name. ";

%feature("docstring")  GetGeometryColumn "const char*
OGR_L_GetGeometryColumn(OGRLayerH hLayer)

This method returns the name of the underlying database column being
used as the geometry column, or \"\" if not supported.

For layers with multiple geometry fields, this method only returns the
geometry type of the first geometry column. For other columns, use OGR
_GFld_GetNameRef(OGR_FD_GetGeomFieldDefn(OGR_L_GetLayerDefn(hLayer),
i)).

This method is the same as the C++ method
OGRLayer::GetGeometryColumn()

Parameters:
-----------

hLayer:  handle to the layer

geometry column name. ";

%feature("docstring")  GetStyleTable "OGRStyleTableH
OGR_L_GetStyleTable(OGRLayerH hLayer) ";

%feature("docstring")  SetStyleTableDirectly "void
OGR_L_SetStyleTableDirectly(OGRLayerH hLayer, OGRStyleTableH
hStyleTable) ";

%feature("docstring")  SetStyleTable "void
OGR_L_SetStyleTable(OGRLayerH hLayer, OGRStyleTableH hStyleTable) ";

%feature("docstring")  GetName "const char* OGR_L_GetName(OGRLayerH
hLayer)

Return the layer name.

This returns the same content as
OGR_FD_GetName(OGR_L_GetLayerDefn(hLayer)), but for a few drivers,
calling OGR_L_GetName() directly can avoid lengthy layer definition
initialization.

This function is the same as the C++ method OGRLayer::GetName().

Parameters:
-----------

hLayer:  handle to the layer.

the layer name (must not been freed)

OGR 1.8.0 ";

%feature("docstring")  GetGeomType "OGRwkbGeometryType
OGR_L_GetGeomType(OGRLayerH hLayer)

Return the layer geometry type.

This returns the same result as
OGR_FD_GetGeomType(OGR_L_GetLayerDefn(hLayer)), but for a few drivers,
calling OGR_L_GetGeomType() directly can avoid lengthy layer
definition initialization.

For layers with multiple geometry fields, this method only returns the
geometry type of the first geometry column. For other columns, use
OGR_GFld_GetType(OGR_FD_GetGeomFieldDefn(OGR_L_GetLayerDefn(hLayer),
i)). For layers without any geometry field, this method returns
wkbNone.

This function is the same as the C++ method OGRLayer::GetGeomType().

Parameters:
-----------

hLayer:  handle to the layer.

the geometry type

OGR 1.8.0 ";

%feature("docstring")  SetIgnoredFields "OGRErr
OGR_L_SetIgnoredFields(OGRLayerH hLayer, const char **papszFields)

Set which fields can be omitted when retrieving features from the
layer.

If the driver supports this functionality (testable using
OLCIgnoreFields capability), it will not fetch the specified fields in
subsequent calls to GetFeature() / GetNextFeature() and thus save some
processing time and/or bandwidth.

Besides field names of the layers, the following special fields can be
passed: \"OGR_GEOMETRY\" to ignore geometry and \"OGR_STYLE\" to
ignore layer style.

By default, no fields are ignored.

This method is the same as the C++ method OGRLayer::SetIgnoredFields()

Parameters:
-----------

papszFields:  an array of field names terminated by NULL item. If NULL
is passed, the ignored list is cleared.

OGRERR_NONE if all field names have been resolved (even if the driver
does not support this method) ";

%feature("docstring")  clone_spatial_filter "static OGRErr
clone_spatial_filter(OGRLayer *pLayer, OGRGeometry **ppGeometry) ";

%feature("docstring")  create_field_map "static OGRErr
create_field_map(OGRFeatureDefn *poDefn, int **map) ";

%feature("docstring")  set_result_schema "static OGRErr
set_result_schema(OGRLayer *pLayerResult, OGRFeatureDefn *poDefnInput,
OGRFeatureDefn *poDefnMethod, int *mapInput, int *mapMethod, int
combined, char **papszOptions) ";

%feature("docstring")  set_filter_from "static OGRGeometry*
set_filter_from(OGRLayer *pLayer, OGRGeometry
*pGeometryExistingFilter, OGRFeature *pFeature) ";

%feature("docstring")  promote_to_multi "static OGRGeometry*
promote_to_multi(OGRGeometry *poGeom) ";

%feature("docstring")  Intersection "OGRErr
OGR_L_Intersection(OGRLayerH pLayerInput, OGRLayerH pLayerMethod,
OGRLayerH pLayerResult, char **papszOptions, GDALProgressFunc
pfnProgress, void *pProgressArg)

Intersection of two layers.

The result layer contains features whose geometries represent areas
that are common between features in the input layer and in the method
layer. The features in the result layer have attributes from both
input and method layers. The schema of the result layer can be set by
the user or, if it is empty, is initialized to contain all fields in
the input and method layers.

If the schema of the result is set by user and contains fields that
have the same name as a field in input and in method layer, then the
attribute in the result feature will get the value from the feature of
the method layer.

For best performance use the minimum amount of features in the method
layer and copy it into a memory layer.

This method relies on GEOS support. Do not use unless the GEOS support
is compiled in.  The recognized list of options is :
SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a feature
could not be inserted.

PROMOTE_TO_MULTI=YES/NO. Set it to YES to convert Polygons into
MultiPolygons, or LineStrings to MultiLineStrings.

INPUT_PREFIX=string. Set a prefix for the field names that will be
created from the fields of the input layer.

METHOD_PREFIX=string. Set a prefix for the field names that will be
created from the fields of the method layer.

USE_PREPARED_GEOMETRIES=YES/NO. Set to NO to not use prepared
geometries to pretest intersection of features of method layer with
features of this layer.

PRETEST_CONTAINMENT=YES/NO. Set to YES to pretest the containment of
features of method layer within the features of this layer. This will
speed up the method significantly in some cases. Requires that the
prepared geometries are in effect.

This function is the same as the C++ method OGRLayer::Intersection().

Parameters:
-----------

pLayerInput:  the input layer. Should not be NULL.

pLayerMethod:  the method layer. Should not be NULL.

pLayerResult:  the layer where the features resulting from the
operation are inserted. Should not be NULL. See above the note about
the schema.

papszOptions:  NULL terminated list of options (may be NULL).

pfnProgress:  a GDALProgressFunc() compatible callback function for
reporting progress or NULL.

pProgressArg:  argument to be passed to pfnProgress. May be NULL.

an error code if there was an error or the execution was interrupted,
OGRERR_NONE otherwise.

The first geometry field is always used.

OGR 1.10 ";

%feature("docstring")  Union "OGRErr OGR_L_Union(OGRLayerH
pLayerInput, OGRLayerH pLayerMethod, OGRLayerH pLayerResult, char
**papszOptions, GDALProgressFunc pfnProgress, void *pProgressArg)

Union of two layers.

The result layer contains features whose geometries represent areas
that are in either in the input layer or in the method layer. The
features in the result layer have attributes from both input and
method layers. For features which represent areas that are only in the
input or in the method layer the respective attributes have undefined
values. The schema of the result layer can be set by the user or, if
it is empty, is initialized to contain all fields in the input and
method layers.

If the schema of the result is set by user and contains fields that
have the same name as a field in input and in method layer, then the
attribute in the result feature will get the value from the feature of
the method layer (even if it is undefined).

For best performance use the minimum amount of features in the method
layer and copy it into a memory layer.

This method relies on GEOS support. Do not use unless the GEOS support
is compiled in.  The recognized list of options is :
SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a feature
could not be inserted.

PROMOTE_TO_MULTI=YES/NO. Set it to YES to convert Polygons into
MultiPolygons, or LineStrings to MultiLineStrings.

INPUT_PREFIX=string. Set a prefix for the field names that will be
created from the fields of the input layer.

METHOD_PREFIX=string. Set a prefix for the field names that will be
created from the fields of the method layer.

USE_PREPARED_GEOMETRIES=YES/NO. Set to NO to not use prepared
geometries to pretest intersection of features of method layer with
features of this layer.

This function is the same as the C++ method OGRLayer::Union().

Parameters:
-----------

pLayerInput:  the input layer. Should not be NULL.

pLayerMethod:  the method layer. Should not be NULL.

pLayerResult:  the layer where the features resulting from the
operation are inserted. Should not be NULL. See above the note about
the schema.

papszOptions:  NULL terminated list of options (may be NULL).

pfnProgress:  a GDALProgressFunc() compatible callback function for
reporting progress or NULL.

pProgressArg:  argument to be passed to pfnProgress. May be NULL.

an error code if there was an error or the execution was interrupted,
OGRERR_NONE otherwise.

The first geometry field is always used.

OGR 1.10 ";

%feature("docstring")  SymDifference "OGRErr
OGR_L_SymDifference(OGRLayerH pLayerInput, OGRLayerH pLayerMethod,
OGRLayerH pLayerResult, char **papszOptions, GDALProgressFunc
pfnProgress, void *pProgressArg)

Symmetrical difference of two layers.

The result layer contains features whose geometries represent areas
that are in either in the input layer or in the method layer but not
in both. The features in the result layer have attributes from both
input and method layers. For features which represent areas that are
only in the input or in the method layer the respective attributes
have undefined values. The schema of the result layer can be set by
the user or, if it is empty, is initialized to contain all fields in
the input and method layers.

If the schema of the result is set by user and contains fields that
have the same name as a field in input and in method layer, then the
attribute in the result feature will get the value from the feature of
the method layer (even if it is undefined).

For best performance use the minimum amount of features in the method
layer and copy it into a memory layer.

This method relies on GEOS support. Do not use unless the GEOS support
is compiled in.  The recognized list of options is :
SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a feature
could not be inserted.

PROMOTE_TO_MULTI=YES/NO. Set it to YES to convert Polygons into
MultiPolygons, or LineStrings to MultiLineStrings.

INPUT_PREFIX=string. Set a prefix for the field names that will be
created from the fields of the input layer.

METHOD_PREFIX=string. Set a prefix for the field names that will be
created from the fields of the method layer.

This function is the same as the C++ method OGRLayer::SymDifference().

Parameters:
-----------

pLayerInput:  the input layer. Should not be NULL.

pLayerMethod:  the method layer. Should not be NULL.

pLayerResult:  the layer where the features resulting from the
operation are inserted. Should not be NULL. See above the note about
the schema.

papszOptions:  NULL terminated list of options (may be NULL).

pfnProgress:  a GDALProgressFunc() compatible callback function for
reporting progress or NULL.

pProgressArg:  argument to be passed to pfnProgress. May be NULL.

an error code if there was an error or the execution was interrupted,
OGRERR_NONE otherwise.

The first geometry field is always used.

OGR 1.10 ";

%feature("docstring")  Identity "OGRErr OGR_L_Identity(OGRLayerH
pLayerInput, OGRLayerH pLayerMethod, OGRLayerH pLayerResult, char
**papszOptions, GDALProgressFunc pfnProgress, void *pProgressArg)

Identify the features of this layer with the ones from the identity
layer.

The result layer contains features whose geometries represent areas
that are in the input layer. The features in the result layer have
attributes from both input and method layers. The schema of the result
layer can be set by the user or, if it is empty, is initialized to
contain all fields in input and method layers.

If the schema of the result is set by user and contains fields that
have the same name as a field in input and in method layer, then the
attribute in the result feature will get the value from the feature of
the method layer (even if it is undefined).

For best performance use the minimum amount of features in the method
layer and copy it into a memory layer.

This method relies on GEOS support. Do not use unless the GEOS support
is compiled in.  The recognized list of options is :
SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a feature
could not be inserted.

PROMOTE_TO_MULTI=YES/NO. Set it to YES to convert Polygons into
MultiPolygons, or LineStrings to MultiLineStrings.

INPUT_PREFIX=string. Set a prefix for the field names that will be
created from the fields of the input layer.

METHOD_PREFIX=string. Set a prefix for the field names that will be
created from the fields of the method layer.

USE_PREPARED_GEOMETRIES=YES/NO. Set to NO to not use prepared
geometries to pretest intersection of features of method layer with
features of this layer.

This function is the same as the C++ method OGRLayer::Identity().

Parameters:
-----------

pLayerInput:  the input layer. Should not be NULL.

pLayerMethod:  the method layer. Should not be NULL.

pLayerResult:  the layer where the features resulting from the
operation are inserted. Should not be NULL. See above the note about
the schema.

papszOptions:  NULL terminated list of options (may be NULL).

pfnProgress:  a GDALProgressFunc() compatible callback function for
reporting progress or NULL.

pProgressArg:  argument to be passed to pfnProgress. May be NULL.

an error code if there was an error or the execution was interrupted,
OGRERR_NONE otherwise.

The first geometry field is always used.

OGR 1.10 ";

%feature("docstring")  Update "OGRErr OGR_L_Update(OGRLayerH
pLayerInput, OGRLayerH pLayerMethod, OGRLayerH pLayerResult, char
**papszOptions, GDALProgressFunc pfnProgress, void *pProgressArg)

Update this layer with features from the update layer.

The result layer contains features whose geometries represent areas
that are either in the input layer or in the method layer. The
features in the result layer have areas of the features of the method
layer or those ares of the features of the input layer that are not
covered by the method layer. The features of the result layer get
their attributes from the input layer. The schema of the result layer
can be set by the user or, if it is empty, is initialized to contain
all fields in the input layer.

If the schema of the result is set by user and contains fields that
have the same name as a field in the method layer, then the attribute
in the result feature the originates from the method layer will get
the value from the feature of the method layer.

For best performance use the minimum amount of features in the method
layer and copy it into a memory layer.

This method relies on GEOS support. Do not use unless the GEOS support
is compiled in.  The recognized list of options is :
SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a feature
could not be inserted.

PROMOTE_TO_MULTI=YES/NO. Set it to YES to convert Polygons into
MultiPolygons, or LineStrings to MultiLineStrings.

INPUT_PREFIX=string. Set a prefix for the field names that will be
created from the fields of the input layer.

METHOD_PREFIX=string. Set a prefix for the field names that will be
created from the fields of the method layer.

This function is the same as the C++ method OGRLayer::Update().

Parameters:
-----------

pLayerInput:  the input layer. Should not be NULL.

pLayerMethod:  the method layer. Should not be NULL.

pLayerResult:  the layer where the features resulting from the
operation are inserted. Should not be NULL. See above the note about
the schema.

papszOptions:  NULL terminated list of options (may be NULL).

pfnProgress:  a GDALProgressFunc() compatible callback function for
reporting progress or NULL.

pProgressArg:  argument to be passed to pfnProgress. May be NULL.

an error code if there was an error or the execution was interrupted,
OGRERR_NONE otherwise.

The first geometry field is always used.

OGR 1.10 ";

%feature("docstring")  Clip "OGRErr OGR_L_Clip(OGRLayerH pLayerInput,
OGRLayerH pLayerMethod, OGRLayerH pLayerResult, char **papszOptions,
GDALProgressFunc pfnProgress, void *pProgressArg)

Clip off areas that are not covered by the method layer.

The result layer contains features whose geometries represent areas
that are in the input layer and in the method layer. The features in
the result layer have the (possibly clipped) areas of features in the
input layer and the attributes from the same features. The schema of
the result layer can be set by the user or, if it is empty, is
initialized to contain all fields in the input layer.

For best performance use the minimum amount of features in the method
layer and copy it into a memory layer.

This method relies on GEOS support. Do not use unless the GEOS support
is compiled in.  The recognized list of options is :
SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a feature
could not be inserted.

PROMOTE_TO_MULTI=YES/NO. Set it to YES to convert Polygons into
MultiPolygons, or LineStrings to MultiLineStrings.

INPUT_PREFIX=string. Set a prefix for the field names that will be
created from the fields of the input layer.

METHOD_PREFIX=string. Set a prefix for the field names that will be
created from the fields of the method layer.

This function is the same as the C++ method OGRLayer::Clip().

Parameters:
-----------

pLayerInput:  the input layer. Should not be NULL.

pLayerMethod:  the method layer. Should not be NULL.

pLayerResult:  the layer where the features resulting from the
operation are inserted. Should not be NULL. See above the note about
the schema.

papszOptions:  NULL terminated list of options (may be NULL).

pfnProgress:  a GDALProgressFunc() compatible callback function for
reporting progress or NULL.

pProgressArg:  argument to be passed to pfnProgress. May be NULL.

an error code if there was an error or the execution was interrupted,
OGRERR_NONE otherwise.

The first geometry field is always used.

OGR 1.10 ";

%feature("docstring")  Erase "OGRErr OGR_L_Erase(OGRLayerH
pLayerInput, OGRLayerH pLayerMethod, OGRLayerH pLayerResult, char
**papszOptions, GDALProgressFunc pfnProgress, void *pProgressArg)

Remove areas that are covered by the method layer.

The result layer contains features whose geometries represent areas
that are in the input layer but not in the method layer. The features
in the result layer have attributes from the input layer. The schema
of the result layer can be set by the user or, if it is empty, is
initialized to contain all fields in the input layer.

For best performance use the minimum amount of features in the method
layer and copy it into a memory layer.

This method relies on GEOS support. Do not use unless the GEOS support
is compiled in.  The recognized list of options is :
SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a feature
could not be inserted.

PROMOTE_TO_MULTI=YES/NO. Set it to YES to convert Polygons into
MultiPolygons, or LineStrings to MultiLineStrings.

INPUT_PREFIX=string. Set a prefix for the field names that will be
created from the fields of the input layer.

METHOD_PREFIX=string. Set a prefix for the field names that will be
created from the fields of the method layer.

This function is the same as the C++ method OGRLayer::Erase().

Parameters:
-----------

pLayerInput:  the input layer. Should not be NULL.

pLayerMethod:  the method layer. Should not be NULL.

pLayerResult:  the layer where the features resulting from the
operation are inserted. Should not be NULL. See above the note about
the schema.

papszOptions:  NULL terminated list of options (may be NULL).

pfnProgress:  a GDALProgressFunc() compatible callback function for
reporting progress or NULL.

pProgressArg:  argument to be passed to pfnProgress. May be NULL.

an error code if there was an error or the execution was interrupted,
OGRERR_NONE otherwise.

The first geometry field is always used.

OGR 1.10 ";

}
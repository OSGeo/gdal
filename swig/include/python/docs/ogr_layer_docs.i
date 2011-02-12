%extend OGRLayerShadow {
// File: ogrlayer_8cpp.xml
%feature("docstring")  CPL_CVSID "CPL_CVSID(\"$Id: ogrlayer.cpp 20885
2010-10-19 00:16:08Z warmerdam $\") ";

%feature("docstring")  Reference "int OGR_L_Reference(OGRLayerH
hLayer) ";

%feature("docstring")  Dereference "int OGR_L_Dereference(OGRLayerH
hLayer) ";

%feature("docstring")  GetRefCount "int OGR_L_GetRefCount(OGRLayerH
hLayer) ";

%feature("docstring")  GetFeatureCount "int
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
OGR_L_GetFeature(OGRLayerH hLayer, long nFeatureId)

Fetch a feature by its identifier.

This function will attempt to read the identified feature. The nFID
value cannot be OGRNullFID. Success or failure of this operation is
unaffected by the spatial or attribute filters.

If this function returns a non-NULL feature, it is guaranteed that its
feature id ( OGR_F_GetFID()) will be the same as nFID.

Use OGR_L_TestCapability(OLCRandomRead) to establish if this layer
supports efficient random access reading via OGR_L_GetFeature();
however, the call should always work if the feature exists as a
fallback implementation just scans all the features in the layer
looking for the desired feature.

Sequential reads are generally considered interrupted by a
OGR_L_GetFeature() call.

The returned feature should be free with OGR_F_Destroy().

This function is the same as the C++ method OGRLayer::GetFeature( ).

Parameters:
-----------

hLayer:  handle to the layer that owned the feature.

nFeatureId:  the feature id of the feature to read.

an handle to a feature now owned by the caller, or NULL on failure. ";

%feature("docstring")  SetNextByIndex "OGRErr
OGR_L_SetNextByIndex(OGRLayerH hLayer, long nIndex)

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

The returned feature becomes the responsiblity of the caller to delete
with OGR_F_Destroy(). It is critical that all features associated with
an OGRLayer (more specifically an OGRFeatureDefn) be deleted before
that layer/datasource is deleted.

Only features matching the current spatial filter (set with
SetSpatialFilter()) will be returned.

This function implements sequential access to the features of a layer.
The OGR_L_ResetReading() function can be used to start at the
beginning again.

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
code. ";

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

This function is the same as the C++ method OGRLayer::CreateField().

Parameters:
-----------

hLayer:  handle to the layer to write the field definition.

hField:  handle of the field definition to write to disk.

bApproxOK:  If TRUE, the field may be created in a slightly different
form depending on the limitations of the format driver.

OGRERR_NONE on success. ";

%feature("docstring")  StartTransaction "OGRErr
OGR_L_StartTransaction(OGRLayerH hLayer)

For datasources which support transactions, StartTransaction creates a
transaction.

If starting the transaction fails, will return OGRERR_FAILURE.
Datasources which do not support transactions will always return
OGRERR_NONE.

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
transaction. If no transaction is active, or the rollback fails, will
return OGRERR_FAILURE. Datasources which do not support transactions
will always return OGRERR_NONE.

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
return a feature count (via OGR_L_GetFeatureCount()) efficiently ...
ie. without counting the features. In some cases this will return TRUE
until a spatial filter is installed after which it will return FALSE.

OLCFastGetExtent / \"FastGetExtent\": TRUE if this layer can return
its data extent (via OGR_L_GetExtent()) efficiently ... ie. without
scanning all the features. In some cases this will return TRUE until a
spatial filter is installed after which it will return FALSE.

OLCFastSetNextByIndex / \"FastSetNextByIndex\": TRUE if this layer can
perform the SetNextByIndex() call efficiently, otherwise FALSE.

OLCCreateField / \"CreateField\": TRUE if this layer can create new
fields on the current layer using CreateField(), otherwise FALSE.

OLCDeleteFeature / \"DeleteFeature\": TRUE if the DeleteFeature()
method is supported on this layer, otherwise FALSE.

OLCStringsAsUTF8 / \"StringsAsUTF8\": TRUE if values of OFTString
fields are assured to be in UTF-8 format. If FALSE the encoding of
fields is uncertain, though it might still be UTF-8.

OLCTransactions / \"Transactions\": TRUE if the StartTransaction(),
CommitTransaction() and RollbackTransaction() methods work in a
meaningful way, otherwise FALSE.

This function is the same as the C++ method
OGRLayer::TestCapability().

Parameters:
-----------

hLayer:  handle to the layer to get the capability from.

pszCap:  the name of the capability to test.

TRUE if the layer has the requested capability, or FALSE otherwise.
OGRLayers will return FALSE for any unrecognised capabilities. ";

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

%feature("docstring")  ResetReading "void
OGR_L_ResetReading(OGRLayerH hLayer)

Reset feature reading to start on the first feature.

This affects GetNextFeature().

This function is the same as the C++ method OGRLayer::ResetReading().

Parameters:
-----------

hLayer:  handle to the layer on which features are read. ";

%feature("docstring")  SyncToDisk "OGRErr OGR_L_SyncToDisk(OGRLayerH
hDS)

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
OGR_L_DeleteFeature(OGRLayerH hDS, long nFID)

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

OGRERR_NONE on success. ";

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

}
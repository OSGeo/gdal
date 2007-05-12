%extend OGRLayerShadow {

%feature("docstring") GetRefCount
"""
Return the reference count of the layer.
""";

%feature("docstring") SetSpatialFilter
"""
Set a new spatial filter on the layer

This method set the geometry to be used as a spatial filter when fetching 
features via the GetNextFeature() method. Only features that geometrically 
intersect the filter geometry will be returned.

Currently this test is may be inaccurately implemented, but it is guaranteed 
that all features who's envelope (as returned by Geometry::getEnvelope()) 
overlaps the envelope of the spatial filter will be returned. This can result 
in more shapes being returned that should strictly be the case.

This method makes an internal copy of the passed geometry. The passed geometry 
remains the responsibility of the caller, and may be safely destroyed.

For the time being the passed filter geometry should be in the same SRS as 
the layer (as returned by Layer::GetSpatialRef()). In the future this 
may be generalized.

""";

%feature("docstring") SetSpatialFilterRect
"""
Set a new rectangular spatial filter.

This method set rectangle to be used as a spatial filter when fetching 
features via the GetNextFeature() method. Only features that geometrically 
intersect the given rectangle will be returned.

The x/y values should be in the same coordinate system as the layer as a 
whole (as returned by Layer::GetSpatialRef()). Internally this method is 
normally implemented as creating a 5 vertex closed rectangular polygon and 
passing it to Layer::SetSpatialFilter(). It exists as a convenience.

The only way to clear a spatial filter set with this method is to call 
Layer::SetSpatialFilter(NULL).

""";

%feature("docstring") GetSpatialFilter
"""
Return the current spatial filter for the layer.
""";

%feature("docstring") SetAttributeFilter
"""
Set a new attribute query.

This method sets the attribute query string to be used when fetching features 
via the GetNextFeature() method. Only features for which the query evaluates
 as true will be returned.

The query string should be in the format of an SQL WHERE clause. For instance 
'population > 1000000 and population < 5000000' where population is an 
attribute in the layer. The query format is a restricted form of SQL WHERE 
clause as defined 'eq_format=restricted_where' about half way through 
this document:

http://ogdi.sourceforge.net/prop/6.2.CapabilitiesMetadata.html

Note that installing a query string will generally result in resetting 
the current reading position (ala ResetReading()).
""";

%feature("docstring") ResetReading
"""
Reset feature reading to start on the first feature. This 
affects GetNextFeature().
""";

%feature("docstring") GetName
"""
Convenience method to return the name of the layer from the layer definition.
""";

%feature("docstring") GetFeature
"""
Fetch a feature by its identifier.

This function will attempt to read the identified feature. The nFID value 
cannot be OGRNullFID. Success or failure of this operation is unaffected 
by the spatial or attribute filters.

If this method returns a non-NULL feature, it is guaranteed that it's feature 
id (Feature::GetFID()) will be the same as nFID.

Use OGRLayer::TestCapability(OLCRandomRead) to establish if this layer 
supports efficient random access reading via GetFeature(); however, the call 
should always work if the feature exists as a fallback implementation just 
scans all the features in the layer looking for the desired feature.

Sequential reads are generally considered interrupted by a GetFeature() call.
""";

%feature("docstring") GetNextFeature
"""
Fetch the next available feature from this layer. The returned feature
becomes the responsibility of the caller to delete.

Only features matching the current spatial filter (set with 
SetSpatialFilter()) will be returned.

This method implements sequential access to the features of a layer. The 
ResetReading() method can be used to start at the beginning again.
""";

%feature("docstring") SetNextByIndex
"""
Move read cursor to the nIndex'th feature in the current resultset.

This method allows positioning of a layer such that the GetNextFeature() 
call will read the requested feature, where nIndex is an absolute index 
into the current result set. So, setting it to 3 would mean the next feature 
read with GetNextFeature() would have been the 4th feature to have been 
read if sequential reading took place from the beginning of the layer, 
including accounting for spatial and attribute filters.

Only in rare circumstances is SetNextByIndex() efficiently implemented. 
In all other cases the default implementation which calls ResetReading() 
and then calls GetNextFeature() nIndex times is used. To determine if fast 
seeking is available on the current layer use the TestCapability() method 
with a value of OLCFastSetNextByIndex.
""";

%feature("docstring") SetFeature
"""
Rewrite an existing feature.

This method will write a feature to the layer, based on the 
feature id within the Feature.

Use Layer::TestCapability(OLCRandomWrite) to establish if this layer 
supports random access writing via SetFeature().
""";

%feature("docstring") CreateFeature
"""
Create and write a new feature within a layer.

The passed feature is written to the layer as a new feature, rather than 
overwriting an existing one. If the feature has a feature id other than 
OGRNullFID, then the native implementation may use that as the feature 
id of the new feature, but not necessarily. Upon successful return the 
passed feature will have been updated with the new feature id.
""";

%feature("docstring") DeleteFeature
"""
Delete feature from layer.

The feature with the indicated feature id is deleted from the layer if 
supported by the driver. Most drivers do not support feature deletion, 
and will return OGRERR_UNSUPPORTED_OPERATION. The TestCapability() layer 
method may be called with OLCDeleteFeature to check if the driver 
supports feature deletion.
""";

%feature("docstring") SyncToDisk
"""
Flush pending changes to disk.

This call is intended to force the layer to flush any pending writes to disk, 
and leave the disk file in a consistent state. It would not normally have 
any effect on read-only datasources.

Some layers do not implement this method, and will still return OGRERR_NONE. 
The default implementation just returns OGRERR_NONE. An error is only 
returned if an error occurs while attempting to flush to disk.
""";

%feature("docstring") GetLayerDefn
"""
Fetch the schema information for this layer.

The returned OGRFeatureDefn is owned by the OGRLayer, and should not be 
modified or freed by the application. It encapsulates the attribute schema 
of the features of the layer.
""";

%feature("docstring") GetFeatureCount
"""
Fetch the feature count in this layer.

Returns the number of features in the layer. For dynamic databases the 
count may not be exact. If bForce is FALSE, and it would be expensive to 
establish the feature count a value of -1 may be returned indicating that 
the count isn't know. If bForce is TRUE some implementations will actually 
scan the entire layer once to count objects.

The returned count takes the spatial filter into account.
""";

%feature("docstring") GetExtent
"""
Fetch the extent of this layer.

Returns the extent (MBR) of the data in the layer. If bForce is FALSE, and it 
would be expensive to establish the extent then OGRERR_FAILURE will be 
returned indicating that the extent isn't know. If bForce is TRUE then some 
implementations will actually scan the entire layer once to compute the MBR 
of all the features in the layer.

The returned extent does not take the spatial filter into account. If a
spatial filter was previously set then it should be ignored but some 
implementations may be unable to do that, so it is safer to call 
GetExtent() without setting a spatial filter.

Layers without any geometry may return OGRERR_FAILURE just indicating 
that no meaningful extents could be collected.
""";

%feature("docstring") TestCapability
"""
Test if this layer supported the named capability.

The capability codes that can be tested are represented as strings, but 
defined constants exists to ensure correct spelling. Specific layer types 
may implement class specific capabilities, but this can't generally be 
discovered by the caller.

    * OLCRandomRead / 'RandomRead': TRUE if the GetFeature() method works 
                                    for this layer.

    * OLCSequentialWrite / 'SequentialWrite': TRUE if the CreateFeature() 
                                              method works for this layer. 
                                              Note this means that this 
                                              particular layer is writable. 
                                              The same Layer class may 
                                              returned FALSE for other layer 
                                              instances that are effectively 
                                              read-only.

    * OLCRandomWrite / 'RandomWrite': TRUE if the SetFeature() method is 
                                      operational on this layer. Note this 
                                      means that this particular layer is 
                                      writable. The same OGRLayer class may 
                                      returned FALSE for other layer instances 
                                      that are effectively read-only.

    * OLCFastSpatialFilter / 'FastSpatialFilter': TRUE if this layer 
                                                  implements spatial filtering 
                                                  efficiently. Layers that 
                                                  effectively read all 
                                                  features, and test them with 
                                                  the OGRFeature intersection 
                                                  methods should return FALSE. 
                                                  This can be used as a clue 
                                                  by the application whether 
                                                  it should build and maintain 
                                                  it's own spatial index for 
                                                  features in this layer.

    * OLCFastFeatureCount / 'FastFeatureCount': TRUE if this layer can return 
                                                a feature count 
                                                (via Layer::GetFeatureCount()) 
                                                efficiently ... ie. without 
                                                counting the features. In some 
                                                cases this will return TRUE 
                                                until a spatial filter is 
                                                installed after which it will 
                                                return FALSE.

    * OLCFastGetExtent / 'FastGetExtent': TRUE if this layer can return its 
                                          data extent (via Layer::GetExtent()) 
                                          efficiently ... ie. without scanning 
                                          all the features. In some cases this 
                                          will return TRUE until a spatial 
                                          filter is installed after which it 
                                          will return FALSE.

    * OLCFastSetNextByIndex / 'FastSetNextByIndex': TRUE if this layer can 
                                                    perform the SetNextByIndex() 
                                                    call efficiently, 
                                                    otherwise FALSE.
""";

%feature("docstring") CreateField
"""
Create a new field on a layer. 

You must use this to create new fields on a real layer. Internally the 
FeatureDefn for the layer will be updated to reflect the new field. 
Applications should never modify the FeatureDefn used by a layer directly.
""";

%feature("docstring") StartTransaction
"""
Starts the transaction for layers that support it.  There is currently no 
capabilities test to discover if layers support transactions.
""";

%feature("docstring") CommitTransaction
"""
Commits the transaction for layers that support it.  There is currently no 
capabilities test to discover if layers support transactions.
""";

%feature("docstring") RollbackTransaction
"""
Rolls back the transaction for layers that support it.  There is currently no 
capabilities test to discover if layers support transactions.
""";

%feature("docstring") GetSpatialRef
"""
Fetch the spatial reference system for this layer.

The returned object is owned by the Layer and should not be modified or 
freed by the application.
""";

%feature("docstring") GetFeatureRead
"""
Fetch the spatial reference system for this layer.

The returned object is owned by the Layer and should not be modified or 
freed by the application.
""";

}
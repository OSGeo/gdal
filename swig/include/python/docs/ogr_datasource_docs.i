%feature("docstring") OGRDataSourceShadow "
Python proxy of a vector :cpp:class:`GDALDataset`.

Since GDAL 3.8, a DataSource can be used as a context manager.
When exiting the context, the DataSource will be closed and
features will be written to disk.
"

%extend OGRDataSourceShadow {
// File: ogrdatasource_8cpp.xml

%feature("docstring")  Close "
Closes opened dataset and releases allocated resources.

This method can be used to force the dataset to close
when one more references to the dataset are still
reachable. If Close is never called, the dataset will
be closed automatically during garbage collection.
"

%feature("docstring")  Destroy "void OGR_DS_Destroy(OGRDataSourceH
hDS)

Closes opened datasource and releases allocated resources.

This method is the same as the C++ method
OGRDataSource::DestroyDataSource().

Deprecated Use GDALClose() in GDAL 2.0

Parameters
-----------
hDS:
    handle to allocated datasource object.
";

%feature("docstring")  Reference "int OGR_DS_Reference(OGRDataSourceH
hDataSource) ";

%feature("docstring")  Dereference "int
OGR_DS_Dereference(OGRDataSourceH hDataSource) ";

%feature("docstring")  GetRefCount "int
OGR_DS_GetRefCount(OGRDataSourceH hDataSource) ";

%feature("docstring")  GetSummaryRefCount "int
OGR_DS_GetSummaryRefCount(OGRDataSourceH hDataSource) ";

%feature("docstring")  CreateLayer "OGRLayerH
OGR_DS_CreateLayer(OGRDataSourceH hDS, const char \\*pszName,
OGRSpatialReferenceH hSpatialRef, OGRwkbGeometryType eType, char
\\*\\*papszOptions)

This function attempts to create a new layer on the data source with
the indicated name, coordinate system, geometry type.

The papszOptions argument can be used to control driver specific
creation options. These options are normally documented in the format
specific documentation.

Deprecated Use GDALDatasetCreateLayer() in GDAL 2.0

Parameters
-----------
hDS:
    The dataset handle.pszName:  the name for the new layer. This should ideally not match
    any existing layer on the datasource.
hSpatialRef:
    handle to the coordinate system to use for the new
    layer, or NULL if no coordinate system is available. The driver might
    only increase the reference counter of the object to take ownership,
    and not make a full copy, so do not use OSRDestroySpatialReference(),
    but OSRRelease() instead when you are done with the object.
eType:
    the geometry type for the layer. Use wkbUnknown if there are
    no constraints on the types geometry to be written.
papszOptions:
    a StringList of name=value options. Options are driver
    specific, and driver information can be found at the following
    url:http://www.gdal.org/ogr_formats.html


Returns
--------
OGRLayerH:
    NULL is returned on failure, or a new OGRLayer handle on success.
";

%feature("docstring")  CopyLayer "OGRLayerH
OGR_DS_CopyLayer(OGRDataSourceH hDS, OGRLayerH hSrcLayer, const char
\\*pszNewName, char \\*\\*papszOptions)

Duplicate an existing layer.

This function creates a new layer, duplicate the field definitions of
the source layer and then duplicate each features of the source layer.
The papszOptions argument can be used to control driver specific
creation options. These options are normally documented in the format
specific documentation. The source layer may come from another
dataset.

Deprecated Use GDALDatasetCopyLayer() in GDAL 2.0

Parameters
-----------
hDS:
    handle to the data source where to create the new layer
hSrcLayer:
    handle to the source layer.
pszNewName:
    the name of the layer to create.
papszOptions:
    a StringList of name=value options. Options are driver
    specific.

Returns
-------
OGRLayerH:
    a handle to the layer, or NULL if an error occurs.
";

%feature("docstring")  GetLayerByName "OGRLayerH
OGR_DS_GetLayerByName(OGRDataSourceH hDS, const char \\*pszLayerName)

Fetch a layer by name.

The returned layer remains owned by the OGRDataSource and should not
be deleted by the application.

Deprecated Use GDALDatasetGetLayerByName() in GDAL 2.0

Parameters
-----------
hDS:
    handle to the data source from which to get the layer.
pszLayerName:
    Layer the layer name of the layer to fetch.


Returns
--------
OGRLayerH:
    a handle to the layer, or NULL if the layer is not found or an error
    occurs.
";

%feature("docstring")  TestCapability "int
OGR_DS_TestCapability(OGRDataSourceH hDS, const char \\*pszCapability)

Test if capability is available.

One of the following data source capability names can be passed into
this function, and a TRUE or FALSE value will be returned indicating
whether or not the capability is available for this object.

ODsCCreateLayer: True if this datasource can create new layers.

ODsCDeleteLayer: True if this datasource can delete existing layers.

ODsCCreateGeomFieldAfterCreateLayer: True if the layers of this
datasource support CreateGeomField() just after layer creation.

ODsCCurveGeometries: True if this datasource supports writing curve
geometries. (GDAL 2.0). In that case, OLCCurveGeometries must also be
declared in layers of that dataset.

The #define macro forms of the capability names should be used in
preference to the strings themselves to avoid misspelling.

Deprecated Use GDALDatasetTestCapability() in GDAL 2.0

Parameters
-----------
hDS:
    handle to the data source against which to test the capability.
pszCapability:
    the capability to test.

Returns
--------
int:
    TRUE if capability available otherwise FALSE.
";

%feature("docstring")  GetLayerCount "int
OGR_DS_GetLayerCount(OGRDataSourceH hDS)

Get the number of layers in this data source.

Deprecated Use GDALDatasetGetLayerCount() in GDAL 2.0

Parameters
-----------
hDS:
    handle to the data source from which to get the number of
    layers.

Returns
--------
int:
    layer count.
";

%feature("docstring")  GetLayer "OGRLayerH
OGR_DS_GetLayer(OGRDataSourceH hDS, int iLayer)

Fetch a layer by index.

The returned layer remains owned by the OGRDataSource and should not
be deleted by the application.

Deprecated Use GDALDatasetGetLayer() in GDAL 2.0

Parameters
-----------
hDS:
    handle to the data source from which to get the layer.
iLayer:
    a layer number between 0 and OGR_DS_GetLayerCount()-1.

Returns
--------
OGRLayerH:
    a handle to the layer, or NULL if iLayer is out of range or an error
    occurs.
";

%feature("docstring")  GetName "const char\\*
OGR_DS_GetName(OGRDataSourceH hDS)

Returns the name of the data source.

This string should be sufficient to open the data source if passed to
the same OGRSFDriver that this data source was opened with, but it
need not be exactly the same string that was used to open the data
source. Normally this is a filename.

Deprecated Use GDALGetDescription() in GDAL 2.0

Parameters
-----------
hDS:
    handle to the data source to get the name from.

Returns
--------
str:
    pointer to an internal name string which should not be modified or
    freed by the caller.
";

%feature("docstring")  SyncToDisk "OGRErr
OGR_DS_SyncToDisk(OGRDataSourceH hDS)

Flush pending changes to disk.

See GDALDataset::FlushCache() ";

%feature("docstring")  GetDriver "OGRSFDriverH
OGR_DS_GetDriver(OGRDataSourceH hDS)

Returns the driver that the dataset was opened with.

NOTE: Starting with GDAL 2.0, it is NOT safe to cast the returned
handle to OGRSFDriver\\*. If a C++ object is needed, the handle should
be cast to GDALDriver\\*.

Deprecated Use GDALGetDatasetDriver() in GDAL 2.0

Parameters
-----------
hDS:
    handle to the datasource

Returns
--------
OGRSFDriverH:
    NULL if driver info is not available, or pointer to a driver owned by
    the OGRSFDriverManager.
";

%feature("docstring")  GetStyleTable "OGRStyleTableH
OGR_DS_GetStyleTable(OGRDataSourceH hDS)

Get style table. ";

%feature("docstring")  SetStyleTableDirectly "void
OGR_DS_SetStyleTableDirectly(OGRDataSourceH hDS, OGRStyleTableH
hStyleTable)

Set style table (and take ownership) ";

%feature("docstring")  SetStyleTable "void
OGR_DS_SetStyleTable(OGRDataSourceH hDS, OGRStyleTableH hStyleTable)

Set style table. ";

}

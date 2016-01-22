%extend OGRDataSourceShadow {
// File: ogrdatasource_8cpp.xml
%feature("docstring")  CPL_CVSID "CPL_CVSID(\"$Id: ogrdatasource.cpp
23403 2011-11-20 21:01:21Z ajolma $\") ";

%feature("docstring")  Destroy "void OGR_DS_Destroy(OGRDataSourceH
hDS)

Closes opened datasource and releases allocated resources.

This method is the same as the C++ method
OGRDataSource::DestroyDataSource().

Parameters:
-----------

hDataSource:  handle to allocated datasource object. ";

%feature("docstring")  Reference "int OGR_DS_Reference(OGRDataSourceH
hDataSource) ";

%feature("docstring")  Dereference "int
OGR_DS_Dereference(OGRDataSourceH hDataSource) ";

%feature("docstring")  GetRefCount "int
OGR_DS_GetRefCount(OGRDataSourceH hDataSource) ";

%feature("docstring")  GetSummaryRefCount "int
OGR_DS_GetSummaryRefCount(OGRDataSourceH hDataSource) ";

%feature("docstring")  CreateLayer "OGRLayerH
OGR_DS_CreateLayer(OGRDataSourceH hDS, const char *pszName,
OGRSpatialReferenceH hSpatialRef, OGRwkbGeometryType eType, char
**papszOptions)

This function attempts to create a new layer on the data source with
the indicated name, coordinate system, geometry type.

The papszOptions argument can be used to control driver specific
creation options. These options are normally documented in the format
specific documentation.

This function is the same as the C++ method
OGRDataSource::CreateLayer().

Parameters:
-----------

hDS:  The dataset handle.

pszName:  the name for the new layer. This should ideally not match
any existing layer on the datasource.

hSpatialRef:  handle to the coordinate system to use for the new
layer, or NULL if no coordinate system is available.

eType:  the geometry type for the layer. Use wkbUnknown if there are
no constraints on the types geometry to be written.

papszOptions:  a StringList of name=value options. Options are driver
specific, and driver information can be found at the following
url:http://www.gdal.org/ogr/ogr_formats.html

NULL is returned on failure, or a new OGRLayer handle on success.
Example: ";

%feature("docstring")  CopyLayer "OGRLayerH
OGR_DS_CopyLayer(OGRDataSourceH hDS, OGRLayerH hSrcLayer, const char
*pszNewName, char **papszOptions)

Duplicate an existing layer.

This function creates a new layer, duplicate the field definitions of
the source layer and then duplicate each features of the source layer.
The papszOptions argument can be used to control driver specific
creation options. These options are normally documented in the format
specific documentation. The source layer may come from another
dataset.

This function is the same as the C++ method OGRDataSource::CopyLayer

Parameters:
-----------

hDS:  handle to the data source where to create the new layer

hSrcLayer:  handle to the source layer.

pszNewName:  the name of the layer to create.

papszOptions:  a StringList of name=value options. Options are driver
specific.

an handle to the layer, or NULL if an error occurs. ";

%feature("docstring")  DeleteLayer "OGRErr
OGR_DS_DeleteLayer(OGRDataSourceH hDS, int iLayer)

Delete the indicated layer from the datasource.

If this method is supported the ODsCDeleteLayer capability will test
TRUE on the OGRDataSource.

This method is the same as the C++ method
OGRDataSource::DeleteLayer().

Parameters:
-----------

hDS:  handle to the datasource

iLayer:  the index of the layer to delete.

OGRERR_NONE on success, or OGRERR_UNSUPPORTED_OPERATION if deleting
layers is not supported for this datasource. ";

%feature("docstring")  GetLayerByName "OGRLayerH
OGR_DS_GetLayerByName(OGRDataSourceH hDS, const char *pszName)

Fetch a layer by name.

The returned layer remains owned by the OGRDataSource and should not
be deleted by the application.

This function is the same as the C++ method
OGRDataSource::GetLayerByName().

Parameters:
-----------

hDS:  handle to the data source from which to get the layer.

pszLayerName:  Layer the layer name of the layer to fetch.

an handle to the layer, or NULL if the layer is not found or an error
occurs. ";

%feature("docstring")  OGRDataSourceParseSQLType "static OGRFieldType
OGRDataSourceParseSQLType(char *pszType, int &nWidth, int &nPrecision)
";

%feature("docstring")  ExecuteSQL "OGRLayerH
OGR_DS_ExecuteSQL(OGRDataSourceH hDS, const char *pszStatement,
OGRGeometryH hSpatialFilter, const char *pszDialect)

Execute an SQL statement against the data store.

The result of an SQL query is either NULL for statements that are in
error, or that have no results set, or an OGRLayer handle representing
a results set from the query. Note that this OGRLayer is in addition
to the layers in the data store and must be destroyed with
OGR_DS_ReleaseResultSet() before the data source is closed
(destroyed).

For more information on the SQL dialect supported internally by OGR
review theOGR SQL document. Some drivers (ie. Oracle and PostGIS) pass
the SQL directly through to the underlying RDBMS.

This function is the same as the C++ method
OGRDataSource::ExecuteSQL();

Parameters:
-----------

hDS:  handle to the data source on which the SQL query is executed.

pszSQLCommand:  the SQL statement to execute.

hSpatialFilter:  handle to a geometry which represents a spatial
filter. Can be NULL.

pszDialect:  allows control of the statement dialect. If set to NULL,
the OGR SQL engine will be used, except for RDBMS drivers that will
use their dedicated SQL engine, unless OGRSQL is explicitely passed as
the dialect.

an handle to a OGRLayer containing the results of the query.
Deallocate with OGR_DS_ReleaseResultSet(). ";

%feature("docstring")  ReleaseResultSet "void
OGR_DS_ReleaseResultSet(OGRDataSourceH hDS, OGRLayerH hLayer)

Release results of OGR_DS_ExecuteSQL().

This function should only be used to deallocate OGRLayers resulting
from an OGR_DS_ExecuteSQL() call on the same OGRDataSource. Failure to
deallocate a results set before destroying the OGRDataSource may cause
errors.

This function is the same as the C++ method
OGRDataSource::ReleaseResultSet().

Parameters:
-----------

hDS:  an handle to the data source on which was executed an SQL query.

hLayer:  handle to the result of a previous OGR_DS_ExecuteSQL() call.
";

%feature("docstring")  TestCapability "int
OGR_DS_TestCapability(OGRDataSourceH hDS, const char *pszCap)

Test if capability is available.

One of the following data source capability names can be passed into
this function, and a TRUE or FALSE value will be returned indicating
whether or not the capability is available for this object.

ODsCCreateLayer: True if this datasource can create new layers.

The #define macro forms of the capability names should be used in
preference to the strings themselves to avoid mispelling.

This function is the same as the C++ method
OGRDataSource::TestCapability().

Parameters:
-----------

hDS:  handle to the data source against which to test the capability.

pszCapability:  the capability to test.

TRUE if capability available otherwise FALSE. ";

%feature("docstring")  GetLayerCount "int
OGR_DS_GetLayerCount(OGRDataSourceH hDS)

Get the number of layers in this data source.

This function is the same as the C++ method
OGRDataSource::GetLayerCount().

Parameters:
-----------

hDS:  handle to the data source from which to get the number of
layers.

layer count. ";

%feature("docstring")  GetLayer "OGRLayerH
OGR_DS_GetLayer(OGRDataSourceH hDS, int iLayer)

Fetch a layer by index.

The returned layer remains owned by the OGRDataSource and should not
be deleted by the application.

This function is the same as the C++ method OGRDataSource::GetLayer().

Parameters:
-----------

hDS:  handle to the data source from which to get the layer.

iLayer:  a layer number between 0 and OGR_DS_GetLayerCount()-1.

an handle to the layer, or NULL if iLayer is out of range or an error
occurs. ";

%feature("docstring")  GetName "const char*
OGR_DS_GetName(OGRDataSourceH hDS)

Returns the name of the data source.

This string should be sufficient to open the data source if passed to
the same OGRSFDriver that this data source was opened with, but it
need not be exactly the same string that was used to open the data
source. Normally this is a filename.

This function is the same as the C++ method OGRDataSource::GetName().

Parameters:
-----------

hDS:  handle to the data source to get the name from.

pointer to an internal name string which should not be modified or
freed by the caller. ";

%feature("docstring")  SyncToDisk "OGRErr
OGR_DS_SyncToDisk(OGRDataSourceH hDS)

Flush pending changes to disk.

This call is intended to force the datasource to flush any pending
writes to disk, and leave the disk file in a consistent state. It
would not normally have any effect on read-only datasources.

Some data sources do not implement this method, and will still return
OGRERR_NONE. An error is only returned if an error occurs while
attempting to flush to disk.

The default implementation of this method just calls the SyncToDisk()
method on each of the layers. Conceptionally, calling SyncToDisk() on
a datasource should include any work that might be accomplished by
calling SyncToDisk() on layers in that data source.

In any event, you should always close any opened datasource with
OGR_DS_Destroy() that will ensure all data is correctly flushed.

This method is the same as the C++ method OGRDataSource::SyncToDisk()

Parameters:
-----------

hDS:  handle to the data source

OGRERR_NONE if no error occurs (even if nothing is done) or an error
code. ";

%feature("docstring")  GetDriver "OGRSFDriverH
OGR_DS_GetDriver(OGRDataSourceH hDS)

Returns the driver that the dataset was opened with.

This method is the same as the C++ method OGRDataSource::GetDriver()

Parameters:
-----------

hDS:  handle to the datasource

NULL if driver info is not available, or pointer to a driver owned by
the OGRSFDriverManager. ";

%feature("docstring")  GetStyleTable "OGRStyleTableH
OGR_DS_GetStyleTable(OGRDataSourceH hDS) ";

%feature("docstring")  SetStyleTableDirectly "void
OGR_DS_SetStyleTableDirectly(OGRDataSourceH hDS, OGRStyleTableH
hStyleTable) ";

%feature("docstring")  SetStyleTable "void
OGR_DS_SetStyleTable(OGRDataSourceH hDS, OGRStyleTableH hStyleTable)
";

}
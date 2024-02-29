%feature("docstring") GDALDatasetShadow "
Python proxy of a :cpp:class:`GDALDataset`.

Since GDAL 3.8, a Dataset can be used as a context manager.
When exiting the context, the Dataset will be closed and
data will be written to disk.
"

%extend GDALDatasetShadow {

%feature("docstring")  AbortSQL "

Abort any SQL statement running in the data store.

Not implemented by all drivers. See :cpp:func:`GDALDataset::AbortSQL`.

Returns
-------
:py:const:`ogr.OGRERR_NONE` on success or :py:const:`ogr.OGRERR_UNSUPPORTED_OPERATION` if AbortSQL is not supported for this dataset.
";


%feature("docstring")  AddBand "

Adds a band to a :py:class:`Dataset`.

Not supported by all drivers.

Parameters
-----------
datatype: int
    the data type of the pixels in the new band
options: dict/list
    an optional dict or list of format-specific ``NAME=VALUE`` option strings.

Returns
-------
int:
   :py:const:`CE_None` on success or :py:const:`CE_Failure` on failure.

Examples
--------
>>> ds=gdal.GetDriverByName('MEM').Create('', 10, 10)
>>> ds.RasterCount
1
>>> ds.AddBand(gdal.GDT_Float32)
0
>>> ds.RasterCount
2
";

%feature("docstring")  AddFieldDomain "

Add a :py:class:`ogr.FieldDomain` to the dataset.

Only a few drivers support this operation. See :cpp:func:`GDALDataset::AddFieldDomain`.

Parameters
----------
fieldDomain : ogr.FieldDomain
              The field domain to add

Returns
--------
bool:
    ``True`` if the field domain was added, ``False`` in case of error.


";

%feature("docstring")  AddRelationship "

Add a :py:class:`Relationship` to the dataset.

See :cpp:func:`GDALDataset::AddRelationship`.

Parameters
----------
relationship : Relationship
               The relationship to add

Returns
-------
bool:
    ``True`` if the field domain was added, ``False`` in case of error.

";

%feature("docstring")  AdviseRead "

Advise driver of upcoming read requests.

See :cpp:func:`GDALDataset::AdviseRead`.

";

%feature("docstring")  BuildOverviews "

Build raster overview(s) for all bands.

See :cpp:func:`GDALDataset::BuildOverviews`

Parameters
----------
resampling : str, optional
             The resampling method to use. See :cpp:func:`GDALDataset::BuildOveriews`.
overviewlist : list
             A list of overview levels (decimation factors) to build, or an
             empty list to clear existing overviews.
callback : function, optional
             A progress callback function
callback_data: optional
             Optional data to be passed to callback function
options : dict/list, optional
             A dict or list of key=value options

Returns
-------
:py:const:`CE_Failure` if an error occurs, otherwise :py:const:`CE_None`.

Examples
--------
>>> import numpy as np
>>> ds = gdal.GetDriverByName('GTiff').Create('test.tif', 12, 12)
>>> ds.GetRasterBand(1).WriteArray(np.arange(12*12).reshape((12, 12)))
0
>>> ds.BuildOverviews('AVERAGE', [2, 4])
0
>>> ds.GetRasterBand(1).GetOverviewCount()
2
>>> ds.BuildOverviews(overviewlist=[])
0
>>> ds.GetRasterBand(1).GetOverviewCount()
0
";

%feature("docstring")  ClearStatistics "

Clear statistics

See :cpp:func:`GDALDatset::ClearStatistics`.

";

%feature("docstring")  Close "
Closes opened dataset and releases allocated resources.

This method can be used to force the dataset to close
when one more references to the dataset are still
reachable. If :py:meth:`Close` is never called, the dataset will
be closed automatically during garbage collection.

In most cases, it is preferable to open or create a dataset
using a context manager instead of calling :py:meth:`Close`
directly.

";

%feature("docstring")  CommitTransaction "
Commits a transaction, for `Datasets` that support transactions.

See :cpp:func:`GDALDataset::CommitTransaction`.
";

%feature("docstring")  CopyLayer "

Duplicate an existing :py:class:`ogr.Layer`.

See :cpp:func:`GDALDAtaset::CopyLayer`.

Parameters
----------
src_layer : ogr.Layer
            source layer
new_name : str
           name of the layer to create
options : dict/list
          a dict or list of name=value driver-specific creation options

Returns
-------
ogr.Layer, or ``None`` if an error occurs
";

%feature("docstring") CreateLayer "

Create a new layer in a vector Dataset.

Parameters
----------
name : string
       the name for the new layer.  This should ideally not
       match any existing layer on the datasource.
srs : osr.SpatialReference, default=None
      the coordinate system to use for the new layer, or ``None`` if
      no coordinate system is available.
geom_type : int, default = :py:const:`ogr.wkbUnknown`
      geometry type for the layer.  Use :py:const:`ogr.wkbUnknown` if there
      are no constraints on the types geometry to be written.
options : dict/list, optional
      Driver-specific dict or list of name=value options

Returns
-------
ogr.Layer or ``None`` on failure.


Examples
--------
>>> ds = gdal.GetDriverByName('GPKG').Create('test.gpkg', 0, 0)
>>> ds.GetLayerCount()
0
>>> lyr = ds.CreateLayer('poly', geom_type=ogr.wkbPolygon)
>>> ds.GetLayerCount()
1

";

%feature("docstring") CreateMaskBand "

Adds a mask band to the dataset.

See :cpp:func:`GDALDataset::CreateMaskBand`.

Parameters
----------
flags : int

Returns
-------
int
    :py:const:`CE_Failure` if an error occurs, otherwise :py:const:`CE_None`.

";

%feature("docstring")  DeleteFieldDomain "

Removes a field domain from the Dataset.

Parameters
----------
name : str
       Name of the field domain to delete

Returns
-------
bool
     ``True`` if the field domain was removed, otherwise ``False``.

";

%feature("docstring")  DeleteRelationship "

Removes a relationship from the Dataset.

Parameters
----------
name : str
       Name of the relationship to remove.

Returns
-------
bool
     ``True`` if the relationship  was removed, otherwise ``False``.


";

%feature("docstring")  FlushCache "

Flush all write-cached data to disk.

See :cpp:func:`GDALDataset::FlushCache`.

Returns
-------
int
    `gdal.CE_None` in case of success
";


%feature("docstring")  GetDriver "

Fetch the driver used to open or create this :py:class:`Dataset`.

";

%feature("docstring")  GetFieldDomain "

Get a field domain from its name.

Parameters
----------
name: str
      The name of the field domain

Returns
-------
ogr.FieldDomain, or ``None`` if it is not found.
";

%feature("docstring")  GetFieldDomainNames "

Get a list of the names of all field domains stored in the dataset.

Parameters
----------
options: dict/list, optional
         Driver-specific options determining how attributes should
         be retrieved.

Returns
-------
list, or ``None`` if no field domains are stored in the dataset.
";

%feature("docstring")  GetFileList "

Returns a list of files believed to be part of this dataset.
See :cpp:func:`GDALGetFileList`.

";

%feature("docstring")  GetGCPCount "

Get number of GCPs. See :cpp:func:`GDALGetGCPCount`.

Returns
--------
int

";

%feature("docstring")  GetGCPProjection "

Return a WKT representation of the GCP spatial reference.

Returns
--------
string

";

%feature("docstring")  GetGCPSpatialRef "

Get output spatial reference system for GCPs.

See :cpp:func:`GDALGetGCPSpatialRef`

";

%feature("docstring")  GetGCPs "

Get the GCPs. See :cpp:func:`GDALGetGCPs`.

Returns
--------
tuple
    a tuple of :py:class:`GCP` objects.

";

%feature("docstring")  GetLayerByIndex "

Fetch a layer by index.

Parameters
----------
index : int
    A layer number between 0 and ``GetLayerCount() - 1``

Returns
-------
ogr.Layer

";


%feature("docstring")  GetLayerByNAme "

Fetch a layer by name.

Parameters
----------
layer_name : str

Returns
-------
ogr.Layer

";

%feature("docstring")  GetLayerCount "

Get the number of layers in this dataset.

Returns
-------
int

";


%feature("docstring")  GetNextFeature "

Fetch the next available feature from this dataset.

This method is intended for the few drivers where
:py:meth:`OGRLayer.GetNextFeature` is not efficient, but in general
:py:meth:`OGRLayer.GetNextFeature` is a more natural API.

See :cpp:func:`GDALDataset::GetNextFeature`.

Returns
-------
ogr.Feature

";

%feature("docstring")  GetProjection "

Return a WKT representation of the dataset spatial reference.
Equivalent to :py:meth:`GetProjectionRef`.

Returns
-------
str

";

%feature("docstring")  GetProjectionRef "

Return a WKT representation of the dataset spatial reference.

Returns
-------
str

";

%feature("docstring")  GetGeoTransform "

Fetch the affine transformation coefficients.

See :cpp:func:`GDALGetGeoTransform`.

Parameters
-----------
can_return_null : bool, default=False
    if ``True``, return ``None`` instead of the default transformation
    if the transformation for this :py:class:`Dataset` has not been defined.

Returns
-------
tuple:
    a 6-member tuple representing the transformation coefficients


";

%feature("docstring")  GetRasterBand "

Fetch a :py:class:`Band` band from a :py:class:`Dataset`. See :cpp:func:`GDALGetRasterBand`.

Parameters
-----------
nBand : int
    the index of the band to fetch, from 1 to :py:attr:`RasterCount`

Returns
--------
Band:
    the :py:class:`Band`, or ``None`` on error.

";

%feature("docstring")  GetRelationship "

Get a relationship from its name.

Returns
-------
Relationship, or ``None`` if not found.
";

%feature("docstring")  GetRelationshipNames "

Get a list of the names of all relationships stored in the dataset.

Parameters
----------
options : dict/list, optional
    driver-specific options determining how the relationships should be retrieved

";

%feature("docstring")  GetRootGroup "

Return the root :py:class:`Group` of this dataset.
Only value for multidimensional datasets.

Returns
-------
Group

";

%feature("docstring")  GetSpatialRef "

Fetch the spatial reference for this dataset.

Returns
--------
osr.SpatialReference

";

%feature("docstring")  GetStyleTable "

Returns dataset style table.

Returns
-------
ogr.StyleTable

";

%feature("docstring")  IsLayerPrivate "

Parameters
----------
index : int
        Index o layer to check

Returns
-------
bool
     ``True`` if the layer is a private or system table, ``False`` otherwise


";

%feature("docstring")  RasterCount "

The number of bands in this dataset.

";

%feature("docstring")  RasterXSize "

Raster width in pixels. See :cpp:func:`GDALGetRasterXSize`.

";

%feature("docstring")  RasterYSize "

Raster height in pixels. See :cpp:func:`GDALGetRasterYSize`.

";

%feature("docstring")  ResetReading "

Reset feature reading to start on the first feature.

This affects :py:meth:`GetNextFeature`.

Depending on drivers, this may also have the side effect of calling
:py:meth:`OGRLayer.ResetReading` on the layers of this dataset.

";

%feature("docstring")  RollbackTransaction "

Roll back a Dataset to its state before the start of the current transaction.

For datasets that support transactions.

Returns
-------
int
    If no transaction is active, or the rollback fails, will return
    :py:const:`OGRERR_FAILURE`. Datasources which do not support transactions will
    always return :py:const:`OGRERR_UNSUPPORTED_OPERATION`.

";

%feature("docstring")  SetGCPs "
";

%feature("docstring")  SetGeoTransform "

Set the affine transformation coefficients.

See :py:meth:`GetGeoTransform` for details on the meaning of the coefficients.

Parameters
----------
argin : tuple

Returns
-------
:py:const:`CE_Failure` if an error occurs, otherwise :py:const:`CE_None`.

";

%feature("docstring")  SetProjection "

Set the spatial reference system for this dataset.

See :cpp:func:`GDALDataset::SetProjection`.

Parameters
----------
prj:
   The projection string in OGC WKT or PROJ.4 format

Returns
-------
:py:const:`CE_Failure` if an error occurs, otherwise :py:const:`CE_None`.

";

%feature("docstring")  SetSpatialRef "

Set the spatial reference system for this dataset.

Parameters
----------
srs : SpatialReference

Returns
-------
:py:const:`CE_Failure` if an error occurs, otherwise :py:const:`CE_None`.

";

%feature("docstring")  SetStyleTable "

Set dataset style table

Parameters
----------
table : ogr.StyleTable
";

%feature("docstring")  StartTransaction "

Creates a transaction. See :cpp:func:`GDALDataset::StartTransaction`.

Returns
-------
int
    If starting the transaction fails, will return
    :py:const:`ogr.OGRERR_FAILURE`. Datasources which do not support transactions will
    always return :py:const:`OGRERR_UNSUPPORTED_OPERATION`.

";

%feature("docstring")  TestCapability "

Test if a capability is available.

Parameters
----------
cap : str
   Name of the capability (e.g., :py:const:`ogr.ODsCTransactions`)

Returns
-------
bool
    ``True`` if the capability is available, ``False`` if invalid or unavailable

Examples
--------
>>> ds = gdal.GetDriverByName('ESRI Shapefile').Create('test.shp', 0, 0, 0, gdal.GDT_Unknown)
>>> ds.TestCapability(ogr.ODsCTransactions)
False
>>> ds.TestCapability(ogr.ODsCMeasuredGeometries)
True
>>> ds.TestCapability(gdal.GDsCAddRelationship)
False

";

%feature("docstring")  UpdateFieldDomain "

Update an existing field domain by replacing its definition.

The existing field domain with matching name will be replaced.

Requires the :py:const:`ogr.ODsCUpdateFieldDomain` datasset capability.

Parameters
----------
fieldDomain : ogr.FieldDomain
    Updated field domain.

Returns
-------
bool
    ``True`` in case of success

";

%feature("docstring")  UpdateRelationship "

Update an existing relationship by replacing its definition.

The existing relationship with matching name will be replaced.

Requires the :py:const:`gdal.GDsCUpdateFieldDomain` dataset capability.

Parameters
----------
relationship : Relationship
    Updated relationship

Returns
-------
bool
    ``True`` in case of success

";

}

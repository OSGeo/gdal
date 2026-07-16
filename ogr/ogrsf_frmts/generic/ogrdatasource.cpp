/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The generic portions of the GDALDataset class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogrsf_frmts.h"
#include "ogr_api.h"
#include "ograpispy.h"

/************************************************************************/
/*                           OGRDataSource()                            */
/************************************************************************/

OGRDataSource::OGRDataSource() = default;

/************************************************************************/
/*                           ~OGRDataSource()                           */
/************************************************************************/

OGRDataSource::~OGRDataSource() = default;

/************************************************************************/
/*                         DestroyDataSource()                          */
/************************************************************************/

//! @cond Doxygen_Suppress
void OGRDataSource::DestroyDataSource(OGRDataSource *poDS)

{
    delete poDS;
}

//! @endcond

/************************************************************************/
/*                           OGR_DS_Destroy()                           */
/************************************************************************/

/**
  \brief Closes opened datasource and releases allocated resources.

   This method is the same as the C++ method OGRDataSource::DestroyDataSource().

  @deprecated Use GDALClose()

  @param hDS handle to allocated datasource object.
*/
void OGR_DS_Destroy(OGRDataSourceH hDS)

{
    if (hDS == nullptr)
        return;
    GDALClose(reinterpret_cast<GDALDatasetH>(hDS));
    // VALIDATE_POINTER0( hDS, "OGR_DS_Destroy" );
}

/************************************************************************/
/*                          OGR_DS_Reference()                          */
/************************************************************************/

int OGR_DS_Reference(OGRDataSourceH hDataSource)

{
    VALIDATE_POINTER1(hDataSource, "OGR_DS_Reference", 0);

    return GDALDataset::FromHandle(hDataSource)->Reference();
}

/************************************************************************/
/*                         OGR_DS_Dereference()                         */
/************************************************************************/

int OGR_DS_Dereference(OGRDataSourceH hDataSource)

{
    VALIDATE_POINTER1(hDataSource, "OGR_DS_Dereference", 0);

    return GDALDataset::FromHandle(hDataSource)->Dereference();
}

/************************************************************************/
/*                         OGR_DS_GetRefCount()                         */
/************************************************************************/

int OGR_DS_GetRefCount(OGRDataSourceH hDataSource)

{
    VALIDATE_POINTER1(hDataSource, "OGR_DS_GetRefCount", 0);

    return GDALDataset::FromHandle(hDataSource)->GetRefCount();
}

/************************************************************************/
/*                     OGR_DS_GetSummaryRefCount()                      */
/************************************************************************/

int OGR_DS_GetSummaryRefCount(OGRDataSourceH hDataSource)

{
    VALIDATE_POINTER1(hDataSource, "OGR_DS_GetSummaryRefCount", 0);

    return GDALDataset::FromHandle(hDataSource)->GetSummaryRefCount();
}

/************************************************************************/
/*                         OGR_DS_CreateLayer()                         */
/************************************************************************/

/**
\brief This function attempts to create a new layer on the data source with the indicated name, coordinate system, geometry type.

The papszOptions argument
can be used to control driver specific creation options.  These options are
normally documented in the format specific documentation.

@deprecated Use GDALDatasetCreateLayer()

 @param hDS The dataset handle.
 @param pszName the name for the new layer.  This should ideally not
match any existing layer on the datasource.
 @param hSpatialRef handle to the coordinate system to use for the new layer,
or NULL if no coordinate system is available. The driver might only increase
the reference counter of the object to take ownership, and not make a full copy,
so do not use OSRDestroySpatialReference(), but OSRRelease() instead when you
are done with the object.
 @param eType the geometry type for the layer.  Use wkbUnknown if there
are no constraints on the types geometry to be written.
 @param papszOptions a StringList of name=value options.  Options are driver
specific.

 @return NULL is returned on failure, or a new OGRLayer handle on success.

<b>Example:</b>

\code
#include "ogrsf_frmts.h"
#include "cpl_string.h"

...

OGRLayerH *hLayer;
char     **papszOptions;

if( OGR_DS_TestCapability( hDS, ODsCCreateLayer ) )
{
    ...
}

papszOptions = CSLSetNameValue( papszOptions, "DIM", "2" );
hLayer = OGR_DS_CreateLayer( hDS, "NewLayer", NULL, wkbUnknown,
                             papszOptions );
CSLDestroy( papszOptions );

if( hLayer == NULL )
{
    ...
}
\endcode
*/

OGRLayerH OGR_DS_CreateLayer(OGRDataSourceH hDS, const char *pszName,
                             OGRSpatialReferenceH hSpatialRef,
                             OGRwkbGeometryType eType, char **papszOptions)

{
    VALIDATE_POINTER1(hDS, "OGR_DS_CreateLayer", nullptr);

    if (pszName == nullptr)
    {
        CPLError(CE_Failure, CPLE_ObjectNull,
                 "Name was NULL in OGR_DS_CreateLayer");
        return nullptr;
    }
    OGRLayerH hLayer =
        OGRLayer::ToHandle(GDALDataset::FromHandle(hDS)->CreateLayer(
            pszName, OGRSpatialReference::FromHandle(hSpatialRef), eType,
            papszOptions));

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_DS_CreateLayer(hDS, pszName, hSpatialRef, eType, papszOptions,
                                 hLayer);
#endif

    return hLayer;
}

/************************************************************************/
/*                          OGR_DS_CopyLayer()                          */
/************************************************************************/

/**
 \brief Duplicate an existing layer.

 This function creates a new layer, duplicate the field definitions of the
 source layer and then duplicate each features of the source layer.
 The papszOptions argument
 can be used to control driver specific creation options.  These options are
 normally documented in the format specific documentation.
 The source layer may come from another dataset.

 @deprecated Use GDALDatasetCopyLayer()

 @param hDS handle to the data source where to create the new layer
 @param hSrcLayer handle to the source layer.
 @param pszNewName the name of the layer to create.
 @param papszOptions a StringList of name=value options.  Options are driver
                     specific.

 @return a handle to the layer, or NULL if an error occurs.
*/

OGRLayerH OGR_DS_CopyLayer(OGRDataSourceH hDS, OGRLayerH hSrcLayer,
                           const char *pszNewName, char **papszOptions)

{
    VALIDATE_POINTER1(hDS, "OGR_DS_CopyLayer", nullptr);
    VALIDATE_POINTER1(hSrcLayer, "OGR_DS_CopyLayer", nullptr);
    VALIDATE_POINTER1(pszNewName, "OGR_DS_CopyLayer", nullptr);

    return OGRLayer::ToHandle(GDALDataset::FromHandle(hDS)->CopyLayer(
        OGRLayer::FromHandle(hSrcLayer), pszNewName, papszOptions));
}

/************************************************************************/
/*                         OGR_DS_DeleteLayer()                         */
/************************************************************************/

/**
 \brief Delete the indicated layer from the datasource.

 If this method is supported
 the ODsCDeleteLayer capability will test TRUE on the OGRDataSource.

 @deprecated Use GDALDatasetDeleteLayer()

 @param hDS handle to the datasource
 @param iLayer the index of the layer to delete.

 @return OGRERR_NONE on success, or OGRERR_UNSUPPORTED_OPERATION if deleting
 layers is not supported for this datasource.
*/

OGRErr OGR_DS_DeleteLayer(OGRDataSourceH hDS, int iLayer)

{
    VALIDATE_POINTER1(hDS, "OGR_DS_DeleteLayer", OGRERR_INVALID_HANDLE);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_DS_DeleteLayer(reinterpret_cast<GDALDatasetH>(hDS), iLayer);
#endif

    OGRErr eErr = GDALDataset::FromHandle(hDS)->DeleteLayer(iLayer);

    return eErr;
}

/************************************************************************/
/*                       OGR_DS_GetLayerByName()                        */
/************************************************************************/

/**
 \brief Fetch a layer by name.

 The returned layer remains owned by the
 OGRDataSource and should not be deleted by the application.

 @deprecated Use GDALDatasetGetLayerByName()

 @param hDS handle to the data source from which to get the layer.
 @param pszLayerName Layer the layer name of the layer to fetch.

 @return a handle to the layer, or NULL if the layer is not found
 or an error occurs.
*/

OGRLayerH OGR_DS_GetLayerByName(OGRDataSourceH hDS, const char *pszLayerName)

{
    VALIDATE_POINTER1(hDS, "OGR_DS_GetLayerByName", nullptr);

    OGRLayerH hLayer = OGRLayer::ToHandle(
        GDALDataset::FromHandle(hDS)->GetLayerByName(pszLayerName));

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_DS_GetLayerByName(reinterpret_cast<GDALDatasetH>(hDS),
                                    pszLayerName, hLayer);
#endif

    return hLayer;
}

/************************************************************************/
/*                         OGR_DS_ExecuteSQL()                          */
/************************************************************************/

/**
 \brief Execute an SQL statement against the data store.

 The result of an SQL query is either NULL for statements that are in error,
 or that have no results set, or an OGRLayer handle representing a results
 set from the query.  Note that this OGRLayer is in addition to the layers
 in the data store and must be destroyed with
 OGR_DS_ReleaseResultSet() before the data source is closed
 (destroyed).

 For more information on the SQL dialect supported internally by OGR
 review the <a href="https://gdal.org/user/ogr_sql_dialect.html">OGR SQL</a> document.  Some drivers (i.e.
 Oracle and PostGIS) pass the SQL directly through to the underlying RDBMS.

 The <a href="https://gdal.org/user/sql_sqlite_dialect.html">SQLITE dialect</a>
 can also be used.

 @deprecated Use GDALDatasetExecuteSQL()

 @param hDS handle to the data source on which the SQL query is executed.
 @param pszStatement the SQL statement to execute.
 @param hSpatialFilter handle to a geometry which represents a spatial filter. Can be NULL.
 @param pszDialect allows control of the statement dialect. If set to NULL, the
OGR SQL engine will be used, except for RDBMS drivers that will use their dedicated SQL engine,
unless OGRSQL is explicitly passed as the dialect. The SQLITE dialect
can also be used.

 @return a handle to a OGRLayer containing the results of the query.
 Deallocate with OGR_DS_ReleaseResultSet().
*/

OGRLayerH OGR_DS_ExecuteSQL(OGRDataSourceH hDS, const char *pszStatement,
                            OGRGeometryH hSpatialFilter, const char *pszDialect)

{
    VALIDATE_POINTER1(hDS, "OGR_DS_ExecuteSQL", nullptr);

    OGRLayerH hLayer =
        OGRLayer::ToHandle(GDALDataset::FromHandle(hDS)->ExecuteSQL(
            pszStatement, OGRGeometry::FromHandle(hSpatialFilter), pszDialect));

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_DS_ExecuteSQL(reinterpret_cast<GDALDatasetH>(hDS),
                                pszStatement, hSpatialFilter, pszDialect,
                                hLayer);
#endif

    return hLayer;
}

/************************************************************************/
/*                      OGR_DS_ReleaseResultSet()                       */
/************************************************************************/

/**
 \brief Release results of OGR_DS_ExecuteSQL().

 This function should only be used to deallocate OGRLayers resulting from
 an OGR_DS_ExecuteSQL() call on the same OGRDataSource.
 Failure to deallocate a results set before destroying the OGRDataSource
 may cause errors.

 @deprecated Use GDALDatasetReleaseResultSet()

 @param hDS a handle to the data source on which was executed an
 SQL query.
 @param hLayer handle to the result of a previous OGR_DS_ExecuteSQL() call.
*/

void OGR_DS_ReleaseResultSet(OGRDataSourceH hDS, OGRLayerH hLayer)

{
    VALIDATE_POINTER0(hDS, "OGR_DS_ReleaseResultSet");

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_DS_ReleaseResultSet(reinterpret_cast<GDALDatasetH>(hDS),
                                      hLayer);
#endif

    GDALDataset::FromHandle(hDS)->ReleaseResultSet(
        OGRLayer::FromHandle(hLayer));
}

/************************************************************************/
/*                       OGR_DS_TestCapability()                        */
/************************************************************************/

/**
 \brief Test if capability is available.

 One of the following data source capability names can be passed into this
 function, and a TRUE or FALSE value will be returned indicating whether
 or not the capability is available for this object.

 <ul>
  <li> <b>ODsCCreateLayer</b>: True if this datasource can create new layers.
  <li> <b>ODsCDeleteLayer</b>: True if this datasource can delete existing layers.<p>
  <li> <b>ODsCCreateGeomFieldAfterCreateLayer</b>: True if the layers of this
        datasource support CreateGeomField() just after layer creation.<p>
  <li> <b>ODsCCurveGeometries</b>: True if this datasource supports writing curve geometries.
                                   In that case, OLCCurveGeometries must also be declared in layers of that dataset.<p>
  <p>
 </ul>

 The \#define macro forms of the capability names should be used in preference
 to the strings themselves to avoid misspelling.

 @deprecated Use GDALDatasetTestCapability()

 @param hDS handle to the data source against which to test the capability.
 @param pszCapability the capability to test.

 @return TRUE if capability available otherwise FALSE.
*/

int OGR_DS_TestCapability(OGRDataSourceH hDS, const char *pszCapability)

{
    VALIDATE_POINTER1(hDS, "OGR_DS_TestCapability", 0);
    VALIDATE_POINTER1(pszCapability, "OGR_DS_TestCapability", 0);

    return GDALDataset::FromHandle(hDS)->TestCapability(pszCapability);
}

/************************************************************************/
/*                        OGR_DS_GetLayerCount()                        */
/************************************************************************/

/**
 \brief Get the number of layers in this data source.

 @deprecated Use GDALDatasetGetLayerCount()

 @param hDS handle to the data source from which to get the number of layers.
 @return layer count.

*/
int OGR_DS_GetLayerCount(OGRDataSourceH hDS)

{
    VALIDATE_POINTER1(hDS, "OGR_DS_GetLayerCount", 0);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_DS_GetLayerCount(reinterpret_cast<GDALDatasetH>(hDS));
#endif

    return GDALDataset::FromHandle(hDS)->GetLayerCount();
}

/************************************************************************/
/*                          OGR_DS_GetLayer()                           */
/************************************************************************/

/**
 \brief Fetch a layer by index.

 The returned layer remains owned by the
 OGRDataSource and should not be deleted by the application.

 @deprecated Use GDALDatasetGetLayer()

 @param hDS handle to the data source from which to get the layer.
 @param iLayer a layer number between 0 and OGR_DS_GetLayerCount()-1.

 @return a handle to the layer, or NULL if iLayer is out of range
 or an error occurs.
*/

OGRLayerH OGR_DS_GetLayer(OGRDataSourceH hDS, int iLayer)

{
    VALIDATE_POINTER1(hDS, "OGR_DS_GetLayer", nullptr);

    OGRLayerH hLayer =
        OGRLayer::ToHandle(GDALDataset::FromHandle(hDS)->GetLayer(iLayer));

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_DS_GetLayer(reinterpret_cast<GDALDatasetH>(hDS), iLayer,
                              hLayer);
#endif

    return hLayer;
}

/************************************************************************/
/*                           OGR_DS_GetName()                           */
/************************************************************************/

/**
 \brief Returns the name of the data source.

  This string should be sufficient to
 open the data source if passed to the same OGRSFDriver that this data
 source was opened with, but it need not be exactly the same string that
 was used to open the data source.  Normally this is a filename.

 @deprecated Use GDALGetDescription()

 @param hDS handle to the data source to get the name from.
 @return pointer to an internal name string which should not be modified
 or freed by the caller.
*/

const char *OGR_DS_GetName(OGRDataSourceH hDS)

{
    VALIDATE_POINTER1(hDS, "OGR_DS_GetName", nullptr);

    return GDALDataset::FromHandle(hDS)->GetDescription();
}

/************************************************************************/
/*                         OGR_DS_SyncToDisk()                          */
/************************************************************************/

OGRErr OGR_DS_SyncToDisk(OGRDataSourceH hDS)

{
    VALIDATE_POINTER1(hDS, "OGR_DS_SyncToDisk", OGRERR_INVALID_HANDLE);

    GDALDataset::FromHandle(hDS)->FlushCache(false);
    if (CPLGetLastErrorType() != 0)
        return OGRERR_FAILURE;
    else
        return OGRERR_NONE;
}

/************************************************************************/
/*                          OGR_DS_GetDriver()                          */
/************************************************************************/

/**
\brief Returns the driver that the dataset was opened with.

NOTE: It is *NOT* safe to cast the returned handle to
OGRSFDriver*. If a C++ object is needed, the handle should be cast to GDALDriver*.

@deprecated Use GDALGetDatasetDriver()

@param hDS handle to the datasource
@return NULL if driver info is not available, or pointer to a driver owned
by the OGRSFDriverManager.
*/

OGRSFDriverH OGR_DS_GetDriver(OGRDataSourceH hDS)

{
    VALIDATE_POINTER1(hDS, "OGR_DS_GetDriver", nullptr);

    return reinterpret_cast<OGRSFDriverH>(
        reinterpret_cast<OGRDataSource *>(hDS)->GetDriver());
}

/************************************************************************/
/*                        OGR_DS_GetStyleTable()                        */
/************************************************************************/

OGRStyleTableH OGR_DS_GetStyleTable(OGRDataSourceH hDS)

{
    VALIDATE_POINTER1(hDS, "OGR_DS_GetStyleTable", nullptr);

    return reinterpret_cast<OGRStyleTableH>(
        GDALDataset::FromHandle(hDS)->GetStyleTable());
}

/************************************************************************/
/*                    OGR_DS_SetStyleTableDirectly()                    */
/************************************************************************/

void OGR_DS_SetStyleTableDirectly(OGRDataSourceH hDS,
                                  OGRStyleTableH hStyleTable)

{
    VALIDATE_POINTER0(hDS, "OGR_DS_SetStyleTableDirectly");

    GDALDataset::FromHandle(hDS)->SetStyleTableDirectly(
        reinterpret_cast<OGRStyleTable *>(hStyleTable));
}

/************************************************************************/
/*                        OGR_DS_SetStyleTable()                        */
/************************************************************************/

void OGR_DS_SetStyleTable(OGRDataSourceH hDS, OGRStyleTableH hStyleTable)

{
    VALIDATE_POINTER0(hDS, "OGR_DS_SetStyleTable");
    VALIDATE_POINTER0(hStyleTable, "OGR_DS_SetStyleTable");

    GDALDataset::FromHandle(hDS)->SetStyleTable(
        reinterpret_cast<OGRStyleTable *>(hStyleTable));
}

/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGR C API "Spy"
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even.rouault at spatialys.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef _OGRAPISPY_H_INCLUDED
#define _OGRAPISPY_H_INCLUDED

#include "gdal.h"

/**
 * \file ograpispy.h
 * 
 * OGR C API spy.
 *
 * If GDAL is compiled with OGRAPISPY_ENABLED defined (which is the case for a
 * DEBUG build), a mechanism to trace calls to the OGR *C* API is available
 * (calls to the C++ API will not be traced)
 *
 * Provided there is compile-time support, the mechanism must also be enabled at
 * runtime by setting the OGR_API_SPY_FILE configuration option
 * to a file where the calls to the OGR C API will be dumped (stdout and stderr
 * are recognized as special strings to name the standard output and error files).
 * The traced calls are outputted as a OGR Python script.
 *
 * Only calls that may have side-effects to the behaviour of drivers are traced.
 *
 * If a file-based datasource is open in update mode, a snapshot of its initial
 * state is stored in a 'snapshot' directory, and then a copy of it is made as
 * the working datasource. That way, the generated script can be executed in a
 * reproducible way. The path for snapshots is the current working directory by
 * default, and can be changed by setting the OGR_API_SPY_SNAPSHOT_PATH
 * configuration option. If it is set to NO, the snapshot feature will be disabled.
 * The reliability of snapshoting relies on if the dataset correctly implements
 * GetFileList() (for multi-file datasources)
 *
 * @since GDAL 2.0
 */


#ifdef DEBUG
#define OGRAPISPY_ENABLED
#endif

#ifdef OGRAPISPY_ENABLED

CPL_C_START

extern int bOGRAPISpyEnabled;

int OGRAPISpyOpenTakeSnapshot(const char* pszName, int bUpdate);
void OGRAPISpyOpen(const char* pszName, int bUpdate, int iSnapshot,
                   GDALDatasetH* phDS);
void OGRAPISpyPreClose(OGRDataSourceH hDS);
void OGRAPISpyPostClose(OGRDataSourceH hDS);
void OGRAPISpyCreateDataSource(OGRSFDriverH hDriver, const char* pszName,
                               char** papszOptions, OGRDataSourceH hDS);
void OGRAPISpyDeleteDataSource(OGRSFDriverH hDriver, const char* pszName);

void OGRAPISpy_DS_GetLayerCount( OGRDataSourceH hDS );
void OGRAPISpy_DS_GetLayer( OGRDataSourceH hDS, int iLayer, OGRLayerH hLayer );
void OGRAPISpy_DS_GetLayerByName( OGRDataSourceH hDS, const char* pszLayerName,
                                  OGRLayerH hLayer );
void OGRAPISpy_DS_ExecuteSQL( OGRDataSourceH hDS, 
                              const char *pszStatement,
                              OGRGeometryH hSpatialFilter,
                              const char *pszDialect,
                              OGRLayerH hLayer);
void OGRAPISpy_DS_ReleaseResultSet( OGRDataSourceH hDS, OGRLayerH hLayer);

void OGRAPISpy_DS_CreateLayer( OGRDataSourceH hDS, 
                               const char * pszName,
                               OGRSpatialReferenceH hSpatialRef,
                               OGRwkbGeometryType eType,
                               char ** papszOptions,
                               OGRLayerH hLayer);
void OGRAPISpy_DS_DeleteLayer( OGRDataSourceH hDS, int iLayer );

void OGRAPISpy_Dataset_StartTransaction( GDALDatasetH hDS, int bForce );
void OGRAPISpy_Dataset_CommitTransaction( GDALDatasetH hDS );
void OGRAPISpy_Dataset_RollbackTransaction( GDALDatasetH hDS );

void OGRAPISpy_L_GetFeatureCount( OGRLayerH hLayer, int bForce );
void OGRAPISpy_L_GetExtent( OGRLayerH hLayer, int bForce );
void OGRAPISpy_L_GetExtentEx( OGRLayerH hLayer, int iGeomField, int bForce );
void OGRAPISpy_L_SetAttributeFilter( OGRLayerH hLayer, const char* pszFilter );
void OGRAPISpy_L_GetFeature( OGRLayerH hLayer, GIntBig nFeatureId );
void OGRAPISpy_L_SetNextByIndex( OGRLayerH hLayer, GIntBig nIndex );
void OGRAPISpy_L_GetNextFeature( OGRLayerH hLayer );
void OGRAPISpy_L_SetFeature( OGRLayerH hLayer, OGRFeatureH hFeat );
void OGRAPISpy_L_CreateFeature( OGRLayerH hLayer, OGRFeatureH hFeat );
void OGRAPISpy_L_CreateField( OGRLayerH hLayer, OGRFieldDefnH hField, 
                              int bApproxOK );
void OGRAPISpy_L_DeleteField( OGRLayerH hLayer, int iField );
void OGRAPISpy_L_ReorderFields( OGRLayerH hLayer, int* panMap );
void OGRAPISpy_L_ReorderField( OGRLayerH hLayer, int iOldFieldPos,
                               int iNewFieldPos );
void OGRAPISpy_L_AlterFieldDefn( OGRLayerH hLayer, int iField,
                                 OGRFieldDefnH hNewFieldDefn,
                                 int nFlags );
void OGRAPISpy_L_CreateGeomField( OGRLayerH hLayer, OGRGeomFieldDefnH hField, 
                                  int bApproxOK );
void OGRAPISpy_L_StartTransaction( OGRLayerH hLayer );
void OGRAPISpy_L_CommitTransaction( OGRLayerH hLayer );
void OGRAPISpy_L_RollbackTransaction( OGRLayerH hLayer );
void OGRAPISpy_L_GetLayerDefn( OGRLayerH hLayer );
void OGRAPISpy_L_FindFieldIndex( OGRLayerH hLayer, const char *pszFieldName,
                                 int bExactMatch );
void OGRAPISpy_L_GetSpatialRef( OGRLayerH hLayer );
void OGRAPISpy_L_TestCapability( OGRLayerH hLayer, const char* pszCap );
void OGRAPISpy_L_GetSpatialFilter( OGRLayerH hLayer );
void OGRAPISpy_L_SetSpatialFilter( OGRLayerH hLayer, OGRGeometryH hGeom );
void OGRAPISpy_L_SetSpatialFilterEx( OGRLayerH hLayer, int iGeomField,
                                     OGRGeometryH hGeom );
void OGRAPISpy_L_SetSpatialFilterRect( OGRLayerH hLayer,
                                       double dfMinX, double dfMinY, 
                                       double dfMaxX, double dfMaxY);
void OGRAPISpy_L_SetSpatialFilterRectEx( OGRLayerH hLayer, int iGeomField,
                                         double dfMinX, double dfMinY, 
                                         double dfMaxX, double dfMaxY);
void OGRAPISpy_L_ResetReading( OGRLayerH hLayer );
void OGRAPISpy_L_SyncToDisk( OGRLayerH hLayer );
void OGRAPISpy_L_DeleteFeature( OGRLayerH hLayer, GIntBig nFID );
void OGRAPISpy_L_GetFIDColumn( OGRLayerH hLayer );
void OGRAPISpy_L_GetGeometryColumn( OGRLayerH hLayer );
void OGRAPISpy_L_GetName( OGRLayerH hLayer );
void OGRAPISpy_L_GetGeomType( OGRLayerH hLayer );
void OGRAPISpy_L_SetIgnoredFields( OGRLayerH hLayer,
                                   const char** papszIgnoredFields );

void OGRAPISpy_FD_GetGeomType(OGRFeatureDefnH hDefn);
void OGRAPISpy_FD_GetFieldCount(OGRFeatureDefnH hDefn);
void OGRAPISpy_FD_GetFieldDefn(OGRFeatureDefnH hDefn, int iField,
                               OGRFieldDefnH hGeomField);
void OGRAPISpy_FD_GetFieldIndex(OGRFeatureDefnH hDefn, const char* pszFieldName);

void OGRAPISpy_Fld_GetXXXX(OGRFieldDefnH hField, const char* pszOp);

void OGRAPISpy_FD_GetGeomFieldCount(OGRFeatureDefnH hDefn);
void OGRAPISpy_FD_GetGeomFieldDefn(OGRFeatureDefnH hDefn, int iGeomField,
                                   OGRGeomFieldDefnH hGeomField);
void OGRAPISpy_FD_GetGeomFieldIndex(OGRFeatureDefnH hDefn, const char* pszFieldName);
void OGRAPISpy_GFld_GetXXXX(OGRGeomFieldDefnH hGeomField, const char* pszOp);

CPL_C_END

#endif /* OGRAPISPY_ENABLED */

#endif /*  _OGRAPISPY_H_INCLUDED */

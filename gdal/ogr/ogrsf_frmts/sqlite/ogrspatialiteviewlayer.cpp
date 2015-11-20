/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRSpatialiteViewLayer class, access to an existing spatialite view.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_sqlite.h"
#include <string>

CPL_CVSID("$Id$");

/************************************************************************/
/*                        OGRSpatialiteViewLayer()                         */
/************************************************************************/

OGRSpatialiteViewLayer::OGRSpatialiteViewLayer( OGRSQLiteDataSource *poDSIn )
{ // start tasks (non-db) are done in OGRSpatialiteLayer(), which runs first
    poDS = poDSIn;
    /* SpatiaLite v.2.4.0 (or any subsequent) is required
       to support 2.5D: if an obsolete version of the library
       is found we'll unconditionally activate 2D casting mode.
    */
    if ( poDS->IsSpatialiteDB() )
     bSpatialite2D = poDS->GetSpatialiteVersionNumber() < 24;
    bHasSpatialIndex = FALSE;
    bHasCheckedSpatialIndexTable = FALSE;
}

/************************************************************************/
/*                        ~OGRSpatialiteViewLayer()                        */
/************************************************************************/

OGRSpatialiteViewLayer::~OGRSpatialiteViewLayer()
{ // clean up done in ~OGRSpatialiteLayer()
}

/************************************************************************/
/*                             Initialize()       [move this to :done in OGRSpatialiteLayer]              */
/************************************************************************/

CPLErr OGRSpatialiteViewLayer::Initialize( const char *pszViewName,
                                       OGRSpatialiteLayerType eSpatialiteLayerType,
                                       int bDeferredCreation)
{
   // CPLDebug( "OGR", "-I-> OGRSpatialiteViewLayer::Initialize(%s): layer_type=[%d] database_type=[%d]", pszViewName, eSpatialiteLayerType,poDS->GetDatabaseType());
   return OGRSpatialiteLayer::Initialize(pszViewName,eSpatialiteLayerType,bDeferredCreation);
}
// Start of View specfic functions
/************************************************************************/
/*                            GetGeometryTable()      - for SpatialViews,                       */
/* the Table-Name that the geometry fields belongs to will be returned */
/************************************************************************/
const char * OGRSpatialiteViewLayer::GetGeometryTable()
{ // View specific function
    if (pszEscapedUnderlyingTableName != NULL)
     return pszEscapedUnderlyingTableName;
    return pszEscapedTableName;
}
/************************************************************************/
/*                            GetEscapedRowId      - for SpatialViews,                       */
/* the primary key of the view - as defined in  views_geometry_columns */
/************************************************************************/
const char * OGRSpatialiteViewLayer::GetEscapedRowId()
{ // View specific function
    CPLString osSQL;
    osSQL.Printf( "\"%s\"", OGRSQLiteEscapeName(pszFIDColumn).c_str());
    return osSQL.c_str();
}



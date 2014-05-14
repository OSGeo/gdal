/******************************************************************************
 * $Id$
 *
 * Project:  GeoPackage Translator
 * Purpose:  Implements OGRGeoPackageSelectLayer class
 * Author:   Even Rouault <even dot rouault at mines-paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_geopackage.h"

/************************************************************************/
/*                        OGRGeoPackageSelectLayer()                    */
/************************************************************************/

OGRGeoPackageSelectLayer::OGRGeoPackageSelectLayer( OGRGeoPackageDataSource *poDS,
                                            CPLString osSQLIn,
                                            sqlite3_stmt *hStmtIn,
                                            int bUseStatementForGetNextFeature,
                                            int bEmptyLayer ) : OGRGeoPackageLayer(poDS)

{
    BuildFeatureDefn( "SELECT", hStmtIn );

    if( bUseStatementForGetNextFeature )
    {
        m_poQueryStatement = hStmtIn;
        bDoStep = FALSE;
    }
    else
        sqlite3_finalize( hStmtIn );

    osSQLBase = osSQLIn;
    this->bEmptyLayer = bEmptyLayer;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGeoPackageSelectLayer::ResetReading()

{
    if( iNextShapeId > 0 )
    {
        OGRGeoPackageLayer::ResetReading();
    }
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRGeoPackageSelectLayer::GetNextFeature()
{
    if( bEmptyLayer )
        return NULL;

    return OGRGeoPackageLayer::GetNextFeature();
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

int OGRGeoPackageSelectLayer::GetFeatureCount( int bForce )
{
    if( bEmptyLayer )
        return 0;

    if( m_poAttrQuery == NULL &&
        EQUALN(osSQLBase, "SELECT COUNT(*) FROM", strlen("SELECT COUNT(*) FROM")) &&
        osSQLBase.ifind(" GROUP BY ") == std::string::npos &&
        osSQLBase.ifind(" UNION ") == std::string::npos &&
        osSQLBase.ifind(" INTERSECT ") == std::string::npos &&
        osSQLBase.ifind(" EXCEPT ") == std::string::npos )
        return 1;

    if( m_poAttrQuery != NULL || m_poFilterGeom != NULL )
        return OGRLayer::GetFeatureCount(bForce);

    CPLString osFeatureCountSQL("SELECT COUNT(*) FROM (");
    osFeatureCountSQL += osSQLBase;
    osFeatureCountSQL += ")";

    CPLDebug("GPKG", "Running %s", osFeatureCountSQL.c_str());

/* -------------------------------------------------------------------- */
/*      Execute.                                                        */
/* -------------------------------------------------------------------- */
    char *pszErrMsg = NULL;
    char **papszResult;
    int nRowCount, nColCount;
    int nResult = -1;

    if( sqlite3_get_table( m_poDS->GetDatabaseHandle(), osFeatureCountSQL, &papszResult, 
                           &nRowCount, &nColCount, &pszErrMsg ) != SQLITE_OK )
    {
        CPLDebug("GPKG", "Error: %s", pszErrMsg);
        sqlite3_free(pszErrMsg);
        return OGRLayer::GetFeatureCount(bForce);
    }

    if( nRowCount == 1 && nColCount == 1 )
    {
        nResult = atoi(papszResult[1]);
    }

    sqlite3_free_table( papszResult );

    return nResult;
}

/************************************************************************/
/*                           ResetStatement()                           */
/************************************************************************/

OGRErr OGRGeoPackageSelectLayer::ResetStatement()

{
    int rc;

    ClearStatement();

    iNextShapeId = 0;
    bDoStep = TRUE;

#ifdef DEBUG
    CPLDebug( "OGR_GPKG", "prepare(%s)", osSQLBase.c_str() );
#endif

    rc = sqlite3_prepare( m_poDS->GetDatabaseHandle(), osSQLBase, osSQLBase.size(),
                          &m_poQueryStatement, NULL );

    if( rc == SQLITE_OK )
    {
        return OGRERR_NONE;
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "In ResetStatement(): sqlite3_prepare(%s):\n  %s", 
                  osSQLBase.c_str(), sqlite3_errmsg(m_poDS->GetDatabaseHandle()) );
        m_poQueryStatement = NULL;
        return OGRERR_FAILURE;
    }
}

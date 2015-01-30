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

OGRGeoPackageSelectLayer::OGRGeoPackageSelectLayer( GDALGeoPackageDataset *poDS,
                                            CPLString osSQLIn,
                                            sqlite3_stmt *hStmtIn,
                                            int bUseStatementForGetNextFeature,
                                            int bEmptyLayer ) : OGRGeoPackageLayer(poDS)

{
    poBehaviour = new OGRSQLiteSelectLayerCommonBehaviour(poDS, this, osSQLIn, bEmptyLayer);
    BuildFeatureDefn( "SELECT", hStmtIn );

    if( bUseStatementForGetNextFeature )
    {
        m_poQueryStatement = hStmtIn;
        bDoStep = FALSE;
    }
    else
        sqlite3_finalize( hStmtIn );
}

/************************************************************************/
/*                       ~OGRGeoPackageSelectLayer()                    */
/************************************************************************/

OGRGeoPackageSelectLayer::~OGRGeoPackageSelectLayer()
{
    delete poBehaviour;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGeoPackageSelectLayer::ResetReading()
{
    poBehaviour->ResetReading();
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRGeoPackageSelectLayer::GetNextFeature()
{
    return poBehaviour->GetNextFeature();
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

GIntBig OGRGeoPackageSelectLayer::GetFeatureCount( int bForce )
{
    return poBehaviour->GetFeatureCount(bForce);
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
    CPLDebug( "OGR_GPKG", "prepare(%s)", poBehaviour->osSQLCurrent.c_str() );
#endif

    rc = sqlite3_prepare( m_poDS->GetDB(), poBehaviour->osSQLCurrent, poBehaviour->osSQLCurrent.size(),
                          &m_poQueryStatement, NULL );

    if( rc == SQLITE_OK )
    {
        return OGRERR_NONE;
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "In ResetStatement(): sqlite3_prepare(%s):\n  %s", 
                  poBehaviour->osSQLCurrent.c_str(), sqlite3_errmsg(m_poDS->GetDB()) );
        m_poQueryStatement = NULL;
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRGeoPackageSelectLayer::SetAttributeFilter( const char *pszQuery )
{
    return poBehaviour->SetAttributeFilter(pszQuery);
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRGeoPackageSelectLayer::SetSpatialFilter( int iGeomField, OGRGeometry * poGeomIn )

{
    poBehaviour->SetSpatialFilter(iGeomField, poGeomIn);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGeoPackageSelectLayer::TestCapability( const char * pszCap )
{
    return poBehaviour->TestCapability(pszCap);
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRGeoPackageSelectLayer::GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce)
{
    return poBehaviour->GetExtent(iGeomField, psExtent, bForce);
}

/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRSQLiteSingleFeatureLayer class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2010-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

CPL_CVSID("$Id$")

/************************************************************************/
/*                    OGRSQLiteSingleFeatureLayer()                     */
/************************************************************************/

OGRSQLiteSingleFeatureLayer::OGRSQLiteSingleFeatureLayer(
    const char* pszLayerName,
    int nValIn ) :
    nVal(nValIn),
    pszVal(NULL),
    poFeatureDefn(new OGRFeatureDefn( "SELECT" )),
    iNextShapeId(0)
{
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
    OGRFieldDefn oField( pszLayerName, OFTInteger );
    poFeatureDefn->AddFieldDefn( &oField );
}

/************************************************************************/
/*                    OGRSQLiteSingleFeatureLayer()                     */
/************************************************************************/

OGRSQLiteSingleFeatureLayer::OGRSQLiteSingleFeatureLayer(
    const char* pszLayerName,
    const char *pszValIn ) :
    nVal(0),
    pszVal(CPLStrdup(pszValIn)),
    poFeatureDefn(new OGRFeatureDefn( "SELECT" )),
    iNextShapeId(0)
{
    poFeatureDefn->Reference();
    OGRFieldDefn oField( pszLayerName, OFTString );
    poFeatureDefn->AddFieldDefn( &oField );
}

/************************************************************************/
/*                   ~OGRSQLiteSingleFeatureLayer()                     */
/************************************************************************/

OGRSQLiteSingleFeatureLayer::~OGRSQLiteSingleFeatureLayer()
{
    if( poFeatureDefn != NULL )
    {
        poFeatureDefn->Release();
        poFeatureDefn = NULL;
    }
    CPLFree(pszVal);
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRSQLiteSingleFeatureLayer::ResetReading()
{
    iNextShapeId = 0;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature * OGRSQLiteSingleFeatureLayer::GetNextFeature()
{
    if (iNextShapeId != 0)
        return NULL;

    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    if (pszVal)
        poFeature->SetField(0, pszVal);
    else
        poFeature->SetField(0, nVal);
    poFeature->SetFID(iNextShapeId ++);
    return poFeature;
}

/************************************************************************/
/*                            GetLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn * OGRSQLiteSingleFeatureLayer::GetLayerDefn()
{
    return poFeatureDefn;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRSQLiteSingleFeatureLayer::TestCapability( const char * )
{
    return FALSE;
}

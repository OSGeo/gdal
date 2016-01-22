/******************************************************************************
 * $Id$
 *
 * Project:  UK NTF Reader
 * Purpose:  Implements OGRNTFFeatureClassLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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

#include "ntf.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                      OGRNTFFeatureClassLayer()                       */
/*                                                                      */
/*      Note that the OGRNTFLayer assumes ownership of the passed       */
/*      OGRFeatureDefn object.                                          */
/************************************************************************/

OGRNTFFeatureClassLayer::OGRNTFFeatureClassLayer( OGRNTFDataSource *poDSIn )

{
    poFilterGeom = NULL;

    poDS = poDSIn;

    iCurrentFC = 0;

/* -------------------------------------------------------------------- */
/*      Establish the schema.                                           */
/* -------------------------------------------------------------------- */
    poFeatureDefn = new OGRFeatureDefn( "FEATURE_CLASSES" );
    poFeatureDefn->SetGeomType( wkbNone );
    poFeatureDefn->Reference();

    OGRFieldDefn      oFCNum( "FEAT_CODE", OFTString );

    oFCNum.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFCNum );
    
    OGRFieldDefn      oFCName( "FC_NAME", OFTString );

    oFCNum.SetWidth( 80 );
    poFeatureDefn->AddFieldDefn( &oFCName );
}

/************************************************************************/
/*                      ~OGRNTFFeatureClassLayer()                      */
/************************************************************************/

OGRNTFFeatureClassLayer::~OGRNTFFeatureClassLayer()

{
    if( poFeatureDefn )
        poFeatureDefn->Release();

    if( poFilterGeom != NULL )
        delete poFilterGeom;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRNTFFeatureClassLayer::SetSpatialFilter( OGRGeometry * poGeomIn )

{
    if( poFilterGeom != NULL )
    {
        delete poFilterGeom;
        poFilterGeom = NULL;
    }

    if( poGeomIn != NULL )
        poFilterGeom = poGeomIn->clone();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRNTFFeatureClassLayer::ResetReading()

{
    iCurrentFC = 0;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRNTFFeatureClassLayer::GetNextFeature()

{
    if( iCurrentFC >= GetFeatureCount() )
        return NULL;

    return GetFeature( (long) iCurrentFC++ );
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRNTFFeatureClassLayer::GetFeature( long nFeatureId )

{
    char        *pszFCName, *pszFCId;

    if( nFeatureId < 0 || nFeatureId >= poDS->GetFCCount() )
        return NULL;
    
    poDS->GetFeatureClass( nFeatureId, &pszFCId, &pszFCName );
    
/* -------------------------------------------------------------------- */
/*      Create a corresponding feature.                                 */
/* -------------------------------------------------------------------- */
    OGRFeature  *poFeature = new OGRFeature( poFeatureDefn );

    poFeature->SetField( 0, pszFCId );
    poFeature->SetField( 1, pszFCName );
    poFeature->SetFID( nFeatureId );
    
    return poFeature;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/*                                                                      */
/*      If a spatial filter is in effect, we turn control over to       */
/*      the generic counter.  Otherwise we return the total count.      */
/*      Eventually we should consider implementing a more efficient     */
/*      way of counting features matching a spatial query.              */
/************************************************************************/

int OGRNTFFeatureClassLayer::GetFeatureCount( CPL_UNUSED int bForce )
{
    return poDS->GetFCCount();
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRNTFFeatureClassLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    else if( EQUAL(pszCap,OLCSequentialWrite) 
             || EQUAL(pszCap,OLCRandomWrite) )
        return FALSE;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return TRUE;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return TRUE;

    else 
        return FALSE;
}


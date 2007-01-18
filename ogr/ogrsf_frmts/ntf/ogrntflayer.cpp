/******************************************************************************
 * $Id$
 *
 * Project:  UK NTF Reader
 * Purpose:  Implements OGRNTFLayer class.
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
/*                            OGRNTFLayer()                             */
/*                                                                      */
/*      Note that the OGRNTFLayer assumes ownership of the passed       */
/*      OGRFeatureDefn object.                                          */
/************************************************************************/

OGRNTFLayer::OGRNTFLayer( OGRNTFDataSource *poDSIn,
                          OGRFeatureDefn * poFeatureDefine,
                          NTFFeatureTranslator pfnTranslatorIn )

{
    poDS = poDSIn;
    poFeatureDefn = poFeatureDefine;
    pfnTranslator = pfnTranslatorIn;

    iCurrentReader = -1;
    nCurrentPos = -1;
    nCurrentFID = 1;
}

/************************************************************************/
/*                           ~OGRNTFLayer()                           */
/************************************************************************/

OGRNTFLayer::~OGRNTFLayer()

{
    if( m_nFeaturesRead > 0 && poFeatureDefn != NULL )
    {
        CPLDebug( "Mem", "%d features read on layer '%s'.",
                  (int) m_nFeaturesRead, 
                  poFeatureDefn->GetName() );
    }

    if( poFeatureDefn )
        poFeatureDefn->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRNTFLayer::ResetReading()

{
    iCurrentReader = -1;
    nCurrentPos = -1;
    nCurrentFID = 1;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRNTFLayer::GetNextFeature()

{
    OGRFeature  *poFeature = NULL;

/* -------------------------------------------------------------------- */
/*      Have we processed all features already?                         */
/* -------------------------------------------------------------------- */
    if( iCurrentReader == poDS->GetFileCount() )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Do we need to open a file?                                      */
/* -------------------------------------------------------------------- */
    if( iCurrentReader == -1 )
    {
        iCurrentReader++;
        nCurrentPos = -1;
    }

    NTFFileReader       *poCurrentReader = poDS->GetFileReader(iCurrentReader);
    if( poCurrentReader->GetFP() == NULL )
    {
        poCurrentReader->Open();
    }

/* -------------------------------------------------------------------- */
/*      Ensure we are reading on from the same point we were reading    */
/*      from for the last feature, even if some other access            */
/*      mechanism has moved the file pointer.                           */
/* -------------------------------------------------------------------- */
    if( nCurrentPos != -1 )
        poCurrentReader->SetFPPos( nCurrentPos, nCurrentFID );
    else
        poCurrentReader->Reset();
        
/* -------------------------------------------------------------------- */
/*      Read features till we find one that satisfies our current       */
/*      spatial criteria.                                               */
/* -------------------------------------------------------------------- */
    while( TRUE )
    {
        poFeature = poCurrentReader->ReadOGRFeature( this );
        if( poFeature == NULL )
            break;

        m_nFeaturesRead++;

        if( (m_poFilterGeom == NULL
             || poFeature->GetGeometryRef() == NULL 
             || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature )) )
            break;

        delete poFeature;
    }

/* -------------------------------------------------------------------- */
/*      If we get NULL the file must be all consumed, advance to the    */
/*      next file that contains features for this layer.                */
/* -------------------------------------------------------------------- */
    if( poFeature == NULL )
    {
        poCurrentReader->Close();

        if( poDS->GetOption("CACHING") != NULL
            && EQUAL(poDS->GetOption("CACHING"),"OFF") )
        {
            poCurrentReader->DestroyIndex();
        }

        do { 
            iCurrentReader++;
        } while( iCurrentReader < poDS->GetFileCount()
                 && !poDS->GetFileReader(iCurrentReader)->TestForLayer(this) );

        nCurrentPos = -1;
        nCurrentFID = 1;

        poFeature = GetNextFeature();
    }
    else
    {
        poCurrentReader->GetFPPos(&nCurrentPos, &nCurrentFID);
    }

    return poFeature;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRNTFLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return FALSE;

    else if( EQUAL(pszCap,OLCSequentialWrite) 
             || EQUAL(pszCap,OLCRandomWrite) )
        return FALSE;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return FALSE;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return FALSE;

    else 
        return FALSE;
}

/************************************************************************/
/*                          FeatureTranslate()                          */
/************************************************************************/

OGRFeature * OGRNTFLayer::FeatureTranslate( NTFFileReader *poReader,
                                            NTFRecord ** papoGroup )

{
    if( pfnTranslator == NULL )
        return NULL;

    return pfnTranslator( poReader, this, papoGroup );
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRNTFLayer::GetSpatialRef()

{
    return poDS->GetSpatialRef();
}

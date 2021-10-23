/******************************************************************************
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

CPL_CVSID("$Id$")

/************************************************************************/
/*                            OGRNTFLayer()                             */
/*                                                                      */
/*      Note that the OGRNTFLayer assumes ownership of the passed       */
/*      OGRFeatureDefn object.                                          */
/************************************************************************/

OGRNTFLayer::OGRNTFLayer( OGRNTFDataSource *poDSIn,
                          OGRFeatureDefn * poFeatureDefine,
                          NTFFeatureTranslator pfnTranslatorIn ) :
    poFeatureDefn(poFeatureDefine),
    pfnTranslator(pfnTranslatorIn),
    poDS(poDSIn),
    iCurrentReader(-1),
    nCurrentPos((vsi_l_offset)-1),
    nCurrentFID(1)
{
    SetDescription( poFeatureDefn->GetName() );
}

/************************************************************************/
/*                           ~OGRNTFLayer()                           */
/************************************************************************/

OGRNTFLayer::~OGRNTFLayer()

{
    if( m_nFeaturesRead > 0 && poFeatureDefn != nullptr )
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
    nCurrentPos = (vsi_l_offset)-1;
    nCurrentFID = 1;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRNTFLayer::GetNextFeature()

{
    OGRFeature  *poFeature = nullptr;

/* -------------------------------------------------------------------- */
/*      Have we processed all features already?                         */
/* -------------------------------------------------------------------- */
    if( iCurrentReader == poDS->GetFileCount() )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Do we need to open a file?                                      */
/* -------------------------------------------------------------------- */
    if( iCurrentReader == -1 )
    {
        iCurrentReader++;
        nCurrentPos = (vsi_l_offset)-1;
    }

    NTFFileReader       *poCurrentReader = poDS->GetFileReader(iCurrentReader);
    if( poCurrentReader->GetFP() == nullptr )
    {
        poCurrentReader->Open();
    }

/* -------------------------------------------------------------------- */
/*      Ensure we are reading on from the same point we were reading    */
/*      from for the last feature, even if some other access            */
/*      mechanism has moved the file pointer.                           */
/* -------------------------------------------------------------------- */
    if( nCurrentPos != (vsi_l_offset)-1 )
        poCurrentReader->SetFPPos( nCurrentPos, nCurrentFID );
    else
        poCurrentReader->Reset();

/* -------------------------------------------------------------------- */
/*      Read features till we find one that satisfies our current       */
/*      spatial criteria.                                               */
/* -------------------------------------------------------------------- */
    while( true )
    {
        poFeature = poCurrentReader->ReadOGRFeature( this );
        if( poFeature == nullptr )
            break;

        m_nFeaturesRead++;

        if( (m_poFilterGeom == nullptr
             || poFeature->GetGeometryRef() == nullptr
             || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == nullptr
                || m_poAttrQuery->Evaluate( poFeature )) )
            break;

        delete poFeature;
    }

/* -------------------------------------------------------------------- */
/*      If we get NULL the file must be all consumed, advance to the    */
/*      next file that contains features for this layer.                */
/* -------------------------------------------------------------------- */
    if( poFeature == nullptr )
    {
        poCurrentReader->Close();

        if( poDS->GetOption("CACHING") != nullptr
            && EQUAL(poDS->GetOption("CACHING"),"OFF") )
        {
            poCurrentReader->DestroyIndex();
        }

        do {
            iCurrentReader++;
        } while( iCurrentReader < poDS->GetFileCount()
                 && !poDS->GetFileReader(iCurrentReader)->TestForLayer(this) );

        nCurrentPos = (vsi_l_offset)-1;
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

int OGRNTFLayer::TestCapability( const char * /* pszCap */ )

{
    return FALSE;
}

/************************************************************************/
/*                          FeatureTranslate()                          */
/************************************************************************/

OGRFeature * OGRNTFLayer::FeatureTranslate( NTFFileReader *poReader,
                                            NTFRecord ** papoGroup )

{
    if( pfnTranslator == nullptr )
        return nullptr;

    return pfnTranslator( poReader, this, papoGroup );
}

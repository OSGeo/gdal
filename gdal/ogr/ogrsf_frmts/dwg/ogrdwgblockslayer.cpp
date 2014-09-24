/******************************************************************************
 * $Id: ogrdwglayer.cpp 19643 2010-05-08 21:56:18Z rouault $
 *
 * Project:  DWG Translator
 * Purpose:  Implements OGRDWGBlocksLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_dwg.h"
#include "cpl_conv.h"

CPL_CVSID("$Id: ogrdwglayer.cpp 19643 2010-05-08 21:56:18Z rouault $");

/************************************************************************/
/*                         OGRDWGBlocksLayer()                          */
/************************************************************************/

OGRDWGBlocksLayer::OGRDWGBlocksLayer( OGRDWGDataSource *poDS )

{
    this->poDS = poDS;

    ResetReading();

    poFeatureDefn = new OGRFeatureDefn( "blocks" );
    poFeatureDefn->Reference();

    poDS->AddStandardFields( poFeatureDefn );
}

/************************************************************************/
/*                         ~OGRDWGBlocksLayer()                         */
/************************************************************************/

OGRDWGBlocksLayer::~OGRDWGBlocksLayer()

{
    if( m_nFeaturesRead > 0 && poFeatureDefn != NULL )
    {
        CPLDebug( "DWG", "%d features read on layer '%s'.",
                  (int) m_nFeaturesRead, 
                  poFeatureDefn->GetName() );
    }

    if( poFeatureDefn )
        poFeatureDefn->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRDWGBlocksLayer::ResetReading()

{
    iNextFID = 0;
    iNextSubFeature = 0;
    oIt = poDS->GetBlockMap().begin();
}

/************************************************************************/
/*                      GetNextUnfilteredFeature()                      */
/************************************************************************/

OGRFeature *OGRDWGBlocksLayer::GetNextUnfilteredFeature()

{
    OGRFeature *poFeature = NULL;

/* -------------------------------------------------------------------- */
/*      Are we out of features?                                         */
/* -------------------------------------------------------------------- */
    if( oIt == poDS->GetBlockMap().end() )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Are we done reading the current blocks features?                */
/* -------------------------------------------------------------------- */
    DWGBlockDefinition *psBlock = &(oIt->second);
    unsigned int nSubFeatureCount = psBlock->apoFeatures.size();

    if( psBlock->poGeometry != NULL )
        nSubFeatureCount++;

    if( iNextSubFeature >= nSubFeatureCount )
    {
        oIt++;

        iNextSubFeature = 0;

        if( oIt == poDS->GetBlockMap().end() )
            return NULL;

        psBlock = &(oIt->second);
    }
        
/* -------------------------------------------------------------------- */
/*      Is this a geometry based block?                                 */
/* -------------------------------------------------------------------- */
    if( psBlock->poGeometry != NULL
        && iNextSubFeature == psBlock->apoFeatures.size() )
    {
        poFeature = new OGRFeature( poFeatureDefn );
        poFeature->SetGeometry( psBlock->poGeometry );
        iNextSubFeature++;
    }

/* -------------------------------------------------------------------- */
/*      Otherwise duplicate the next sub-feature.                       */
/* -------------------------------------------------------------------- */
    else
    {
        poFeature = new OGRFeature( poFeatureDefn );
        poFeature->SetFrom( psBlock->apoFeatures[iNextSubFeature] );
        iNextSubFeature++;
    }

/* -------------------------------------------------------------------- */
/*      Set FID and block name.                                         */
/* -------------------------------------------------------------------- */
    poFeature->SetFID( iNextFID++ );

    poFeature->SetField( "BlockName", oIt->first.c_str() );

    m_nFeaturesRead++;

    return poFeature;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRDWGBlocksLayer::GetNextFeature()

{
    while( TRUE )
    {
        OGRFeature *poFeature = GetNextUnfilteredFeature();

        if( poFeature == NULL )
            return NULL;

        if( (m_poFilterGeom == NULL
             || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature ) ) )
        {
            return poFeature;
        }

        delete poFeature;
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDWGBlocksLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCStringsAsUTF8) )
        return TRUE;
    else
        return FALSE;
}


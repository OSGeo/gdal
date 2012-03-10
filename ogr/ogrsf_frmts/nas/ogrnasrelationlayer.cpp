/******************************************************************************
 * $Id$
 *
 * Project:  OGR
 * Purpose:  Implements OGRNASRelationLayer class, a special layer holding all
 *           the relations from the NAS file.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_nas.h"
#include "cpl_conv.h"
#include "cpl_port.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                        OGRNASRelationLayer()                         */
/************************************************************************/

OGRNASRelationLayer::OGRNASRelationLayer( OGRNASDataSource *poDSIn )

{
    poDS = poDSIn;

    iNextFeature = 0;
    bPopulated = FALSE;

/* -------------------------------------------------------------------- */
/*      Establish the layer fields.                                     */
/* -------------------------------------------------------------------- */
    poFeatureDefn = new OGRFeatureDefn( "ALKIS_beziehungen" );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbNone );

    OGRFieldDefn  oFD( "", OFTString );

    oFD.SetName( "beziehung_von" );
    poFeatureDefn->AddFieldDefn( &oFD );

    oFD.SetName( "beziehungsart" );
    poFeatureDefn->AddFieldDefn( &oFD );

    oFD.SetName( "beziehung_zu" );
    poFeatureDefn->AddFieldDefn( &oFD );
}

/************************************************************************/
/*                        ~OGRNASRelationLayer()                        */
/************************************************************************/

OGRNASRelationLayer::~OGRNASRelationLayer()

{
    if( poFeatureDefn )
        poFeatureDefn->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRNASRelationLayer::ResetReading()

{
    iNextFeature = 0;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRNASRelationLayer::GetNextFeature()

{
    if( !bPopulated )
        poDS->PopulateRelations();

/* ==================================================================== */
/*      Loop till we find and translate a feature meeting all our       */
/*      requirements.                                                   */
/* ==================================================================== */
    while( TRUE )
    {
        // out of features?
        if( iNextFeature >= (int) aoRelationCollection.size() )
            return NULL;

/* -------------------------------------------------------------------- */
/*      The from/type/to values are stored in a packed string with      */
/*      \0 separators for compactness.  Split out components.           */
/* -------------------------------------------------------------------- */
        const char *pszFromID, *pszType, *pszToID;

        pszFromID = aoRelationCollection[iNextFeature].c_str();
        pszType = pszFromID + strlen(pszFromID) + 1;
        pszToID = pszType + strlen(pszType) + 1;

        m_nFeaturesRead++;

/* -------------------------------------------------------------------- */
/*      Translate values into an OGRFeature.                            */
/* -------------------------------------------------------------------- */
        OGRFeature *poFeature = new OGRFeature( poFeatureDefn );

        poFeature->SetField( 0, pszFromID );
        poFeature->SetField( 1, pszType );
        poFeature->SetField( 2, pszToID );

        poFeature->SetFID( iNextFeature++ );

/* -------------------------------------------------------------------- */
/*      Test against the attribute query.                               */
/* -------------------------------------------------------------------- */
        if( m_poAttrQuery != NULL
            && !m_poAttrQuery->Evaluate( poFeature ) )
            delete poFeature;
        else
            return poFeature;
    }

    return NULL;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRNASRelationLayer::GetFeatureCount( int bForce )

{
    if( !bPopulated )
        poDS->PopulateRelations();

    if( m_poAttrQuery == NULL )
        return aoRelationCollection.size();
    else
        return OGRLayer::GetFeatureCount( bForce );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRNASRelationLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCFastGetExtent) )
        return TRUE;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return bPopulated && m_poAttrQuery == NULL;

    else if( EQUAL(pszCap,OLCStringsAsUTF8) )
        return TRUE;

    else 
        return FALSE;
}

/************************************************************************/
/*                            AddRelation()                             */
/************************************************************************/

void OGRNASRelationLayer::AddRelation( const char *pszFromID,
                                       const char *pszType,
                                       const char *pszToID )

{
    int nMergedLen = strlen(pszFromID) + strlen(pszType) + strlen(pszToID) + 3;
    char *pszMerged = (char *) CPLMalloc(nMergedLen);
    
    strcpy( pszMerged, pszFromID );
    strcpy( pszMerged + strlen(pszFromID) + 1, pszType );
    strcpy( pszMerged + strlen(pszFromID) + strlen(pszType) + 2, pszToID );

    CPLString osRelation;
    osRelation.assign( pszMerged, nMergedLen );

    CPLFree( pszMerged );

    aoRelationCollection.push_back( osRelation );
}


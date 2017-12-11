/******************************************************************************
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

#include "cpl_conv.h"
#include "cpl_port.h"
#include "cpl_string.h"
#include "ogr_nas.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                        OGRNASRelationLayer()                         */
/************************************************************************/

OGRNASRelationLayer::OGRNASRelationLayer( OGRNASDataSource *poDSIn ) :
    poFeatureDefn(new OGRFeatureDefn( "ALKIS_beziehungen" )),
    poDS(poDSIn),
    bPopulated(false),
    iNextFeature(0)
{
/* -------------------------------------------------------------------- */
/*      Establish the layer fields.                                     */
/* -------------------------------------------------------------------- */
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbNone );

    OGRFieldDefn oFD( "", OFTString );

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
    while( true )
    {
        // out of features?
        if( iNextFeature >= static_cast<int>(aoRelationCollection.size()) )
            return nullptr;

/* -------------------------------------------------------------------- */
/*      The from/type/to values are stored in a packed string with      */
/*      \0 separators for compactness.  Split out components.           */
/* -------------------------------------------------------------------- */
        const char *pszFromID = aoRelationCollection[iNextFeature].c_str();
        const char *pszType = pszFromID + strlen(pszFromID) + 1;
        const char *pszToID = pszType + strlen(pszType) + 1;

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
        if( m_poAttrQuery != nullptr
            && !m_poAttrQuery->Evaluate( poFeature ) )
            delete poFeature;
        else
            return poFeature;
    }

    return nullptr;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRNASRelationLayer::GetFeatureCount( int bForce )

{
    if( !bPopulated )
        poDS->PopulateRelations();

    if( m_poAttrQuery == nullptr )
        return aoRelationCollection.size();

    return OGRLayer::GetFeatureCount( bForce );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRNASRelationLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCFastGetExtent) )
        return TRUE;

    if( EQUAL(pszCap,OLCFastFeatureCount) )
        return bPopulated && m_poAttrQuery == nullptr;

    if( EQUAL(pszCap,OLCStringsAsUTF8) )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                            AddRelation()                             */
/************************************************************************/

void OGRNASRelationLayer::AddRelation( const char *pszFromID,
                                       const char *pszType,
                                       const char *pszToID )

{
    const size_t nMergedLen =
        strlen(pszFromID) + strlen(pszType) + strlen(pszToID) + 3;
    char *pszMerged = (char *) CPLMalloc(nMergedLen);

    strcpy( pszMerged, pszFromID );
    strcpy( pszMerged + strlen(pszFromID) + 1, pszType );
    strcpy( pszMerged + strlen(pszFromID) + strlen(pszType) + 2, pszToID );

    CPLString osRelation;
    osRelation.assign( pszMerged, nMergedLen );

    CPLFree( pszMerged );

    aoRelationCollection.push_back( osRelation );
}

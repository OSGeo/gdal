/******************************************************************************
 *
 * Project:  OGR
 * Purpose:  Implements OGRAVCBinLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_avc.h"
#include "ogr_api.h"
#include "cpl_conv.h"
#include "cpl_string.h"

#include <cstdlib>

CPL_CVSID("$Id$")

/************************************************************************/
/*                           OGRAVCBinLayer()                           */
/************************************************************************/

OGRAVCBinLayer::OGRAVCBinLayer( OGRAVCBinDataSource *poDSIn,
                                AVCE00Section *psSectionIn ) :
    OGRAVCLayer( psSectionIn->eType, poDSIn ),
    m_psSection(psSectionIn),
    hFile(NULL),
    poArcLayer(NULL),
    bNeedReset(false),
    hTable(NULL),
    nTableBaseField(-1),
    nTableAttrIndex(-1),
    nNextFID(1)
{
    SetupFeatureDefinition( m_psSection->pszName );

    szTableName[0] = '\0';
    if( m_psSection->eType == AVCFilePAL )
        snprintf( szTableName, sizeof(szTableName), "%s.PAT",
                  poDS->GetCoverageName() );
    else if( m_psSection->eType == AVCFileRPL )
        snprintf( szTableName, sizeof(szTableName), "%s.PAT%s",
                  poDS->GetCoverageName(), m_psSection->pszName );
    else if( m_psSection->eType == AVCFileARC )
        snprintf( szTableName, sizeof(szTableName), "%s.AAT",
                  poDS->GetCoverageName() );
    else if( m_psSection->eType == AVCFileLAB )
    {
        AVCE00ReadPtr psInfo
            = static_cast<OGRAVCBinDataSource *>( poDS) ->GetInfo();

        snprintf( szTableName, sizeof(szTableName), "%s.PAT",
                  poDS->GetCoverageName() );

        for( int iSection = 0; iSection < psInfo->numSections; iSection++ )
        {
            if( psInfo->pasSections[iSection].eType == AVCFilePAL )
                nTableAttrIndex = poFeatureDefn->GetFieldIndex( "PolyId" );
        }
    }

    CheckSetupTable();
}

/************************************************************************/
/*                          ~OGRAVCBinLayer()                           */
/************************************************************************/

OGRAVCBinLayer::~OGRAVCBinLayer()

{
    ResetReading();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRAVCBinLayer::ResetReading()

{
    if( hFile != NULL )
    {
        AVCBinReadClose( hFile );
        hFile = NULL;
    }

    bNeedReset = false;
    nNextFID = 1;

    if( hTable != NULL )
    {
        AVCBinReadClose( hTable );
        hTable = NULL;
    }
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRAVCBinLayer::GetFeature( GIntBig nFID )

{
    if( !CPL_INT64_FITS_ON_INT32(nFID) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      If we haven't started yet, open the file now.                   */
/* -------------------------------------------------------------------- */
    if( hFile == NULL )
    {
        AVCE00ReadPtr psInfo
            = static_cast<OGRAVCBinDataSource *>( poDS )->GetInfo();

        hFile = AVCBinReadOpen(psInfo->pszCoverPath,
                               m_psSection->pszFilename,
                               psInfo->eCoverType,
                               m_psSection->eType,
                               psInfo->psDBCSInfo);
    }

/* -------------------------------------------------------------------- */
/*      Read the raw feature - the -3 fid is a special flag             */
/*      indicating serial access.                                       */
/* -------------------------------------------------------------------- */
    void *pFeature = NULL;

    if( nFID == -3 )
    {
        while( (pFeature = AVCBinReadNextObject( hFile )) != NULL
               && !MatchesSpatialFilter( pFeature ) )
        {
            nNextFID++;
        }
    }
    else
    {
        bNeedReset = true;
        pFeature = AVCBinReadObject( hFile, (int)nFID );
    }

    if( pFeature == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Translate the feature.                                          */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature = TranslateFeature( pFeature );
    if( poFeature == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      LAB's we have to assign the FID to directly, since it           */
/*      doesn't seem to be stored in the file structure.                */
/* -------------------------------------------------------------------- */
    if( m_psSection->eType == AVCFileLAB )
    {
        if( nFID == -3 )
            poFeature->SetFID( nNextFID++ );
        else
            poFeature->SetFID( nFID );
    }

/* -------------------------------------------------------------------- */
/*      If this is a polygon layer, try to assemble the arcs to form    */
/*      the whole polygon geometry.                                     */
/* -------------------------------------------------------------------- */
    if( m_psSection->eType == AVCFilePAL
        || m_psSection->eType == AVCFileRPL )
        FormPolygonGeometry( poFeature, (AVCPal *) pFeature );

/* -------------------------------------------------------------------- */
/*      If we have an attribute table, append the attributes now.       */
/* -------------------------------------------------------------------- */
    AppendTableFields( poFeature );

    return poFeature;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRAVCBinLayer::GetNextFeature()

{
    if( bNeedReset )
        ResetReading();

    OGRFeature *poFeature = GetFeature( -3 );

    // Skip universe polygon.
    if( poFeature != NULL && poFeature->GetFID() == 1
        && m_psSection->eType == AVCFilePAL )
    {
        OGRFeature::DestroyFeature( poFeature );
        poFeature = GetFeature( -3 );
    }

    while( poFeature != NULL
           && ((m_poAttrQuery != NULL
                && !m_poAttrQuery->Evaluate( poFeature ) )
               || !FilterGeometry( poFeature->GetGeometryRef() ) ) )
    {
        OGRFeature::DestroyFeature( poFeature );
        poFeature = GetFeature( -3 );
    }

    if( poFeature == NULL )
        ResetReading();

    return poFeature;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRAVCBinLayer::TestCapability( const char * pszCap )

{
    if( eSectionType == AVCFileARC && EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    return OGRAVCLayer::TestCapability( pszCap );
}

/************************************************************************/
/*                        FormPolygonGeometry()                         */
/*                                                                      */
/*      Collect all the arcs forming edges to this polygon and form     */
/*      them into the appropriate OGR geometry on the target feature.   */
/************************************************************************/

bool OGRAVCBinLayer::FormPolygonGeometry( OGRFeature *poFeature,
                                          AVCPal *psPAL )

{
/* -------------------------------------------------------------------- */
/*      Try to find the corresponding ARC layer if not already          */
/*      recorded.                                                       */
/* -------------------------------------------------------------------- */
    if( poArcLayer == NULL )
    {
        for( int i = 0; i < poDS->GetLayerCount(); i++ )
        {
            OGRAVCBinLayer *poLayer
                = static_cast<OGRAVCBinLayer *>( poDS->GetLayer(i) );

            if( poLayer->eSectionType == AVCFileARC )
                poArcLayer = poLayer;
        }

        if( poArcLayer == NULL )
            return false;
    }

/* -------------------------------------------------------------------- */
/*      Read all the arcs related to this polygon, making a working     */
/*      copy of them since the one returned by AVC is temporary.        */
/* -------------------------------------------------------------------- */
    OGRGeometryCollection oArcs;

    for( int iArc = 0; iArc < psPAL->numArcs; iArc++ )
    {
        if( psPAL->pasArcs[iArc].nArcId == 0 )
            continue;

        // If the other side of the line is the same polygon then this
        // arc is a "bridge" arc and can be discarded.  If we don't discard
        // it, then we should double it as bridge arcs seem to only appear
        // once.  But by discarding it we ensure a multi-ring polygon will be
        // properly formed.
        if( psPAL->pasArcs[iArc].nAdjPoly == psPAL->nPolyId )
            continue;

        OGRFeature *poArc
            = poArcLayer->GetFeature( std::abs(psPAL->pasArcs[iArc].nArcId) );

        if( poArc == NULL )
            return false;

        if( poArc->GetGeometryRef() == NULL )
            return false;

        oArcs.addGeometry( poArc->GetGeometryRef() );
        OGRFeature::DestroyFeature( poArc );
    }

    OGRErr eErr;
    OGRPolygon *poPolygon
      = reinterpret_cast<OGRPolygon *>(
        OGRBuildPolygonFromEdges( (OGRGeometryH) &oArcs, TRUE, FALSE,
                                  0.0, &eErr ) );
    if( poPolygon != NULL )
        poFeature->SetGeometryDirectly( poPolygon );

    return eErr == OGRERR_NONE;
}

/************************************************************************/
/*                          CheckSetupTable()                           */
/*                                                                      */
/*      Check if the named table exists, and if so, setup access to     */
/*      it (open it), and add it's fields to the feature class          */
/*      definition.                                                     */
/************************************************************************/

bool OGRAVCBinLayer::CheckSetupTable()

{
    if( szTableName[0] == '\0' )
        return false;

/* -------------------------------------------------------------------- */
/*      Scan for the indicated section.                                 */
/* -------------------------------------------------------------------- */
    AVCE00ReadPtr psInfo
        = static_cast<OGRAVCBinDataSource *>( poDS )->GetInfo();
    const size_t BUFSIZE = 32;
    char szPaddedName[BUFSIZE+1] = { 0 };

    // Fill szPaddedName with szTableName up to 32 chars and fill the remaining
    // ones with ' '
    strncpy( szPaddedName, szTableName, BUFSIZE );
    if( strlen(szTableName) < BUFSIZE )
    {
        memset( szPaddedName + strlen(szTableName), ' ',
                BUFSIZE - strlen(szTableName) );
    }

    AVCE00Section *l_psSection = NULL;
    for( int iSection = 0; iSection < psInfo->numSections; iSection++ )
    {
        if( EQUAL(szPaddedName,psInfo->pasSections[iSection].pszName)
            && psInfo->pasSections[iSection].eType == AVCFileTABLE )
            l_psSection = psInfo->pasSections + iSection;
    }

    if( l_psSection == NULL )
    {
        szTableName[0] = '\0';
        return false;
    }

/* -------------------------------------------------------------------- */
/*      Try opening the table.                                          */
/* -------------------------------------------------------------------- */
    hTable = AVCBinReadOpen( psInfo->pszInfoPath,  szTableName,
                             psInfo->eCoverType, AVCFileTABLE,
                             psInfo->psDBCSInfo);

    if( hTable == NULL )
    {
        szTableName[0] = '\0';
        return false;
    }

/* -------------------------------------------------------------------- */
/*      Setup attributes.                                               */
/* -------------------------------------------------------------------- */
    nTableBaseField = poFeatureDefn->GetFieldCount();

    AppendTableDefinition( hTable->hdr.psTableDef );

/* -------------------------------------------------------------------- */
/*      Close table so we don't have to many files open at once.        */
/* -------------------------------------------------------------------- */
    AVCBinReadClose( hTable );

    hTable = NULL;

    return true;
}

/************************************************************************/
/*                         AppendTableFields()                          */
/************************************************************************/

bool OGRAVCBinLayer::AppendTableFields( OGRFeature *poFeature )

{
    AVCE00ReadPtr psInfo
        = static_cast<OGRAVCBinDataSource *>( poDS)->GetInfo();

    if( szTableName[0] == '\0' )
        return false;

/* -------------------------------------------------------------------- */
/*      Open the table if it is currently closed.                       */
/* -------------------------------------------------------------------- */
    if( hTable == NULL )
    {
        hTable = AVCBinReadOpen( psInfo->pszInfoPath,  szTableName,
                                 psInfo->eCoverType, AVCFileTABLE,
                                 psInfo->psDBCSInfo);
    }

    if( hTable == NULL )
        return false;

/* -------------------------------------------------------------------- */
/*      Read the info record.                                           */
/*                                                                      */
/*      We usually assume the FID of the feature is the key but in a    */
/*      polygon coverage we need to use the PolyId attribute of LAB     */
/*      features to lookup the related attributes.  In this case        */
/*      nTableAttrIndex will already be setup to refer to the           */
/*      PolyId field.                                                   */
/* -------------------------------------------------------------------- */
    const int nRecordId = nTableAttrIndex == -1
        ? static_cast<int>( poFeature->GetFID() )
        : poFeature->GetFieldAsInteger( nTableAttrIndex );

    void *hRecord = AVCBinReadObject( hTable, nRecordId );
    if( hRecord == NULL )
        return false;

/* -------------------------------------------------------------------- */
/*      Translate it.                                                   */
/* -------------------------------------------------------------------- */
    return TranslateTableFields( poFeature, nTableBaseField,
                                 hTable->hdr.psTableDef,
                                 (AVCField *) hRecord );
}

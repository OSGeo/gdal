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

constexpr int SERIAL_ACCESS_FID = INT_MIN;

/************************************************************************/
/*                           OGRAVCBinLayer()                           */
/************************************************************************/

OGRAVCBinLayer::OGRAVCBinLayer( OGRAVCBinDataSource *poDSIn,
                                AVCE00Section *psSectionIn ) :
    OGRAVCLayer( psSectionIn->eType, poDSIn ),
    m_psSection(psSectionIn),
    hFile(nullptr),
    poArcLayer(nullptr),
    bNeedReset(false),
    hTable(nullptr),
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
    OGRAVCBinLayer::ResetReading();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRAVCBinLayer::ResetReading()

{
    if( hFile != nullptr )
    {
        AVCBinReadClose( hFile );
        hFile = nullptr;
    }

    bNeedReset = false;
    nNextFID = 1;
    m_bEOF = false;

    if( hTable != nullptr )
    {
        AVCBinReadClose( hTable );
        hTable = nullptr;
    }
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRAVCBinLayer::GetFeature( GIntBig nFID )

{
    if( !CPL_INT64_FITS_ON_INT32(nFID) )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      If we haven't started yet, open the file now.                   */
/* -------------------------------------------------------------------- */
    if( hFile == nullptr )
    {
        AVCE00ReadPtr psInfo
            = static_cast<OGRAVCBinDataSource *>( poDS )->GetInfo();

        hFile = AVCBinReadOpen(psInfo->pszCoverPath,
                               m_psSection->pszFilename,
                               psInfo->eCoverType,
                               m_psSection->eType,
                               psInfo->psDBCSInfo);
        if( hFile == nullptr )
            return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Read the raw feature - the SERIAL_ACCESS_FID fid is a special flag */
/*      indicating serial access.                                       */
/* -------------------------------------------------------------------- */
    void *pFeature = nullptr;

    if( nFID == SERIAL_ACCESS_FID )
    {
        while( (pFeature = AVCBinReadNextObject( hFile )) != nullptr
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

    if( pFeature == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Translate the feature.                                          */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature = TranslateFeature( pFeature );
    if( poFeature == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      LAB's we have to assign the FID to directly, since it           */
/*      doesn't seem to be stored in the file structure.                */
/* -------------------------------------------------------------------- */
    if( m_psSection->eType == AVCFileLAB )
    {
        if( nFID == SERIAL_ACCESS_FID )
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
    if ( m_bEOF )
        return nullptr;

    if( bNeedReset )
        ResetReading();

    OGRFeature *poFeature = GetFeature( SERIAL_ACCESS_FID );

    // Skip universe polygon.
    if( poFeature != nullptr && poFeature->GetFID() == 1
        && m_psSection->eType == AVCFilePAL )
    {
        OGRFeature::DestroyFeature( poFeature );
        poFeature = GetFeature( SERIAL_ACCESS_FID );
    }

    while( poFeature != nullptr
           && ((m_poAttrQuery != nullptr
                && !m_poAttrQuery->Evaluate( poFeature ) )
               || !FilterGeometry( poFeature->GetGeometryRef() ) ) )
    {
        OGRFeature::DestroyFeature( poFeature );
        poFeature = GetFeature( SERIAL_ACCESS_FID );
    }

    if( poFeature == nullptr )
        m_bEOF = true;

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
    if( poArcLayer == nullptr )
    {
        for( int i = 0; i < poDS->GetLayerCount(); i++ )
        {
            OGRAVCBinLayer *poLayer
                = static_cast<OGRAVCBinLayer *>( poDS->GetLayer(i) );

            if( poLayer->eSectionType == AVCFileARC )
                poArcLayer = poLayer;
        }

        if( poArcLayer == nullptr )
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

        if( poArc == nullptr )
            return false;

        if( poArc->GetGeometryRef() == nullptr )
            return false;

        oArcs.addGeometry( poArc->GetGeometryRef() );
        OGRFeature::DestroyFeature( poArc );
    }

    OGRErr eErr;
    OGRGeometry *poPolygon
      = reinterpret_cast<OGRGeometry *>(
        OGRBuildPolygonFromEdges( (OGRGeometryH) &oArcs, TRUE, FALSE,
                                  0.0, &eErr ) );
    if( poPolygon != nullptr )
    {
        poPolygon->assignSpatialReference( GetSpatialRef() );
        poFeature->SetGeometryDirectly( poPolygon );
    }

    return eErr == OGRERR_NONE;
}

/************************************************************************/
/*                          CheckSetupTable()                           */
/*                                                                      */
/*      Check if the named table exists, and if so, setup access to     */
/*      it (open it), and add its fields to the feature class           */
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

    AVCE00Section *l_psSection = nullptr;
    for( int iSection = 0; iSection < psInfo->numSections; iSection++ )
    {
        if( EQUAL(szTableName,CPLString(psInfo->pasSections[iSection].pszName).Trim())
            && psInfo->pasSections[iSection].eType == AVCFileTABLE )
            l_psSection = psInfo->pasSections + iSection;
    }

    if( l_psSection == nullptr )
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

    if( hTable == nullptr )
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

    hTable = nullptr;

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
    if( hTable == nullptr )
    {
        hTable = AVCBinReadOpen( psInfo->pszInfoPath,  szTableName,
                                 psInfo->eCoverType, AVCFileTABLE,
                                 psInfo->psDBCSInfo);
    }

    if( hTable == nullptr )
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
    if( hRecord == nullptr )
        return false;

/* -------------------------------------------------------------------- */
/*      Translate it.                                                   */
/* -------------------------------------------------------------------- */
    return TranslateTableFields( poFeature, nTableBaseField,
                                 hTable->hdr.psTableDef,
                                 (AVCField *) hRecord );
}

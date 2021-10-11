/******************************************************************************
 *
 * Project:  OGR
 * Purpose:  Implements OGRAVCE00Layer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *           James Flemer <jflemer@alum.rpi.edu>
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2006, James Flemer <jflemer@alum.rpi.edu>
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
/*                           OGRAVCE00Layer()                           */
/************************************************************************/

OGRAVCE00Layer::OGRAVCE00Layer( OGRAVCDataSource *poDSIn,
                                AVCE00Section *psSectionIn ) :
    OGRAVCLayer( psSectionIn->eType, poDSIn ),
    psSection(psSectionIn),
    psRead(nullptr),
    poArcLayer(nullptr),
    nFeatureCount(-1),
    bNeedReset(false),
    nNextFID(1),
    psTableSection(nullptr),
    psTableRead(nullptr),
    pszTableFilename(nullptr),
    nTablePos(0),
    nTableBaseField(0),
    nTableAttrIndex(-1)
{
    SetupFeatureDefinition( psSection->pszName );
    /* psRead = AVCE00ReadOpenE00(psSection->pszFilename); */

#if 0
    szTableName[0] = '\0';
    if( psSection->eType == AVCFilePAL )
        sprintf( szTableName, "%s.PAT", poDS->GetCoverageName() );
    else if( psSection->eType == AVCFileRPL )
        sprintf( szTableName, "%s.PAT%s", poDS->GetCoverageName(),
                 psSectionIn->pszName );
    else if( psSection->eType == AVCFileARC )
        sprintf( szTableName, "%s.AAT", poDS->GetCoverageName() );
    else if( psSection->eType == AVCFileLAB )
    {
        AVCE00ReadPtr psInfo = ((OGRAVCE00DataSource *) poDS)->GetInfo();

        sprintf( szTableName, "%s.PAT", poDS->GetCoverageName() );

        for( int iSection = 0; iSection < psInfo->numSections; iSection++ )
        {
            if( psInfo->pasSections[iSection].eType == AVCFilePAL )
                nTableAttrIndex = poFeatureDefn->GetFieldIndex( "PolyId" );
        }
    }

#endif
}

/************************************************************************/
/*                          ~OGRAVCE00Layer()                           */
/************************************************************************/

OGRAVCE00Layer::~OGRAVCE00Layer()

{
    if (psRead)
    {
        AVCE00ReadCloseE00(psRead);
        psRead = nullptr;
    }

    if (psTableRead)
    {
        AVCE00ReadCloseE00(psTableRead);
        psTableRead = nullptr;
    }

    if (pszTableFilename)
    {
        CPLFree(pszTableFilename);
        pszTableFilename = nullptr;
    }
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRAVCE00Layer::ResetReading()

{
    if (psRead)
    {
        AVCE00ReadGotoSectionE00(psRead, psSection, 0);
    }

    if (psTableRead)
    {
        AVCE00ReadGotoSectionE00(psTableRead, psTableSection, 0);
    }

    m_bEOF = false;
    bNeedReset = false;
    nNextFID = 1;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRAVCE00Layer::GetFeature( GIntBig nFID )

{
    if( nFID < 0 && nFID != SERIAL_ACCESS_FID )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      If we haven't started yet, open the file now.                   */
/* -------------------------------------------------------------------- */
    if( psRead == nullptr )
    {
        psRead = AVCE00ReadOpenE00(psSection->pszFilename);
        if (psRead == nullptr)
            return nullptr;
        /* advance to the specified line number */
        if (AVCE00ReadGotoSectionE00(psRead, psSection, 0) != 0)
            return nullptr;
        nNextFID = 1;
    }

/* -------------------------------------------------------------------- */
/*      Read the raw feature - the SERIAL_ACCESS_FID fid is a special flag  */
/*      indicating serial access.                                       */
/* -------------------------------------------------------------------- */
    void *pFeature = nullptr;

    if( nFID == SERIAL_ACCESS_FID )
    {
        bLastWasSequential = true;

        while( (pFeature = AVCE00ReadNextObjectE00(psRead)) != nullptr
               && psRead->hParseInfo->eFileType != AVCFileUnknown
               && !MatchesSpatialFilter( pFeature ) )
        {
            nNextFID++;
        }
    }
    else
    {
        bNeedReset = true;

        if (nNextFID > nFID || bLastWasSequential)
        {
            bLastWasSequential = false;
            /* advance to the specified line number */
            if (AVCE00ReadGotoSectionE00(psRead, psSection, 0) != 0)
                return nullptr;
            nNextFID = 1;
        }

        do
        {
            pFeature = AVCE00ReadNextObjectE00(psRead);
            ++nNextFID;
        }
        while (nullptr != pFeature && nNextFID <= nFID);
    }

    if( pFeature == nullptr )
        return nullptr;
    if( eSectionType != psRead->hParseInfo->eFileType )
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
    if( psSection->eType == AVCFileLAB )
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
    if( psSection->eType == AVCFilePAL
        || psSection->eType == AVCFileRPL )
    {
        FormPolygonGeometry( poFeature, static_cast<AVCPal *>( pFeature ) );
    }

/* -------------------------------------------------------------------- */
/*      If we have an attribute table, append the attributes now.       */
/* -------------------------------------------------------------------- */
    AppendTableFields( poFeature );

    return poFeature;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRAVCE00Layer::GetNextFeature()

{
    if ( m_bEOF )
        return nullptr;

    if( bNeedReset )
        ResetReading();

    OGRFeature *poFeature = GetFeature( SERIAL_ACCESS_FID );

    // Skip universe polygon.
    if( poFeature != nullptr && poFeature->GetFID() == 1
        && psSection->eType == AVCFilePAL )
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

#if 0
int OGRAVCE00Layer::TestCapability( const char * pszCap )

{
    if( eSectionType == AVCFileARC && EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    return OGRAVCLayer::TestCapability( pszCap );
}
#endif

/************************************************************************/
/*                        FormPolygonGeometry()                         */
/*                                                                      */
/*      Collect all the arcs forming edges to this polygon and form     */
/*      them into the appropriate OGR geometry on the target feature.   */
/************************************************************************/

bool OGRAVCE00Layer::FormPolygonGeometry( OGRFeature *poFeature,
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
            OGRAVCE00Layer *poLayer
                = static_cast<OGRAVCE00Layer *>( poDS->GetLayer(i) );

            if( poLayer->eSectionType == AVCFileARC )
                poArcLayer = poLayer;
        }

        if( poArcLayer == nullptr )
            return false;
    }

/* -------------------------------------------------------------------- */
/*  Read all the arcs related to this polygon, making a working         */
/*  copy of them since the one returned by AVC is temporary.            */
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
    OGRGeometry *poPolygon = reinterpret_cast<OGRGeometry *>(
        OGRBuildPolygonFromEdges( reinterpret_cast<OGRGeometryH>( &oArcs ),
                                  TRUE, FALSE, 0.0, &eErr ) );
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

bool OGRAVCE00Layer::CheckSetupTable(AVCE00Section *psTblSectionIn)
{
    if (psTableRead)
        return false;

    const char *pszTableType = nullptr;
    switch (eSectionType)
    {
    case AVCFileARC:
        pszTableType = ".AAT";
        break;

    case AVCFilePAL:
    case AVCFileLAB:
        pszTableType = ".PAT";
        break;

    default:
        break;
    }

/* -------------------------------------------------------------------- */
/*      Is the table type found anywhere in the section pszName?  Do    */
/*      a case insensitive check.                                       */
/* -------------------------------------------------------------------- */
    if( pszTableType == nullptr )
        return false;

    int iCheckOff = 0;
    for( ;
         psTblSectionIn->pszName[iCheckOff] != '\0';
         iCheckOff++ )
    {
        if( EQUALN( psTblSectionIn->pszName + iCheckOff,
                    pszTableType, strlen(pszTableType) ) )
            break;
    }

    if( psTblSectionIn->pszName[iCheckOff] == '\0' )
        return false;

    psTableSection = psTblSectionIn;

/* -------------------------------------------------------------------- */
/*      Try opening the table.                                          */
/* -------------------------------------------------------------------- */
    psTableRead = AVCE00ReadOpenE00(psTblSectionIn->pszFilename);
    if (psTableRead == nullptr)
        return false;

    /* advance to the specified line number */
    if (AVCE00ReadGotoSectionE00(psTableRead, psTableSection, 0) != 0)
    {
        AVCE00ReadCloseE00(psTableRead);
        psTableRead = nullptr;
        return false;
    }

    AVCE00ReadNextObjectE00(psTableRead);
    bNeedReset = true;

    CPLFree(pszTableFilename);
    pszTableFilename = CPLStrdup(psTblSectionIn->pszFilename);
    nTableBaseField = poFeatureDefn->GetFieldCount();

    if (eSectionType == AVCFileLAB)
    {
        AVCE00ReadE00Ptr psInfo
            = static_cast<OGRAVCE00DataSource *>( poDS )->GetInfo();
        for( int iSection = 0; iSection < psInfo->numSections; iSection++ )
        {
            if( psInfo->pasSections[iSection].eType == AVCFilePAL )
                nTableAttrIndex = poFeatureDefn->GetFieldIndex( "PolyId" );
        }
    }

/* -------------------------------------------------------------------- */
/*      Setup attributes.                                               */
/* -------------------------------------------------------------------- */
    if( psTableRead->hParseInfo->hdr.psTableDef == nullptr )
    {
        AVCE00ReadCloseE00(psTableRead);
        psTableRead = nullptr;
        return false;
    }

    AppendTableDefinition( psTableRead->hParseInfo->hdr.psTableDef );

/* -------------------------------------------------------------------- */
/*      Close table so we don't have to many files open at once.        */
/* -------------------------------------------------------------------- */
    /* AVCE00ReadCloseE00( psTableRead ); */

    return true;
}

/************************************************************************/
/*                         AppendTableFields()                          */
/************************************************************************/

bool OGRAVCE00Layer::AppendTableFields( OGRFeature *poFeature )

{
    if (psTableRead == nullptr)
        return false;
#ifdef deadcode
/* -------------------------------------------------------------------- */
/*      Open the table if it is currently closed.                       */
/* -------------------------------------------------------------------- */
    if (psTableRead == nullptr)
    {
        psTableRead = AVCE00ReadOpenE00(pszTableFilename);
        if (psTableRead == nullptr)
            return false;

        /* Advance to the specified line number */
        if (AVCE00ReadGotoSectionE00(psTableRead, psTableSection, 0) != 0)
        {
            AVCE00ReadCloseE00(psTableRead);
            psTableRead = nullptr;
            return false;
        }
        nTablePos = 0;
    }
#endif
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

    if (nRecordId <= nTablePos)
    {
        if( AVCE00ReadGotoSectionE00(psTableRead, psTableSection, 0) != 0 )
            return false;
        nTablePos = 0;
    }

    void *hRecord = nullptr;
    do
    {
        hRecord = AVCE00ReadNextObjectE00(psTableRead);
        ++nTablePos;
    }
    while (nullptr != hRecord && nTablePos < nRecordId);

    if( hRecord == nullptr )
        return false;
    if( psTableRead->hParseInfo->hdr.psTableDef == nullptr )
        return false;

/* -------------------------------------------------------------------- */
/*      Translate it.                                                   */
/* -------------------------------------------------------------------- */
    return TranslateTableFields( poFeature, nTableBaseField,
                                 psTableRead->hParseInfo->hdr.psTableDef,
                                 static_cast<AVCField *>( hRecord ) );
}

GIntBig OGRAVCE00Layer::GetFeatureCount(int bForce)
{
    if (m_poAttrQuery != nullptr || m_poFilterGeom != nullptr)
        return OGRAVCLayer::GetFeatureCount(bForce);

    if (bForce && nFeatureCount < 0)
    {
        if (psSection->nFeatureCount < 0)
        {
            nFeatureCount = (int) OGRLayer::GetFeatureCount(bForce);
        }
        else
        {
            nFeatureCount = psSection->nFeatureCount;
            if (psSection->eType == AVCFilePAL)
                --nFeatureCount;
        }
    }
    return nFeatureCount;
}

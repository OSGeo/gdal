/******************************************************************************
 * $Id$
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

CPL_CVSID("$Id$");

/************************************************************************/
/*                           OGRAVCE00Layer()                           */
/************************************************************************/

OGRAVCE00Layer::OGRAVCE00Layer( OGRAVCDataSource *poDSIn,
                                AVCE00Section *psSectionIn )
        : OGRAVCLayer( psSectionIn->eType, poDSIn ),
          psSection(psSectionIn),
          psRead(NULL),
          poArcLayer(NULL),
          nFeatureCount(-1),
          bNeedReset(0),
          nNextFID(1),
		  psTableSection(NULL),
          psTableRead(NULL),
          pszTableFilename(NULL),
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
        psRead = NULL;
    }

    if (psTableRead)
    {
        AVCE00ReadCloseE00(psTableRead);
        psTableRead = NULL;
    }

    if (pszTableFilename)
    {
        CPLFree(pszTableFilename);
        pszTableFilename = NULL;
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

    bNeedReset = FALSE;
    nNextFID = 1;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRAVCE00Layer::GetFeature( GIntBig nFID )

{
/* -------------------------------------------------------------------- */
/*      If we haven't started yet, open the file now.                   */
/* -------------------------------------------------------------------- */
    if( psRead == NULL )
    {
        psRead = AVCE00ReadOpenE00(psSection->pszFilename);
        if (psRead == NULL)
            return NULL;
        /* advance to the specified line number */
        if (AVCE00ReadGotoSectionE00(psRead, psSection, 0) != 0)
            return NULL;
        nNextFID = 1;
    }

/* -------------------------------------------------------------------- */
/*      Read the raw feature - the -3 fid is a special flag             */
/*      indicating serial access.                                       */
/* -------------------------------------------------------------------- */
    void *pFeature;

    if( nFID == -3 )
    {
        while( (pFeature = AVCE00ReadNextObjectE00(psRead)) != NULL
               && psRead->hParseInfo->eFileType != AVCFileUnknown
               && !MatchesSpatialFilter( pFeature ) )
        {
            nNextFID++;
        }
    }
    else
    {
        bNeedReset = TRUE;

        if (nNextFID > nFID)
        {
            /* advance to the specified line number */
            if (AVCE00ReadGotoSectionE00(psRead, psSection, 0) != 0)
                return NULL;
        }

        do
        {
            pFeature = AVCE00ReadNextObjectE00(psRead);
            ++nNextFID;
        }
        while (NULL != pFeature && nNextFID <= nFID);
    }
        
    if( pFeature == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Translate the feature.                                          */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature;

    poFeature = TranslateFeature( pFeature );
    if( poFeature == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      LAB's we have to assign the FID to directly, since it           */
/*      doesn't seem to be stored in the file structure.                */
/* -------------------------------------------------------------------- */
    if( psSection->eType == AVCFileLAB )
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
    if( psSection->eType == AVCFilePAL 
        || psSection->eType == AVCFileRPL )
    {
        FormPolygonGeometry( poFeature, (AVCPal *) pFeature );
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
    if( bNeedReset )
        ResetReading();

    OGRFeature *poFeature = GetFeature( -3 );

    // Skip universe polygon.
    if( poFeature != NULL && poFeature->GetFID() == 1 
        && psSection->eType == AVCFilePAL )
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

#if 0
int OGRAVCE00Layer::TestCapability( const char * pszCap )

{
    if( eSectionType == AVCFileARC && EQUAL(pszCap,OLCRandomRead) )
        return TRUE;
    else 
        return OGRAVCLayer::TestCapability( pszCap );
}
#endif

/************************************************************************/
/*                        FormPolygonGeometry()                         */
/*                                                                      */
/*      Collect all the arcs forming edges to this polygon and form     */
/*      them into the appropriate OGR geometry on the target feature.   */
/************************************************************************/

int OGRAVCE00Layer::FormPolygonGeometry( OGRFeature *poFeature, 
                                         AVCPal *psPAL )
{
/* -------------------------------------------------------------------- */
/*      Try to find the corresponding ARC layer if not already          */
/*      recorded.                                                       */
/* -------------------------------------------------------------------- */
    if( poArcLayer == NULL )
    {
        int i;

        for( i = 0; i < poDS->GetLayerCount(); i++ )
        {
            OGRAVCE00Layer *poLayer = (OGRAVCE00Layer *) poDS->GetLayer(i);

            if( poLayer->eSectionType == AVCFileARC )
                poArcLayer = poLayer;
        }

        if( poArcLayer == NULL )
            return FALSE;
    }

/* -------------------------------------------------------------------- */
/*  Read all the arcs related to this polygon, making a working         */
/*  copy of them since the one returned by AVC is temporary.            */
/* -------------------------------------------------------------------- */
    OGRGeometryCollection oArcs;
    int iArc;

    for( iArc = 0; iArc < psPAL->numArcs; iArc++ )
    {
        OGRFeature *poArc;

        if( psPAL->pasArcs[iArc].nArcId == 0 )
            continue;

        // If the other side of the line is the same polygon then this
        // arc is a "bridge" arc and can be discarded.  If we don't discard
        // it, then we should double it as bridge arcs seem to only appear
        // once.  But by discarding it we ensure a multi-ring polygon will be
        // properly formed. 
        if( psPAL->pasArcs[iArc].nAdjPoly == psPAL->nPolyId )
            continue;

        poArc = poArcLayer->GetFeature( ABS(psPAL->pasArcs[iArc].nArcId) );

        if( poArc == NULL )
            return FALSE;

        if( poArc->GetGeometryRef() == NULL )
            return FALSE;

        oArcs.addGeometry( poArc->GetGeometryRef() );
        OGRFeature::DestroyFeature( poArc );
    }

    OGRErr  eErr;
    OGRPolygon *poPolygon;

    poPolygon = (OGRPolygon *) 
        OGRBuildPolygonFromEdges( (OGRGeometryH) &oArcs, TRUE, FALSE,  
                                  0.0, &eErr );
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

int OGRAVCE00Layer::CheckSetupTable(AVCE00Section *psTblSectionIn)
{
    if (psTableRead)
        return FALSE;

    const char *pszTableType = NULL;
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
    if( pszTableType == NULL )
        return FALSE;
    
    int iCheckOff;
    for( iCheckOff = 0; 
         psTblSectionIn->pszName[iCheckOff] != '\0'; 
         iCheckOff++ )
    {
        if( EQUALN(psTblSectionIn->pszName + iCheckOff, 
                   pszTableType, strlen(pszTableType) ) )
            break;
    }

    if( psTblSectionIn->pszName[iCheckOff] == '\0' )
        return FALSE;

    psTableSection = psTblSectionIn;

/* -------------------------------------------------------------------- */
/*      Try opening the table.                                          */
/* -------------------------------------------------------------------- */
    psTableRead = AVCE00ReadOpenE00(psTblSectionIn->pszFilename);
    if (psTableRead == NULL)
        return FALSE;

    /* advance to the specified line number */
    if (AVCE00ReadGotoSectionE00(psTableRead, psTableSection, 0) != 0)
    {
        AVCE00ReadCloseE00(psTableRead);
        psTableRead = NULL;
        return FALSE;
    }
    
    AVCE00ReadNextObjectE00(psTableRead);
    bNeedReset = 1;

    pszTableFilename = CPLStrdup(psTblSectionIn->pszFilename);
    nTableBaseField = poFeatureDefn->GetFieldCount();

	if (eSectionType == AVCFileLAB)
	{
        AVCE00ReadE00Ptr psInfo = ((OGRAVCE00DataSource *) poDS)->GetInfo();
        for( int iSection = 0; iSection < psInfo->numSections; iSection++ )
        {
            if( psInfo->pasSections[iSection].eType == AVCFilePAL )
                nTableAttrIndex = poFeatureDefn->GetFieldIndex( "PolyId" );
        }
	}

/* -------------------------------------------------------------------- */
/*      Setup attributes.                                               */
/* -------------------------------------------------------------------- */
    AppendTableDefinition( psTableRead->hParseInfo->hdr.psTableDef );

/* -------------------------------------------------------------------- */
/*      Close table so we don't have to many files open at once.        */
/* -------------------------------------------------------------------- */
    /* AVCE00ReadCloseE00( psTableRead ); */

    return TRUE;
}

/************************************************************************/
/*                         AppendTableFields()                          */
/************************************************************************/

int OGRAVCE00Layer::AppendTableFields( OGRFeature *poFeature )

{
    if (psTableRead == NULL)
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Open the table if it is currently closed.                       */
/* -------------------------------------------------------------------- */
    if (psTableRead == NULL)
    {
        psTableRead = AVCE00ReadOpenE00(pszTableFilename);
        if (psTableRead == NULL)
            return FALSE;

        /* advance to the specified line number */
    	if (AVCE00ReadGotoSectionE00(psTableRead, psTableSection, 0) != 0)
        {
            AVCE00ReadCloseE00(psTableRead);
            psTableRead = NULL;
            return FALSE;
        }
        nTablePos = 0;
    }

/* -------------------------------------------------------------------- */
/*      Read the info record.                                           */
/*                                                                      */
/*      We usually assume the FID of the feature is the key but in a    */
/*      polygon coverage we need to use the PolyId attribute of LAB     */
/*      features to lookup the related attributes.  In this case        */
/*      nTableAttrIndex will already be setup to refer to the           */
/*      PolyId field.                                                   */
/* -------------------------------------------------------------------- */
    int nRecordId;
    void *hRecord;

    if( nTableAttrIndex == -1 )
        nRecordId = poFeature->GetFID();
    else
        nRecordId = poFeature->GetFieldAsInteger( nTableAttrIndex );

    if (nRecordId <= nTablePos)
    {
    	if (AVCE00ReadGotoSectionE00(psTableRead, psTableSection, 0) != 0)
			return FALSE;
        nTablePos = 0;
    }

    do
    {
        hRecord = AVCE00ReadNextObjectE00(psTableRead);
        ++nTablePos;
    }
    while (NULL != hRecord && nTablePos < nRecordId);

    if( hRecord == NULL )
        return FALSE;


/* -------------------------------------------------------------------- */
/*      Translate it.                                                   */
/* -------------------------------------------------------------------- */
    return TranslateTableFields( poFeature, nTableBaseField, 
                                 psTableRead->hParseInfo->hdr.psTableDef, 
                                 (AVCField *) hRecord );
}


GIntBig OGRAVCE00Layer::GetFeatureCount(int bForce)
{
    if (m_poAttrQuery != NULL || m_poFilterGeom != NULL)
        return OGRAVCLayer::GetFeatureCount(bForce);

    if (bForce && nFeatureCount < 0)
    {
        if (psSection->nFeatureCount < 0)
        {
            nFeatureCount = OGRLayer::GetFeatureCount(bForce);
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

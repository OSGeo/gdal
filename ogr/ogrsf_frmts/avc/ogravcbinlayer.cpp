/******************************************************************************
 * $Id$
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  2002/02/13 20:48:18  warmerda
 * New
 *
 */

#include "ogr_avc.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                           OGRAVCBinLayer()                           */
/************************************************************************/

OGRAVCBinLayer::OGRAVCBinLayer( OGRAVCBinDataSource *poDSIn,
                                AVCE00Section *psSectionIn )

{
    psSection = psSectionIn;
    poDS = poDSIn;
    hFile = NULL;
    poArcLayer = NULL;
    bNeedReset = FALSE;

    SetupFeatureDefinition( psSection->eType, psSection->pszName );
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

    bNeedReset = FALSE;
}

/************************************************************************/
/*                           GetRawFeature()                            */
/*                                                                      */
/*      Note that the returned feature remains owned by the AVC         */
/*      library and won't last long.                                    */
/************************************************************************/

void *OGRAVCBinLayer::GetRawFeature( long nFID )

{
/* -------------------------------------------------------------------- */
/*      If we haven't started yet, open the file now.                   */
/* -------------------------------------------------------------------- */
    if( hFile == NULL )
    {
        hFile = AVCBinReadOpen(poDS->GetInfo()->pszCoverPath, 
                               psSection->pszFilename, 
                               poDS->GetInfo()->eCoverType, 
                               psSection->eType,
                               poDS->GetInfo()->psDBCSInfo);
    }

/* -------------------------------------------------------------------- */
/*      Sequential reading order will be confused by having hFile       */
/*      opened, but doing a random read.  Note that the next            */
/*      sequential read will need to reset the reading.                 */
/* -------------------------------------------------------------------- */
    bNeedReset = TRUE;

/* -------------------------------------------------------------------- */
/*      Read the feature/record.                                        */
/* -------------------------------------------------------------------- */
    return AVCBinReadObject( hFile, nFID );
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRAVCBinLayer::GetFeature( long nFID )

{
    void *pFeature;

    pFeature = GetRawFeature( nFID );
    if( pFeature == NULL )
        return NULL;

    return TranslateFeature( pFeature );
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRAVCBinLayer::GetNextFeature()

{
    if( bNeedReset )
        ResetReading();

/* -------------------------------------------------------------------- */
/*      If we haven't started yet, open the file now.                   */
/* -------------------------------------------------------------------- */
    if( hFile == NULL )
    {
        hFile = AVCBinReadOpen(poDS->GetInfo()->pszCoverPath, 
                               psSection->pszFilename, 
                               poDS->GetInfo()->eCoverType, 
                               psSection->eType,
                               poDS->GetInfo()->psDBCSInfo);
    }

/* -------------------------------------------------------------------- */
/*      Read a feature/record.                                          */
/* -------------------------------------------------------------------- */
    void	*pFeature;

    pFeature = AVCBinReadNextObject( hFile );

/* -------------------------------------------------------------------- */
/*      If we are out of features, close the file, and reset            */
/*      conditions.                                                     */
/* -------------------------------------------------------------------- */
    if( pFeature == NULL )
    {
        AVCBinReadClose( hFile );
        hFile = NULL;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Translate the feature                                           */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature = TranslateFeature( pFeature );

    if( poFeature == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      If this is a polygon layer, try to assemble the arcs to form    */
/*      the whole polygon geometry.                                     */
/* -------------------------------------------------------------------- */
    if( psSection->eType == AVCFilePAL )
        FormPolygonGeometry( poFeature, (AVCPal *) pFeature );

    return poFeature;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRAVCBinLayer::TestCapability( const char * pszCap )

{
    if( eSectionType == AVCFileARC && EQUAL(pszCap,OLCRandomRead) )
        return TRUE;
    else 
        return OGRAVCLayer::TestCapability( pszCap );
}

/************************************************************************/
/*                        FormPolygonGeometry()                         */
/*                                                                      */
/*      Collect all the arcs forming edges to this polygon and form     */
/*      them into the appropriate OGR geometry on the target feature.   */
/************************************************************************/

int OGRAVCBinLayer::FormPolygonGeometry( OGRFeature *poFeature, 
                                         AVCPal *psPAL )

{
/* -------------------------------------------------------------------- */
/*      Try to find the corresponding ARC layer if not already          */
/*      recorded.                                                       */
/* -------------------------------------------------------------------- */
    if( poArcLayer == NULL )
    {
        int	i;

        for( i = 0; i < poDS->GetLayerCount(); i++ )
        {
            OGRAVCBinLayer *poLayer = (OGRAVCBinLayer *) poDS->GetLayer(i);

            if( poLayer->eSectionType == AVCFileARC )
                poArcLayer = poLayer;
        }

        if( poArcLayer == NULL )
            return FALSE;
    }

/* -------------------------------------------------------------------- */
/*	Read all the arcs related to this polygon, making a working	*/
/*	copy of them since the one returned by AVC is temporary.	*/
/* -------------------------------------------------------------------- */
    OGRGeometryCollection oArcs;
    int		iArc;

    for( iArc = 0; iArc < psPAL->numArcs; iArc++ )
    {
        OGRFeature *poArc;

        if( psPAL->pasArcs[iArc].nArcId == 0 )
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

    poPolygon = OGRBuildPolygonFromEdges( &oArcs, TRUE, &eErr );
    if( poPolygon != NULL )
        poFeature->SetGeometryDirectly( poPolygon );

    return eErr == OGRERR_NONE;
}

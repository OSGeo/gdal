/******************************************************************************
 * $Id$
 *
 * Project:  Contour Generation
 * Purpose:  Core algorithm implementation for contour line generation. 
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2003, Applied Coherent Technology Corporation, www.actgate.com
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
 * Revision 1.1  2003/10/10 19:44:46  warmerda
 * New
 *
 */

#include "gdal_priv.h"
#include "gdal_alg.h"
#include "ogr_api.h"

CPL_CVSID("$Id$");

// The amount of a contour interval that pixels should be fudged by if they
// match a contour level exactly.

#define FUDGE_EXACT 0.001

// The amount of a pixel that line ends need to be within to be considered to
// match for joining purposes. 

#define JOIN_DIST 0.0001

/************************************************************************/
/* ==================================================================== */
/*                         GDALContourGenerator                         */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                        GDALContourGenerator()                        */
/************************************************************************/

GDALContourGenerator::GDALContourGenerator( int nWidthIn, int nHeightIn,
                                            GDALContourWriter pfnWriterIn, 
                                            void *pWriterCBDataIn )
{
    nWidth = nWidthIn;
    nHeight = nHeightIn;

    padfLastLine = (double *) CPLCalloc(sizeof(double),nWidth);
    padfThisLine = (double *) CPLCalloc(sizeof(double),nWidth);

    pfnWriter = pfnWriterIn;
    pWriterCBData = pWriterCBDataIn;

    iLine = -1;
    nActiveContours = 0;
    nMaxContours = 0;
    papoContours = NULL;

    bNoDataActive = FALSE;
    dfNoDataValue = -1000000.0;
    dfContourInterval = 10.0;
    dfContourOffset = 0.0;
}

/************************************************************************/
/*                       ~GDALContourGenerator()                        */
/************************************************************************/

GDALContourGenerator::~GDALContourGenerator()

{
    
}

/************************************************************************/
/*                             SetNoData()                              */
/************************************************************************/

void GDALContourGenerator::SetNoData( double dfNewValue )

{
    bNoDataActive = TRUE;
    dfNoDataValue = dfNewValue;
}

/************************************************************************/
/*                            ProcessPixel()                            */
/************************************************************************/

CPLErr GDALContourGenerator::ProcessPixel( int iPixel )

{
    double  dfUpLeft, dfUpRight, dfLoLeft, dfLoRight;
    int     bSubdivide = FALSE;

/* -------------------------------------------------------------------- */
/*      Collect the four corner pixel values.  Value left or right      */
/*      of the scanline are taken from the nearest pixel on the         */
/*      scanline itself.                                                */
/* -------------------------------------------------------------------- */
    dfUpLeft = padfLastLine[MAX(0,iPixel-1)];
    dfUpRight = padfLastLine[MIN(nWidth-1,iPixel)];
    
    dfLoLeft = padfThisLine[MAX(0,iPixel-1)];
    dfLoRight = padfThisLine[MIN(nWidth-1,iPixel)];

/* -------------------------------------------------------------------- */
/*      Check if we have any nodata values.                             */
/* -------------------------------------------------------------------- */
    if( bNoDataActive 
        && ( dfUpLeft == dfNoDataValue
             || dfLoLeft == dfNoDataValue
             || dfLoRight == dfNoDataValue
             || dfUpRight == dfNoDataValue ) )
        bSubdivide = TRUE;

/* -------------------------------------------------------------------- */
/*      Check if we have any nodata, if so, go to a special case of     */
/*      code.                                                           */
/* -------------------------------------------------------------------- */
    if( iPixel > 0 && iPixel < nWidth 
        && iLine > 0 && iLine < nHeight && !bSubdivide )
    {
        return ProcessRect( dfUpLeft, iPixel - 0.5, iLine - 0.5, 
                            dfLoLeft, iPixel - 0.5, iLine + 0.5, 
                            dfLoRight, iPixel + 0.5, iLine + 0.5, 
                            dfUpRight, iPixel + 0.5, iLine - 0.5 );
    }

/* -------------------------------------------------------------------- */
/*      Prepare subdivisions.                                           */
/* -------------------------------------------------------------------- */
    int nGoodCount = 0; 
    double dfASum = 0.0;
    double dfCenter, dfTop=0.0, dfRight=0.0, dfLeft=0.0, dfBottom=0.0;
    
    if( dfUpLeft != dfNoDataValue )
    {
        dfASum += dfUpLeft;
        nGoodCount++;
    }

    if( dfLoLeft != dfNoDataValue )
    {
        dfASum += dfLoLeft;
        nGoodCount++;
    }

    if( dfLoRight != dfNoDataValue )
    {
        dfASum += dfLoRight;
        nGoodCount++;
    }

    if( dfUpRight != dfNoDataValue )
    {
        dfASum += dfUpRight;
        nGoodCount++;
    }

    if( nGoodCount == 0.0 )
        return CE_None;

    dfCenter = dfASum / nGoodCount;

    if( dfUpLeft != dfNoDataValue )
    {
        if( dfUpRight != dfNoDataValue )
            dfTop = (dfUpLeft + dfUpRight) / 2.0;
        else
            dfTop = dfUpLeft;

        if( dfLoLeft != dfNoDataValue )
            dfLeft = (dfUpLeft + dfLoLeft) / 2.0;
        else
            dfLeft = dfUpLeft;
    }
    else
    {
        dfTop = dfUpRight;
        dfLeft = dfLoLeft;
    }

    if( dfLoRight != dfNoDataValue )
    {
        if( dfUpRight != dfNoDataValue )
            dfRight = (dfLoRight + dfUpRight) / 2.0;
        else
            dfRight = dfLoRight;

        if( dfLoLeft != dfNoDataValue )
            dfBottom = (dfLoRight + dfLoLeft) / 2.0;
        else
            dfBottom = dfLoRight;
    }
    else
    {
        dfBottom = dfLoLeft;;
        dfRight = dfUpRight;
    }

/* -------------------------------------------------------------------- */
/*      Process any quadrants that aren't "nodata" anchored.            */
/* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;

    if( dfUpLeft != dfNoDataValue && iPixel > 0 && iLine > 0 )
    {
        eErr = ProcessRect( dfUpLeft, iPixel - 0.5, iLine - 0.5, 
                            dfLeft, iPixel - 0.5, iLine, 
                            dfCenter, iPixel, iLine, 
                            dfTop, iPixel, iLine - 0.5 );
    }

    if( dfLoLeft != dfNoDataValue && eErr == CE_None 
        && iPixel > 0 && iLine < nHeight )
    {
        eErr = ProcessRect( dfLeft, iPixel - 0.5, iLine, 
                            dfLoLeft, iPixel - 0.5, iLine + 0.5,
                            dfBottom, iPixel, iLine + 0.5, 
                            dfCenter, iPixel, iLine );
    }

    if( dfLoRight != dfNoDataValue && iPixel < nWidth && iLine < nHeight )
    {
        eErr = ProcessRect( dfCenter, iPixel, iLine, 
                            dfBottom, iPixel, iLine + 0.5,
                            dfLoRight, iPixel + 0.5, iLine + 0.5, 
                            dfRight, iPixel + 0.5, iLine );
    }

    if( dfUpRight != dfNoDataValue && iPixel < nWidth && iLine > 0 )
    {
        eErr = ProcessRect( dfTop, iPixel, iLine - 0.5, 
                            dfCenter, iPixel, iLine,
                            dfRight, iPixel + 0.5, iLine, 
                            dfUpRight, iPixel + 0.5, iLine - 0.5 );
    }

    return eErr;
}

/************************************************************************/
/*                            ProcessRect()                             */
/************************************************************************/

CPLErr GDALContourGenerator::ProcessRect( 
    double dfUpLeft, double dfUpLeftX, double dfUpLeftY, 
    double dfLoLeft, double dfLoLeftX, double dfLoLeftY, 
    double dfLoRight, double dfLoRightX, double dfLoRightY, 
    double dfUpRight, double dfUpRightX, double dfUpRightY )
    
{
/* -------------------------------------------------------------------- */
/*      Identify the range of contour levels we have to deal with.      */
/* -------------------------------------------------------------------- */
    int iStartLevel, iEndLevel;

    double dfMin = MIN(MIN(dfUpLeft,dfUpRight),MIN(dfLoLeft,dfLoRight));
    double dfMax = MAX(MAX(dfUpLeft,dfUpRight),MAX(dfLoLeft,dfLoRight));
    
    iStartLevel = (int) floor((dfMin - dfContourOffset) / dfContourInterval);
    iEndLevel = (int)   floor((dfMax - dfContourOffset) / dfContourInterval);

    if( iStartLevel == iEndLevel )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Loop over them.                                                 */
/* -------------------------------------------------------------------- */
    int iLevel;

    for( iLevel = iStartLevel+1; iLevel <= iEndLevel; iLevel++ )
    {
        double dfLevel = iLevel * dfContourInterval + dfContourOffset;
        int  nPoints = 0; 
        double adfX[4], adfY[4];
        CPLErr eErr;

        Intersect( dfUpLeft, dfUpLeftX, dfUpLeftY,
                   dfLoLeft, dfLoLeftX, dfLoLeftY,
                   dfLoRight, dfLevel, &nPoints, adfX, adfY );
        Intersect( dfLoLeft, dfLoLeftX, dfLoLeftY,
                   dfLoRight, dfLoRightX, dfLoRightY,
                   dfUpRight, dfLevel, &nPoints, adfX, adfY );
        Intersect( dfLoRight, dfLoRightX, dfLoRightY,
                   dfUpRight, dfUpRightX, dfUpRightY,
                   dfUpLeft, dfLevel, &nPoints, adfX, adfY );
        Intersect( dfUpRight, dfUpRightX, dfUpRightY,
                   dfUpLeft, dfUpLeftX, dfUpLeftY,
                   dfLoLeft, dfLevel, &nPoints, adfX, adfY );
        
        if( nPoints == 1 || nPoints == 3 )
            CPLDebug( "CONTOUR", "Got nPoints = %d", nPoints );

        if( nPoints >= 2 )
        {
            eErr = AddSegment( dfLevel, adfX[0], adfY[0], adfX[1], adfY[1] );

            if( eErr != CE_None )
                return eErr;
        }

        if( nPoints == 4 )
        {
            eErr = AddSegment( dfLevel, adfX[2], adfY[2], adfX[3], adfY[3] );
            if( eErr != CE_None )
                return eErr;
        }
    }

    return CE_None;
}

/************************************************************************/
/*                             Intersect()                              */
/************************************************************************/

void GDALContourGenerator::Intersect( double dfVal1, double dfX1, double dfY1, 
                                      double dfVal2, double dfX2, double dfY2,
                                      double dfNext, 
                                      double dfLevel, int *pnPoints, 
                                      double *padfX, double *padfY )

{
    if( dfVal1 < dfLevel && dfVal2 >= dfLevel )
    {
        double dfRatio = (dfLevel - dfVal1) / (dfVal2 - dfVal1);

        padfX[*pnPoints] = dfX1 * (1.0 - dfRatio) + dfX2 * dfRatio;
        padfY[*pnPoints] = dfY1 * (1.0 - dfRatio) + dfY2 * dfRatio;
        (*pnPoints)++;
    }
    else if( dfVal1 > dfLevel && dfVal2 <= dfLevel )
    {
        double dfRatio = (dfLevel - dfVal2) / (dfVal1 - dfVal2);

        padfX[*pnPoints] = dfX2 * (1.0 - dfRatio) + dfX1 * dfRatio;
        padfY[*pnPoints] = dfY2 * (1.0 - dfRatio) + dfY1 * dfRatio;
        (*pnPoints)++;
    }
    else if( dfVal1 == dfLevel && dfVal2 == dfLevel && dfNext != dfLevel )
    {
        padfX[*pnPoints] = dfX2;
        padfY[*pnPoints] = dfY2;
        (*pnPoints)++;
    }
}

/************************************************************************/
/*                             AddSegment()                             */
/************************************************************************/

CPLErr GDALContourGenerator::AddSegment( double dfLevel, 
                                         double dfX1, double dfY1,
                                         double dfX2, double dfY2 )

{
/* -------------------------------------------------------------------- */
/*      Check all active contours for any that this might attach        */
/*      to. Eventually this should be recoded to find the contours      */
/*      of the correct level more efficiently.                          */
/* -------------------------------------------------------------------- */
    int  iContour;

    for( iContour = 0; iContour < nActiveContours; iContour++ )
    {
        if( papoContours[iContour]->dfLevel == dfLevel 
            && papoContours[iContour]->AddSegment( dfX1, dfY1, dfX2, dfY2 ) )
            return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      No existing contour found, lets create a new one.               */
/* -------------------------------------------------------------------- */
    if( nActiveContours == nMaxContours )
    {
        nMaxContours = nMaxContours * 2 + 100;
        papoContours = (GDALContourItem **) 
            CPLRealloc(papoContours,sizeof(void*) * nMaxContours);
    }

    papoContours[nActiveContours++] = new GDALContourItem( dfLevel );

    papoContours[nActiveContours-1]->AddSegment( dfX1, dfY1, dfX2, dfY2 );

    return CE_None;
}

/************************************************************************/
/*                              FeedLine()                              */
/************************************************************************/

CPLErr GDALContourGenerator::FeedLine( double *padfScanline )

{
/* -------------------------------------------------------------------- */
/*      Switch current line to "lastline" slot, and copy new data       */
/*      into new "this line".                                           */
/* -------------------------------------------------------------------- */
    double *padfTempLine = padfLastLine;
    padfLastLine = padfThisLine;
    padfThisLine = padfTempLine;

/* -------------------------------------------------------------------- */
/*      If this is the end of the lines (NULL passed in), copy the      */
/*      last line.                                                      */
/* -------------------------------------------------------------------- */
    if( padfScanline == NULL )
    {
        memcpy( padfThisLine, padfLastLine, sizeof(double) * nWidth );
    }
    else
    {
        memcpy( padfThisLine, padfScanline, sizeof(double) * nWidth );
    }

/* -------------------------------------------------------------------- */
/*      Perturb any values that occur exactly on level boundaries.      */
/* -------------------------------------------------------------------- */
    int iPixel;

    for( int iPixel = 0; iPixel < nWidth; iPixel++ )
    {
        double dfLevel = (padfThisLine[iPixel] - dfContourOffset) 
            / dfContourInterval;

        if( dfLevel - (int) dfLevel == 0.0 )
        {
            padfThisLine[iPixel] += dfContourInterval * FUDGE_EXACT;
        }
    }

/* -------------------------------------------------------------------- */
/*      If this is the first line we need to initialize the previous    */
/*      line from the first line of data.                               */
/* -------------------------------------------------------------------- */
    if( iLine == -1 )
    {
        memcpy( padfLastLine, padfThisLine, sizeof(double) * nWidth );
        iLine = 0;
    }

/* -------------------------------------------------------------------- */
/*      Clear the recently used flags on the contours so we can         */
/*      check later which ones were touched for this scanline.          */
/* -------------------------------------------------------------------- */
    int iContour;

    for( iContour = 0; iContour < nActiveContours; iContour++ )
        papoContours[iContour]->bRecentlyAccessed = FALSE;

/* -------------------------------------------------------------------- */
/*      Process each pixel.                                             */
/* -------------------------------------------------------------------- */
    for( iPixel = 0; iPixel < nWidth+1; iPixel++ )
    {
        CPLErr eErr = ProcessPixel( iPixel );
        if( eErr != CE_None )
            return eErr;
    }
    
/* -------------------------------------------------------------------- */
/*      eject any pending contours.                                     */
/* -------------------------------------------------------------------- */
    CPLErr eErr = EjectContours( padfScanline != NULL );

    iLine++;

    if( iLine == nHeight && eErr == CE_None )
        return FeedLine( NULL );
    else
        return eErr;
}

/************************************************************************/
/*                           EjectContours()                            */
/************************************************************************/

CPLErr GDALContourGenerator::EjectContours( int bOnlyUnused )

{
    int nContoursKept = 0, iContour;
    CPLErr eErr = CE_None;

/* -------------------------------------------------------------------- */
/*      Try to merge contours that are about to be ejected.  For now    */
/*      we don't worry about updating the total count or reshuffling    */
/*      things.  The later "write out" pass will fix things up.         */
/* -------------------------------------------------------------------- */
    for( iContour = 0; 
         iContour < nActiveContours && eErr == CE_None; 
         iContour++ )
    {
        int  iC2;

        if( bOnlyUnused && papoContours[iContour]->bRecentlyAccessed )
            continue;

        for( iC2 = 0; iC2 < nActiveContours; iC2++ )
        {
            if( iC2 == iContour || papoContours[iC2] == NULL )
                continue;

            if( papoContours[iC2]->Merge( papoContours[iContour] ) )
            {
                delete papoContours[iContour];
                papoContours[iContour] = NULL;
                break;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Write candidate contours.                                       */
/* -------------------------------------------------------------------- */
    for( iContour = 0; 
         iContour < nActiveContours && eErr == CE_None; 
         iContour++ )
    {
        if( papoContours[iContour] == NULL )
            continue;

        if( bOnlyUnused && papoContours[iContour]->bRecentlyAccessed )
        {
            papoContours[nContoursKept++] = papoContours[iContour];
            continue;
        }

        if( pfnWriter != NULL )
        {
            eErr = pfnWriter( papoContours[iContour], pWriterCBData );
            if( eErr != CE_None )
                break;
        }

        delete papoContours[iContour];
        papoContours[iContour] = NULL;
    }

    nActiveContours = nContoursKept;

    return eErr;
}

/************************************************************************/
/* ==================================================================== */
/*                           GDALContourItem                            */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                          GDALContourItem()                           */
/************************************************************************/

GDALContourItem::GDALContourItem( double dfLevelIn )

{
    dfLevel = dfLevelIn;
    bRecentlyAccessed = FALSE;
    nPoints = 0;
    nMaxPoints = 0;
    padfX = NULL;
    padfY = NULL;
}

/************************************************************************/
/*                          ~GDALContourItem()                          */
/************************************************************************/

GDALContourItem::~GDALContourItem()

{
    CPLFree( padfX );
    CPLFree( padfY );
}

/************************************************************************/
/*                             AddSegment()                             */
/************************************************************************/

int GDALContourItem::AddSegment( double dfXStart, double dfYStart, 
                                 double dfXEnd, double dfYEnd )

{
    MakeRoomFor( nPoints + 1 );

/* -------------------------------------------------------------------- */
/*      If there are no segments, just add now.                         */
/* -------------------------------------------------------------------- */
    if( nPoints == 0 )
    {
        nPoints = 2;
        padfX[0] = dfXStart;
        padfY[0] = dfYStart;
        padfX[1] = dfXEnd;
        padfY[1] = dfYEnd;
        bRecentlyAccessed = TRUE;

        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Try to matching up with one of the ends, and insert.            */
/* -------------------------------------------------------------------- */
    if( fabs(padfX[nPoints-1]-dfXStart) < JOIN_DIST 
             && fabs(padfY[nPoints-1]-dfYStart) < JOIN_DIST )
    {
        padfX[nPoints] = dfXEnd;
        padfY[nPoints] = dfYEnd;
        nPoints++;

        bRecentlyAccessed = TRUE;

        return TRUE;
    }
    else if( fabs(padfX[nPoints-1]-dfXEnd) < JOIN_DIST 
             && fabs(padfY[nPoints-1]-dfYEnd) < JOIN_DIST )
    {
        padfX[nPoints] = dfXStart;
        padfY[nPoints] = dfYStart;
        nPoints++;

        bRecentlyAccessed = TRUE;

        return TRUE;
    }
#ifdef notdef
    else if( fabs(padfX[0]-dfXStart) < JOIN_DIST && fabs(padfY[0]-dfYStart) < JOIN_DIST )
    {
        memmove( padfX + 1, padfX, sizeof(double) * nPoints );
        memmove( padfY + 1, padfY, sizeof(double) * nPoints );
        padfX[0] = dfXEnd;
        padfY[0] = dfYEnd;
        nPoints++;

        bRecentlyAccessed = TRUE;

        return TRUE;
    }
    else if( fabs(padfX[0]-dfXEnd) < JOIN_DIST && fabs(padfY[0]-dfYEnd) < JOIN_DIST )
    {
        memmove( padfX + 1, padfX, sizeof(double) * nPoints );
        memmove( padfY + 1, padfY, sizeof(double) * nPoints );
        padfX[0] = dfXStart;
        padfY[0] = dfYStart;
        nPoints++;

        bRecentlyAccessed = TRUE;

        return TRUE;
    }
#endif
    else
        return FALSE;
}
 
/************************************************************************/
/*                               Merge()                                */
/************************************************************************/

int GDALContourItem::Merge( GDALContourItem *poOther )

{
    if( poOther->dfLevel != dfLevel )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Try to matching up with one of the ends, and insert.            */
/* -------------------------------------------------------------------- */
    if( fabs(padfX[nPoints-1]-poOther->padfX[0]) < JOIN_DIST 
        && fabs(padfY[nPoints-1]-poOther->padfY[0]) < JOIN_DIST )
    {
        MakeRoomFor( nPoints + poOther->nPoints - 1 );

        memcpy( padfX + nPoints, poOther->padfX + 1, 
                sizeof(double) * (poOther->nPoints-1) );
        memcpy( padfY + nPoints, poOther->padfY + 1, 
                sizeof(double) * (poOther->nPoints-1) );
        nPoints += poOther->nPoints - 1;

        bRecentlyAccessed = TRUE;

        return TRUE;
    }
    else if( fabs(padfX[0]-poOther->padfX[poOther->nPoints-1]) < JOIN_DIST 
             && fabs(padfY[0]-poOther->padfY[poOther->nPoints-1]) < JOIN_DIST )
    {
        MakeRoomFor( nPoints + poOther->nPoints - 1 );

        memmove( padfX + poOther->nPoints - 1, padfX, 
                sizeof(double) * nPoints );
        memmove( padfY + poOther->nPoints - 1, padfY, 
                sizeof(double) * nPoints );
        memcpy( padfX, poOther->padfX, 
                sizeof(double) * (poOther->nPoints-1) );
        memcpy( padfY, poOther->padfY, 
                sizeof(double) * (poOther->nPoints-1) );
        nPoints += poOther->nPoints - 1;

        bRecentlyAccessed = TRUE;

        return TRUE;
    }
    else if( fabs(padfX[nPoints-1]-poOther->padfX[poOther->nPoints-1]) < JOIN_DIST 
        && fabs(padfY[nPoints-1]-poOther->padfY[poOther->nPoints-1]) < JOIN_DIST )
    {
        int i;

        MakeRoomFor( nPoints + poOther->nPoints - 1 );

        for( i = 0; i < poOther->nPoints-1; i++ )
        {
            padfX[i+nPoints] = poOther->padfX[poOther->nPoints-i-2];
            padfY[i+nPoints] = poOther->padfY[poOther->nPoints-i-2];
        }

        nPoints += poOther->nPoints - 1;

        bRecentlyAccessed = TRUE;

        return TRUE;
    }
    else if( fabs(padfX[0]-poOther->padfX[0]) < JOIN_DIST 
        && fabs(padfY[0]-poOther->padfY[0]) < JOIN_DIST )
    {
        int i;

        MakeRoomFor( nPoints + poOther->nPoints - 1 );

        memmove( padfX + poOther->nPoints - 1, padfX, 
                sizeof(double) * nPoints );
        memmove( padfY + poOther->nPoints - 1, padfY, 
                sizeof(double) * nPoints );

        for( i = 0; i < poOther->nPoints-1; i++ )
        {
            padfX[i] = poOther->padfX[poOther->nPoints - i - 1];
            padfY[i] = poOther->padfY[poOther->nPoints - i - 1];
        }

        nPoints += poOther->nPoints - 1;

        bRecentlyAccessed = TRUE;

        return TRUE;
    }
    else
        return FALSE;
}

/************************************************************************/
/*                            MakeRoomFor()                             */
/************************************************************************/

void GDALContourItem::MakeRoomFor( int nNewPoints )

{
    if( nNewPoints > nMaxPoints )
    {
        nMaxPoints = nNewPoints * 2 + 50;
        padfX = (double *) CPLRealloc(padfX,sizeof(double) * nMaxPoints);
        padfY = (double *) CPLRealloc(padfY,sizeof(double) * nMaxPoints);
    }
}

/************************************************************************/
/* ==================================================================== */
/*                   Additional C Callable Functions                    */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                          OGRContourWriter()                          */
/************************************************************************/

CPLErr OGRContourWriter( GDALContourItem *poContour, void *pInfo )

{
    OGRContourWriterInfo *poInfo = (OGRContourWriterInfo *) pInfo;
    OGRFeatureH hFeat;
    OGRGeometryH hGeom;
    int iPoint;

    hFeat = OGR_F_Create( OGR_L_GetLayerDefn( poInfo->hLayer ) );

    if( poInfo->nIDField != -1 )
        OGR_F_SetFieldInteger( hFeat, poInfo->nIDField, poInfo->nNextID++ );

    if( poInfo->nElevField != -1 )
        OGR_F_SetFieldDouble( hFeat, poInfo->nElevField, poContour->dfLevel );
    
    hGeom = OGR_G_CreateGeometry( wkbLineString );
    
    for( iPoint = poContour->nPoints-1; iPoint >= 0; iPoint-- )
    {
        OGR_G_SetPoint( hGeom, iPoint,
                        poInfo->adfGeoTransform[0] 
                        + poInfo->adfGeoTransform[1]*poContour->padfX[iPoint]
                        + poInfo->adfGeoTransform[2]*poContour->padfY[iPoint],
                        poInfo->adfGeoTransform[3] 
                        + poInfo->adfGeoTransform[4]*poContour->padfX[iPoint]
                        + poInfo->adfGeoTransform[5]*poContour->padfY[iPoint],
                        poContour->dfLevel );
    }

    OGR_F_SetGeometryDirectly( hFeat, hGeom );

    OGR_L_CreateFeature( poInfo->hLayer, hFeat );
    OGR_F_Destroy( hFeat );

    return CE_None;
}

/************************************************************************/
/*                        GDALContourGenerate()                         */
/************************************************************************/

CPLErr GDALContourGenerate( GDALRasterBandH hBand, 
                            double dfContourInterval, double dfContourBase,
                            int bUseNoData, double dfNoDataValue, 
                            OGRLayerH hLayer, int iIDField, int iElevField,
                            GDALProgressFunc pfnProgress, void *pProgressArg )

{
    OGRContourWriterInfo oCWI;

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

    if( !pfnProgress( 0.0, "", pProgressArg ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Setup contour writer information.                               */
/* -------------------------------------------------------------------- */
    GDALDatasetH hSrcDS;

    oCWI.hLayer = hLayer;

    oCWI.nElevField = iElevField;
    oCWI.nIDField = iIDField;

    hSrcDS = GDALGetBandDataset( hBand );
    GDALGetGeoTransform( hSrcDS, oCWI.adfGeoTransform );
    oCWI.nNextID = 0;

/* -------------------------------------------------------------------- */
/*      Setup contour generator.                                        */
/* -------------------------------------------------------------------- */
    int nXSize = GDALGetRasterBandXSize( hBand );
    int nYSize = GDALGetRasterBandYSize( hBand );

    GDALContourGenerator oCG( nXSize, nYSize, OGRContourWriter, &oCWI );

    oCG.SetContourLevels( dfContourInterval, dfContourBase );

    if( bUseNoData )
        oCG.SetNoData( dfNoDataValue );

/* -------------------------------------------------------------------- */
/*      Feed the data into the contour generator.                       */
/* -------------------------------------------------------------------- */
    int iLine;
    double *padfScanline;
    CPLErr eErr = CE_None;

    padfScanline = (double *) CPLMalloc(sizeof(double) * nXSize);

    for( iLine = 0; iLine < nYSize && eErr == CE_None; iLine++ )
    {
        GDALRasterIO( hBand, GF_Read, 0, iLine, nXSize, 1, 
                      padfScanline, nXSize, 1, GDT_Float64, 0, 0 );
        eErr = oCG.FeedLine( padfScanline );

        if( eErr == CE_None 
            && !pfnProgress( (iLine+1) / (double) nYSize, "", pProgressArg ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    }

    CPLFree( padfScanline );

    return eErr;
}

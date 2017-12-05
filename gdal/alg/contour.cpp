/******************************************************************************
 *
 * Project:  Contour Generation
 * Purpose:  Core algorithm implementation for contour line generation.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2003, Applied Coherent Technology Corporation, www.actgate.com
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_port.h"
#include "gdal_alg.h"

#include <cmath>
#include <cstring>

#include <algorithm>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "ogr_api.h"
#include "ogr_core.h"

CPL_CVSID("$Id$")

// The amount of a contour interval that pixels should be fudged by if they
// match a contour level exactly.

static const double FUDGE_EXACT = 0.001;

// The amount of a pixel that line ends need to be within to be considered to
// match for joining purposes.

static const double JOIN_DIST = 0.0001;

/************************************************************************/
/*                           GDALContourItem                            */
/************************************************************************/
class GDALContourItem
{
public:
    bool bRecentlyAccessed;
    double dfLevel;

    int nPoints;
    int nMaxPoints;
    double *padfX;
    double *padfY;

    bool bLeftIsHigh;

    double dfTailX;

    explicit GDALContourItem( double dfLevel );
    ~GDALContourItem();

    int    AddSegment( double dfXStart, double dfYStart,
                       double dfXEnd, double dfYEnd, int bLeftHigh );
    void   MakeRoomFor( int );
    int    Merge( GDALContourItem * );
    static double DistanceSqr( double x0, double y0, double x1, double y1 );
    static int MergeCase( double ax0, double ay0, double ax1, double ay1,
                          double bx0, double by0, double bx1, double by1);
    void   PrepareEjection();
};

/************************************************************************/
/*                           GDALContourLevel                           */
/************************************************************************/
class GDALContourLevel
{
    double dfLevel;

    int nEntryMax;
    int nEntryCount;
    GDALContourItem **papoEntries;

public:
    explicit GDALContourLevel( double );
    ~GDALContourLevel();

    double GetLevel() const { return dfLevel; }
    int    GetContourCount() const { return nEntryCount; }
    GDALContourItem *GetContour( int i) { return papoEntries[i]; }
    void   AdjustContour( int );
    void   RemoveContour( int );
    int    FindContour( double dfX, double dfY );
    int    InsertContour( GDALContourItem * );
};

/************************************************************************/
/*                         GDALContourGenerator                         */
/************************************************************************/
class GDALContourGenerator
{
    int    nWidth;
    int    nHeight;
    int    iLine;

    double *padfLastLine;
    double *padfThisLine;

    int    nLevelMax;
    int    nLevelCount;
    GDALContourLevel **papoLevels;

    bool   bNoDataActive;
    double dfNoDataValue;

    bool   bFixedLevels;
    double dfContourInterval;
    double dfContourOffset;

    CPLErr AddSegment( double dfLevel,
                       double dfXStart, double dfYStart,
                       double dfXEnd, double dfYEnd, int bLeftHigh );

    template<EMULATED_BOOL bNoDataIsNan> inline bool
        IsNoData( double dfVal ) const;

    template<EMULATED_BOOL bNoDataIsNan> CPLErr ProcessPixel( int iPixel );
    CPLErr ProcessRect( double, double, double,
                        double, double, double,
                        double, double, double,
                        double, double, double );

    static void Intersect( double, double, double,
                           double, double, double,
                           double, double, int *, double *, double * );

    GDALContourLevel *FindLevel( double dfLevel );

public:
    GDALContourWriter pfnWriter;
    void   *pWriterCBData;

    GDALContourGenerator( int nWidth, int nHeight,
                          GDALContourWriter pfnWriter, void *pWriterCBData );
    ~GDALContourGenerator();

    bool                Init();

    void                SetNoData( double dfNoDataValue );
    void                SetContourLevels( double dfContourIntervalIn,
                                          double dfContourOffsetIn = 0.0 )
        { dfContourInterval = dfContourIntervalIn;
          dfContourOffset = dfContourOffsetIn; }

    void                SetFixedLevels( int, double * );
    CPLErr              FeedLine( double *padfScanline );
    CPLErr              EjectContours( int bOnlyUnused = FALSE );
};

template<> inline bool GDALContourGenerator::IsNoData<true>(double dfVal) const
{
    return CPL_TO_BOOL(CPLIsNan(dfVal));
}

template<> inline bool GDALContourGenerator::IsNoData<false>(double dfVal) const
{
    return dfVal == dfNoDataValue;
}

/************************************************************************/
/*                           GDAL_CG_Create()                           */
/************************************************************************/

/** Create contour generator */
GDALContourGeneratorH
GDAL_CG_Create( int nWidth, int nHeight, int bNoDataSet, double dfNoDataValue,
                double dfContourInterval, double dfContourBase,
                GDALContourWriter pfnWriter, void *pCBData )

{
    GDALContourGenerator *poCG = new GDALContourGenerator( nWidth, nHeight,
                                                           pfnWriter, pCBData );
    if( !poCG->Init() )
    {
        delete poCG;
        return NULL;
    }
    if( bNoDataSet )
        poCG->SetNoData( dfNoDataValue );

    poCG->SetContourLevels( dfContourInterval, dfContourBase );
    return (GDALContourGeneratorH) poCG;
}

/************************************************************************/
/*                          GDAL_CG_FeedLine()                          */
/************************************************************************/

/** Feed a line to the contour generator */
CPLErr GDAL_CG_FeedLine( GDALContourGeneratorH hCG, double *padfScanline )

{
    VALIDATE_POINTER1( hCG, "GDAL_CG_FeedLine", CE_Failure );

    return static_cast<GDALContourGenerator *>(hCG)->FeedLine(padfScanline);
}

/************************************************************************/
/*                          GDAL_CG_Destroy()                           */
/************************************************************************/

/** Destroy contour generator */
void GDAL_CG_Destroy( GDALContourGeneratorH hCG )

{
    delete static_cast<GDALContourGenerator *>(hCG);
}

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
                                            void *pWriterCBDataIn ) :
    nWidth(nWidthIn),
    nHeight(nHeightIn),
    iLine(-1),
    padfLastLine(NULL),
    padfThisLine(NULL),
    nLevelMax(0),
    nLevelCount(0),
    papoLevels(NULL),
    bNoDataActive(false),
    dfNoDataValue(-1000000.0),
    bFixedLevels(false),
    dfContourInterval(10.0),
    dfContourOffset(0.0),
    pfnWriter(pfnWriterIn),
    pWriterCBData(pWriterCBDataIn)
{}

/************************************************************************/
/*                       ~GDALContourGenerator()                        */
/************************************************************************/

GDALContourGenerator::~GDALContourGenerator()

{
    for( int i = 0; i < nLevelCount; i++ )
        delete papoLevels[i];
    CPLFree( papoLevels );

    CPLFree( padfLastLine );
    CPLFree( padfThisLine );
}

/************************************************************************/
/*                              Init()                                  */
/************************************************************************/

bool GDALContourGenerator::Init()
{
    padfLastLine =
        static_cast<double *>(VSI_CALLOC_VERBOSE(sizeof(double), nWidth));
    padfThisLine =
        static_cast<double *>(VSI_CALLOC_VERBOSE(sizeof(double), nWidth));
    return padfLastLine != NULL && padfThisLine != NULL;
}

/************************************************************************/
/*                           SetFixedLevels()                           */
/************************************************************************/

void GDALContourGenerator::SetFixedLevels( int nFixedLevelCount,
                                           double *padfFixedLevels )

{
    bFixedLevels = true;
    for( int i = 0; i < nFixedLevelCount; i++ )
        FindLevel( padfFixedLevels[i] );
}

/************************************************************************/
/*                             SetNoData()                              */
/************************************************************************/

void GDALContourGenerator::SetNoData( double dfNewValue )

{
    bNoDataActive = true;
    dfNoDataValue = dfNewValue;
}

/************************************************************************/
/*                            ProcessPixel()                            */
/************************************************************************/

template<EMULATED_BOOL bNoDataIsNan> CPLErr
GDALContourGenerator::ProcessPixel( int iPixel )

{
    bool bSubdivide = false;

/* -------------------------------------------------------------------- */
/*      Collect the four corner pixel values.  Value left or right      */
/*      of the scanline are taken from the nearest pixel on the         */
/*      scanline itself.                                                */
/* -------------------------------------------------------------------- */
    const double dfUpLeft = padfLastLine[std::max(0, iPixel-1)];
    const double dfUpRight = padfLastLine[std::min(nWidth - 1, iPixel)];

    const double dfLoLeft = padfThisLine[std::max(0, iPixel - 1)];
    const double dfLoRight = padfThisLine[std::min(nWidth - 1, iPixel)];

/* -------------------------------------------------------------------- */
/*      Check if we have any nodata values.                             */
/* -------------------------------------------------------------------- */
    if( bNoDataActive
        && (IsNoData<bNoDataIsNan>(dfUpLeft) ||
            IsNoData<bNoDataIsNan>(dfLoLeft) ||
            IsNoData<bNoDataIsNan>(dfLoRight) ||
            IsNoData<bNoDataIsNan>(dfUpRight)) )
    {
        bSubdivide = true;
    }

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

    if( !IsNoData<bNoDataIsNan>(dfUpLeft) )
    {
        dfASum += dfUpLeft;
        nGoodCount++;
    }

    if( !IsNoData<bNoDataIsNan>(dfLoLeft) )
    {
        dfASum += dfLoLeft;
        nGoodCount++;
    }

    if( !IsNoData<bNoDataIsNan>(dfLoRight) )
    {
        dfASum += dfLoRight;
        nGoodCount++;
    }

    if( !IsNoData<bNoDataIsNan>(dfUpRight) )
    {
        dfASum += dfUpRight;
        nGoodCount++;
    }

    if( nGoodCount == 0 )
        return CE_None;

    const double dfCenter = dfASum / nGoodCount;
    double dfTop = 0.0;
    double dfLeft = 0.0;

    if( !IsNoData<bNoDataIsNan>(dfUpLeft) )
    {
        if( !IsNoData<bNoDataIsNan>(dfUpRight) )
            dfTop = (dfUpLeft + dfUpRight) / 2.0;
        else
            dfTop = dfUpLeft;

        if( !IsNoData<bNoDataIsNan>(dfLoLeft) )
            dfLeft = (dfUpLeft + dfLoLeft) / 2.0;
        else
            dfLeft = dfUpLeft;
    }
    else
    {
        dfTop = dfUpRight;
        dfLeft = dfLoLeft;
    }

    double dfRight = 0.0;
    double dfBottom = 0.0;

    if( !IsNoData<bNoDataIsNan>(dfLoRight) )
    {
        if( !IsNoData<bNoDataIsNan>(dfUpRight) )
            dfRight = (dfLoRight + dfUpRight) / 2.0;
        else
            dfRight = dfLoRight;

        if( !IsNoData<bNoDataIsNan>(dfLoLeft) )
            dfBottom = (dfLoRight + dfLoLeft) / 2.0;
        else
            dfBottom = dfLoRight;
    }
    else
    {
        dfBottom = dfLoLeft;
        dfRight = dfUpRight;
    }

/* -------------------------------------------------------------------- */
/*      Process any quadrants that aren't "nodata" anchored.            */
/* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;

    if( !IsNoData<bNoDataIsNan>(dfUpLeft) && iPixel > 0 && iLine > 0 )
    {
        eErr = ProcessRect( dfUpLeft, iPixel - 0.5, iLine - 0.5,
                            dfLeft, iPixel - 0.5, iLine,
                            dfCenter, iPixel, iLine,
                            dfTop, iPixel, iLine - 0.5 );
    }

    if( !IsNoData<bNoDataIsNan>(dfLoLeft) && eErr == CE_None
        && iPixel > 0 && iLine < nHeight )
    {
        eErr = ProcessRect( dfLeft, iPixel - 0.5, iLine,
                            dfLoLeft, iPixel - 0.5, iLine + 0.5,
                            dfBottom, iPixel, iLine + 0.5,
                            dfCenter, iPixel, iLine );
    }

    if( !IsNoData<bNoDataIsNan>(dfLoRight) &&
        iPixel < nWidth && iLine < nHeight )
    {
        eErr = ProcessRect( dfCenter, iPixel, iLine,
                            dfBottom, iPixel, iLine + 0.5,
                            dfLoRight, iPixel + 0.5, iLine + 0.5,
                            dfRight, iPixel + 0.5, iLine );
    }

    if( !IsNoData<bNoDataIsNan>(dfUpRight) && iPixel < nWidth && iLine > 0 )
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
/*      Identify the range of elevations over this rect.                */
/* -------------------------------------------------------------------- */
    int iStartLevel = 0;
    int iEndLevel = 0;

    const double dfMin =
        std::min(std::min(dfUpLeft, dfUpRight), std::min(dfLoLeft, dfLoRight));
    const double dfMax =
        std::max(std::max(dfUpLeft, dfUpRight), std::max(dfLoLeft, dfLoRight));

/* -------------------------------------------------------------------- */
/*      Compute the set of levels to compute contours for.              */
/* -------------------------------------------------------------------- */

    // If we are using fixed levels, then find the min/max in the levels table.
    if( bFixedLevels )
    {
        int nStart = 0;
        int nEnd = nLevelCount-1;

        iStartLevel = -1;
        while( nStart <= nEnd )
        {
            const int nMiddle = (nEnd + nStart) / 2;

            const double dfMiddleLevel = papoLevels[nMiddle]->GetLevel();

            if( dfMiddleLevel < dfMin )
                nStart = nMiddle + 1;
            else if( dfMiddleLevel > dfMin )
                nEnd = nMiddle - 1;
            else
            {
                iStartLevel = nMiddle;
                break;
            }
        }

        if( iStartLevel == -1 )
            iStartLevel = nEnd + 1;

        iEndLevel = iStartLevel;
        while( iEndLevel < nLevelCount-1
               && papoLevels[iEndLevel+1]->GetLevel() < dfMax )
            iEndLevel++;

        if( iStartLevel >= nLevelCount )
            return CE_None;

        CPLAssert( iStartLevel >= 0 && iStartLevel < nLevelCount );
        CPLAssert( iEndLevel >= 0 && iEndLevel < nLevelCount );
    }
    // Otherwise figure out the start and end using the base and offset.
    else
    {
        iStartLevel = static_cast<int>(
            ceil((dfMin - dfContourOffset) / dfContourInterval));
        iEndLevel = static_cast<int>(
            floor((dfMax - dfContourOffset) / dfContourInterval));
    }

    if( iStartLevel > iEndLevel )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Loop over them.                                                 */
/* -------------------------------------------------------------------- */
    for( int iLevel = iStartLevel; iLevel <= iEndLevel; iLevel++ )
    {
        const double dfLevel =
            bFixedLevels
            ? papoLevels[iLevel]->GetLevel()
            : iLevel * dfContourInterval + dfContourOffset;

        int nPoints = 0;
        double adfX[4] = {};
        double adfY[4] = {};

        // Logs how many points we have af left + bottom,
        // and left + bottom + right.
        Intersect( dfUpLeft, dfUpLeftX, dfUpLeftY,
                   dfLoLeft, dfLoLeftX, dfLoLeftY,
                   dfLoRight, dfLevel, &nPoints, adfX, adfY );
        const int nPoints1 = nPoints;
        Intersect( dfLoLeft, dfLoLeftX, dfLoLeftY,
                   dfLoRight, dfLoRightX, dfLoRightY,
                   dfUpRight, dfLevel, &nPoints, adfX, adfY );
        const int nPoints2 = nPoints;
        Intersect( dfLoRight, dfLoRightX, dfLoRightY,
                   dfUpRight, dfUpRightX, dfUpRightY,
                   dfUpLeft, dfLevel, &nPoints, adfX, adfY );
        const int nPoints3 = nPoints;
        Intersect( dfUpRight, dfUpRightX, dfUpRightY,
                   dfUpLeft, dfUpLeftX, dfUpLeftY,
                   dfLoLeft, dfLevel, &nPoints, adfX, adfY );

        if( nPoints == 1 || nPoints == 3 )
            CPLDebug( "CONTOUR", "Got nPoints = %d", nPoints );

        CPLErr eErr = CE_None;

        if( nPoints >= 2 )
        {
            if( nPoints1 == 1 && nPoints2 == 2 ) // left + bottom
            {
                eErr = AddSegment( dfLevel,
                                   adfX[0], adfY[0], adfX[1], adfY[1],
                                   dfUpRight > dfLoLeft );
            }
            else if( nPoints1 == 1 && nPoints3 == 2 ) // left + right
            {
                eErr = AddSegment( dfLevel,
                                   adfX[0], adfY[0], adfX[1], adfY[1],
                                   dfUpLeft > dfLoRight );
            }
            else if( nPoints1 == 1 && nPoints == 2 ) // left + top
            {
                // Do not do vertical contours on the left, due to symmetry.
                if( !(dfUpLeft == dfLevel && dfLoLeft == dfLevel) )
                    eErr = AddSegment( dfLevel,
                                       adfX[0], adfY[0], adfX[1], adfY[1],
                                       dfUpLeft > dfLoRight );
            }
            else if( nPoints2 == 1 && nPoints3 == 2 ) // bottom + right
            {
                eErr = AddSegment( dfLevel,
                                   adfX[0], adfY[0], adfX[1], adfY[1],
                                   dfUpLeft > dfLoRight );
            }
            else if( nPoints2 == 1 && nPoints == 2 ) // bottom + top
            {
                eErr = AddSegment( dfLevel,
                                   adfX[0], adfY[0], adfX[1], adfY[1],
                                   dfLoLeft > dfUpRight );
            }
            else if( nPoints3 == 1 && nPoints == 2 ) // right + top
            {
                // Do not do horizontal contours on upside, due to symmetry.
                if( !(dfUpRight == dfLevel && dfUpLeft == dfLevel) )
                    eErr = AddSegment( dfLevel,
                                       adfX[0], adfY[0], adfX[1], adfY[1],
                                       dfLoLeft > dfUpRight );
            }
            else
            {
                // If we get here it is a serious error!
                CPLDebug( "CONTOUR", "Contour state not implemented!");
            }

            if( eErr != CE_None )
                 return eErr;
        }

        if( nPoints == 4 )
        {
          // Do not do horizontal contours on upside, due to symmetry.
          if( !(dfUpRight == dfLevel && dfUpLeft == dfLevel) )
          {
/* -------------------------------------------------------------------- */
/*          If we get here, we know the first was left+bottom,          */
/*          so we are at right+top, therefore "left is high"            */
/*          if low-left is larger than up-right.                        */
/*          We do not do a diagonal check here as we are dealing with   */
/*          a saddle point.                                             */
/* -------------------------------------------------------------------- */
            eErr = AddSegment( dfLevel,
                               adfX[2], adfY[2], adfX[3], adfY[3],
                               ( dfLoRight > dfUpRight) );
            if( eErr != CE_None )
                return eErr;
          }
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
        const double dfRatio = (dfLevel - dfVal1) / (dfVal2 - dfVal1);

        padfX[*pnPoints] = dfX1 * (1.0 - dfRatio) + dfX2 * dfRatio;
        padfY[*pnPoints] = dfY1 * (1.0 - dfRatio) + dfY2 * dfRatio;
        (*pnPoints)++;
    }
    else if( dfVal1 > dfLevel && dfVal2 <= dfLevel )
    {
        const double dfRatio = (dfLevel - dfVal2) / (dfVal1 - dfVal2);

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
                                         double dfX2, double dfY2,
                                         int bLeftHigh)

{
    GDALContourLevel *poLevel = FindLevel( dfLevel );
    GDALContourItem *poTarget = NULL;

/* -------------------------------------------------------------------- */
/*      Check all active contours for any that this might attach        */
/*      to. Eventually this should be recoded to find the contours      */
/*      of the correct level more efficiently.                          */
/* -------------------------------------------------------------------- */

    const int iTarget =
        dfY1 < dfY2
        ? poLevel->FindContour(dfX1, dfY1)
        : poLevel->FindContour(dfX2, dfY2);

    if( iTarget != -1 )
    {
        poTarget = poLevel->GetContour( iTarget );

        poTarget->AddSegment( dfX1, dfY1, dfX2, dfY2, bLeftHigh );

        poLevel->AdjustContour( iTarget );

        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      No existing contour found, lets create a new one.               */
/* -------------------------------------------------------------------- */
    poTarget = new GDALContourItem( dfLevel );

    poTarget->AddSegment( dfX1, dfY1, dfX2, dfY2, bLeftHigh );

    poLevel->InsertContour( poTarget );

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
    int iPixel = 0;  // Used after for.

    for( ; iPixel < nWidth; iPixel++ )
    {
        if( bNoDataActive && padfThisLine[iPixel] == dfNoDataValue )
            continue;

        const double dfLevel =
            (padfThisLine[iPixel] - dfContourOffset) / dfContourInterval;

        if( dfLevel - static_cast<int>(dfLevel) == 0.0 )
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
    for( int iLevel = 0; iLevel < nLevelCount; iLevel++ )
    {
        GDALContourLevel *poLevel = papoLevels[iLevel];

        for( int iContour = 0;
             iContour < poLevel->GetContourCount();
             iContour++ )
            poLevel->GetContour( iContour )->bRecentlyAccessed = false;
    }

/* -------------------------------------------------------------------- */
/*      Process each pixel.                                             */
/* -------------------------------------------------------------------- */
    const bool bNoDataIsNan = CPL_TO_BOOL(CPLIsNan(dfNoDataValue));
    for( iPixel = 0; iPixel < nWidth + 1; iPixel++ )
    {
        const CPLErr eErr = bNoDataIsNan ? ProcessPixel<true>( iPixel ) :
                                           ProcessPixel<false>( iPixel );
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

    return eErr;
}

/************************************************************************/
/*                           EjectContours()                            */
/************************************************************************/

CPLErr GDALContourGenerator::EjectContours( int bOnlyUnused )

{
    CPLErr eErr = CE_None;

/* -------------------------------------------------------------------- */
/*      Process all contours of all levels that match our criteria      */
/* -------------------------------------------------------------------- */
    for( int iLevel = 0; iLevel < nLevelCount && eErr == CE_None; iLevel++ )
    {
        GDALContourLevel *poLevel = papoLevels[iLevel];

        for( int iContour = 0;
             iContour < poLevel->GetContourCount() && eErr == CE_None;
             /* increment in loop if we don't consume it. */ )
        {
            GDALContourItem *poTarget = poLevel->GetContour( iContour );

            if( bOnlyUnused && poTarget->bRecentlyAccessed )
            {
                iContour++;
                continue;
            }

            poLevel->RemoveContour( iContour );

            // Try to find another contour we can merge with in this level.

            int iC2 = 0;  // Used after for.
            for( ; iC2 < poLevel->GetContourCount(); iC2++ )
            {
                GDALContourItem *poOther = poLevel->GetContour( iC2 );

                if( poOther->Merge( poTarget ) )
                    break;
            }

            // If we didn't merge it, then eject (write) it out.
            if( iC2 == poLevel->GetContourCount() )
            {
                if( pfnWriter != NULL )
                {
                    // If direction is wrong, then reverse before ejecting.
                    poTarget->PrepareEjection();

                    eErr = pfnWriter( poTarget->dfLevel, poTarget->nPoints,
                                      poTarget->padfX, poTarget->padfY,
                                      pWriterCBData );
                }
            }

            delete poTarget;
        }
    }

    return eErr;
}

/************************************************************************/
/*                             FindLevel()                              */
/************************************************************************/

GDALContourLevel *GDALContourGenerator::FindLevel( double dfLevel )

{
    int nStart = 0;
    int nEnd = nLevelCount - 1;

/* -------------------------------------------------------------------- */
/*      Binary search to find the requested level.                      */
/* -------------------------------------------------------------------- */
    while( nStart <= nEnd )
    {
        const int nMiddle = (nEnd + nStart) / 2;

        const double dfMiddleLevel = papoLevels[nMiddle]->GetLevel();

        if( dfMiddleLevel < dfLevel )
            nStart = nMiddle + 1;
        else if( dfMiddleLevel > dfLevel )
            nEnd = nMiddle - 1;
        else
            return papoLevels[nMiddle];
    }

/* -------------------------------------------------------------------- */
/*      Didn't find the level, create a new one and insert it in        */
/*      order.                                                          */
/* -------------------------------------------------------------------- */
    GDALContourLevel *poLevel = new GDALContourLevel( dfLevel );

    if( nLevelMax == nLevelCount )
    {
        nLevelMax = nLevelMax * 2 + 10;
        papoLevels = static_cast<GDALContourLevel **>(
            CPLRealloc(papoLevels, sizeof(void*) * nLevelMax));
    }

    if( nLevelCount - nEnd - 1 > 0 )
        memmove( papoLevels + nEnd + 2, papoLevels + nEnd + 1,
                 (nLevelCount - nEnd - 1) * sizeof(void*) );
    papoLevels[nEnd+1] = poLevel;
    nLevelCount++;

    return poLevel;
}

/************************************************************************/
/* ==================================================================== */
/*                           GDALContourLevel                           */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                          GDALContourLevel()                          */
/************************************************************************/

GDALContourLevel::GDALContourLevel( double dfLevelIn ) :
    dfLevel(dfLevelIn),
    nEntryMax(0),
    nEntryCount(0),
    papoEntries(NULL)
{}

/************************************************************************/
/*                         ~GDALContourLevel()                          */
/************************************************************************/

GDALContourLevel::~GDALContourLevel()

{
    CPLAssert( nEntryCount == 0 );
    CPLFree( papoEntries );
}

/************************************************************************/
/*                           AdjustContour()                            */
/*                                                                      */
/*      Assume the indicated contour's tail may have changed, and       */
/*      adjust it up or down in the list of contours to re-establish    */
/*      proper ordering.                                                */
/************************************************************************/

void GDALContourLevel::AdjustContour( int iChanged )

{
    while( iChanged > 0
         && papoEntries[iChanged]->dfTailX < papoEntries[iChanged-1]->dfTailX )
    {
        GDALContourItem *poTemp = papoEntries[iChanged];
        papoEntries[iChanged] = papoEntries[iChanged-1];
        papoEntries[iChanged-1] = poTemp;
        iChanged--;
    }

    while( iChanged < nEntryCount-1
         && papoEntries[iChanged]->dfTailX > papoEntries[iChanged+1]->dfTailX )
    {
        GDALContourItem *poTemp = papoEntries[iChanged];
        papoEntries[iChanged] = papoEntries[iChanged+1];
        papoEntries[iChanged+1] = poTemp;
        iChanged++;
    }
}

/************************************************************************/
/*                           RemoveContour()                            */
/************************************************************************/

void GDALContourLevel::RemoveContour( int iTarget )

{
    if( iTarget < nEntryCount )
        memmove( papoEntries + iTarget, papoEntries + iTarget + 1,
                 (nEntryCount - iTarget - 1) * sizeof(void*) );
    nEntryCount--;
}

/************************************************************************/
/*                            FindContour()                             */
/*                                                                      */
/*      Perform a binary search to find the requested "tail"            */
/*      location.  If not available return -1.  In theory there can     */
/*      be more than one contour with the same tail X and different     */
/*      Y tails ... ensure we check against them all.                   */
/************************************************************************/

int GDALContourLevel::FindContour( double dfX, double dfY )

{
    int nStart = 0;
    int nEnd = nEntryCount - 1;

    while( nEnd >= nStart )
    {
        int nMiddle = (nEnd + nStart) / 2;

        const double dfMiddleX = papoEntries[nMiddle]->dfTailX;

        if( dfMiddleX < dfX )
            nStart = nMiddle + 1;
        else if( dfMiddleX > dfX )
            nEnd = nMiddle - 1;
        else
        {
            while( nMiddle > 0
                   && fabs(papoEntries[nMiddle]->dfTailX-dfX) < JOIN_DIST )
                nMiddle--;

            while( nMiddle < nEntryCount
                   && fabs(papoEntries[nMiddle]->dfTailX-dfX) < JOIN_DIST )
            {
                if( fabs(papoEntries[nMiddle]->
                         padfY[papoEntries[nMiddle]->nPoints-1] - dfY) <
                    JOIN_DIST )
                    return nMiddle;
                nMiddle++;
            }

            return -1;
        }
    }

    return -1;
}

/************************************************************************/
/*                           InsertContour()                            */
/*                                                                      */
/*      Ensure the newly added contour is placed in order according     */
/*      to the X value relative to the other contours.                  */
/************************************************************************/

int GDALContourLevel::InsertContour( GDALContourItem *poNewContour )

{
/* -------------------------------------------------------------------- */
/*      Find where to insert by binary search.                          */
/* -------------------------------------------------------------------- */
    int nStart = 0;
    int nEnd = nEntryCount - 1;

    while( nEnd >= nStart )
    {
        const int nMiddle = (nEnd + nStart) / 2;

        const double dfMiddleX = papoEntries[nMiddle]->dfTailX;

        if( dfMiddleX < poNewContour->dfLevel )
            nStart = nMiddle + 1;
        else if( dfMiddleX > poNewContour->dfLevel )
            nEnd = nMiddle - 1;
        else
        {
            nEnd = nMiddle - 1;
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we need to grow the array?                                   */
/* -------------------------------------------------------------------- */
    if( nEntryMax == nEntryCount )
    {
        nEntryMax = nEntryMax * 2 + 10;
        papoEntries = static_cast<GDALContourItem **>(
            CPLRealloc(papoEntries, sizeof(void*) * nEntryMax));
    }

/* -------------------------------------------------------------------- */
/*      Insert the new contour at the appropriate location.             */
/* -------------------------------------------------------------------- */
    if( nEntryCount - nEnd - 1 > 0 )
        memmove( papoEntries + nEnd + 2, papoEntries + nEnd + 1,
                 (nEntryCount - nEnd - 1) * sizeof(void*) );
    papoEntries[nEnd+1] = poNewContour;
    nEntryCount++;

    return nEnd+1;
}

/************************************************************************/
/* ==================================================================== */
/*                           GDALContourItem                            */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                          GDALContourItem()                           */
/************************************************************************/

GDALContourItem::GDALContourItem( double dfLevelIn ) :
    bRecentlyAccessed(false),
    dfLevel(dfLevelIn),
    nPoints(0),
    nMaxPoints(0),
    padfX(NULL),
    padfY(NULL),
    bLeftIsHigh(false),
    dfTailX(0.0)
{}

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
                                 double dfXEnd, double dfYEnd,
                                 int bLeftHigh)

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
        bRecentlyAccessed = true;

        dfTailX = padfX[1];

        // Here we know that the left of this vector is the high side.
        bLeftIsHigh = CPL_TO_BOOL(bLeftHigh);

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

        bRecentlyAccessed = true;

        dfTailX = dfXEnd;

        return TRUE;
    }
    else if( fabs(padfX[nPoints-1]-dfXEnd) < JOIN_DIST
             && fabs(padfY[nPoints-1]-dfYEnd) < JOIN_DIST )
    {
        padfX[nPoints] = dfXStart;
        padfY[nPoints] = dfYStart;
        nPoints++;

        bRecentlyAccessed = true;

        dfTailX = dfXStart;

        return TRUE;
    }

    return FALSE;
}

/************************************************************************/
/*                               DistanceSqr()                          */
/************************************************************************/

double GDALContourItem::DistanceSqr(
   double x0, double y0, double x1, double y1 )
{
// --------------------------------------------------------------------
// Coumpute the square of the euclidian distance between
// (x0;y0)-(x1;y1)
// --------------------------------------------------------------------
   const double dx = x0 - x1;
   const double dy = y0 - y1;

   return dx * dx + dy * dy;
}

/************************************************************************/
/*                               MergeCase()                            */
/************************************************************************/

int GDALContourItem::MergeCase(
   double ax0, double ay0, double ax1, double ay1,
   double bx0, double by0, double bx1, double by1
)
{
// --------------------------------------------------------------------
// Try to find a match case between line ends.
// Calculate all possible distances and choose the closest
// if less than JOIN_DIST.
// --------------------------------------------------------------------

    // Avoid sqrt().
    const double jds = JOIN_DIST * JOIN_DIST;

    // Case 1 e-b.
    int cs = 1;
    double dmin = DistanceSqr( ax1, ay1, bx0, by0 );

    // Case 2 b-e.
    double dd = DistanceSqr( ax0, ay0, bx1, by1 );
    if( dd < dmin )
    {
        dmin = dd;
        cs   = 2;
    }

    // Case 3 e-e.
    dd = DistanceSqr( ax1, ay1, bx1, by1 );
    if( dd < dmin )
    {
        dmin = dd;
        cs   = 3;
    }

    // Case 4 b-b.
    dd = DistanceSqr (ax0, ay0, bx0, by0);
    if( dd < dmin )
    {
        dmin = dd;
        cs   = 4;
    }

    if( dmin > jds )
        cs = 0;

    return cs;
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

    const int mc = MergeCase(
        padfX[0],                           padfY[0],
        padfX[nPoints-1],                   padfY[nPoints-1],
        poOther->padfX[0],                  poOther->padfY[0],
        poOther->padfX[poOther->nPoints-1], poOther->padfY[poOther->nPoints-1]
    );

    bool rc = false;
    switch( mc )
    {
        case 0:
            break;

        case 1:  // Case 1 e-b.
            MakeRoomFor( nPoints + poOther->nPoints - 1 );

            memcpy( padfX + nPoints, poOther->padfX + 1,
                    sizeof(double) * (poOther->nPoints-1) );
            memcpy( padfY + nPoints, poOther->padfY + 1,
                    sizeof(double) * (poOther->nPoints-1) );
            nPoints += poOther->nPoints - 1;

            bRecentlyAccessed = true;

            dfTailX = padfX[nPoints-1];

            rc = true;
            break;

        case 2:  // Case 2 b-e.
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

            bRecentlyAccessed = true;

            dfTailX = padfX[nPoints-1];

            rc = true;
            break;

        case 3:  // Case 3 e-e
            MakeRoomFor( nPoints + poOther->nPoints - 1 );

            for( int i = 0; i < poOther->nPoints-1; i++ )
            {
                padfX[i+nPoints] = poOther->padfX[poOther->nPoints-i-2];
                padfY[i+nPoints] = poOther->padfY[poOther->nPoints-i-2];
            }

            nPoints += poOther->nPoints - 1;

            bRecentlyAccessed = true;

            dfTailX = padfX[nPoints-1];

            rc = true;
            break;

        case 4:  // Case 3 b-b.
            MakeRoomFor( nPoints + poOther->nPoints - 1 );

            memmove( padfX + poOther->nPoints - 1, padfX,
                    sizeof(double) * nPoints );
            memmove( padfY + poOther->nPoints - 1, padfY,
                    sizeof(double) * nPoints );

            for( int i = 0; i < poOther->nPoints-1; i++ )
            {
                padfX[i] = poOther->padfX[poOther->nPoints - i - 1];
                padfY[i] = poOther->padfY[poOther->nPoints - i - 1];
            }

            nPoints += poOther->nPoints - 1;

            bRecentlyAccessed = true;

            dfTailX = padfX[nPoints-1];

            rc = true;
            break;

        default:
            CPLAssert(false);
            break;
    }

    return rc;
}

/************************************************************************/
/*                            MakeRoomFor()                             */
/************************************************************************/

void GDALContourItem::MakeRoomFor( int nNewPoints )

{
    if( nNewPoints > nMaxPoints )
    {
        nMaxPoints = nNewPoints * 2 + 50;
        padfX = static_cast<double *>(
            CPLRealloc(padfX, sizeof(double) * nMaxPoints));
        padfY = static_cast<double *>(
            CPLRealloc(padfY, sizeof(double) * nMaxPoints));
    }
}

/************************************************************************/
/*                          PrepareEjection()                           */
/************************************************************************/

void GDALContourItem::PrepareEjection()

{
    // If left side is the high side, then reverse to get curve normal
    // pointing downwards.
    if( bLeftIsHigh )
    {
        std::reverse(padfX, padfX + nPoints);
        std::reverse(padfY, padfY + nPoints);
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

CPLErr OGRContourWriter( double dfLevel,
                         int nPoints, double *padfX, double *padfY,
                         void *pInfo )

{
    OGRContourWriterInfo *poInfo = static_cast<OGRContourWriterInfo *>(pInfo);

    OGRFeatureH hFeat =
        OGR_F_Create(
            OGR_L_GetLayerDefn( static_cast<OGRLayerH>(poInfo->hLayer) ));

    if( poInfo->nIDField != -1 )
        OGR_F_SetFieldInteger( hFeat, poInfo->nIDField, poInfo->nNextID++ );

    if( poInfo->nElevField != -1 )
        OGR_F_SetFieldDouble( hFeat, poInfo->nElevField, dfLevel );

    OGRGeometryH hGeom = OGR_G_CreateGeometry( wkbLineString );

    for( int iPoint = nPoints - 1; iPoint >= 0; iPoint-- )
    {
        OGR_G_SetPoint( hGeom, iPoint,
                        poInfo->adfGeoTransform[0]
                        + poInfo->adfGeoTransform[1] * padfX[iPoint]
                        + poInfo->adfGeoTransform[2] * padfY[iPoint],
                        poInfo->adfGeoTransform[3]
                        + poInfo->adfGeoTransform[4] * padfX[iPoint]
                        + poInfo->adfGeoTransform[5] * padfY[iPoint],
                        dfLevel );
    }

    OGR_F_SetGeometryDirectly( hFeat, hGeom );

    const OGRErr eErr =
        OGR_L_CreateFeature(static_cast<OGRLayerH>(poInfo->hLayer), hFeat);
    OGR_F_Destroy( hFeat );

    return eErr == OGRERR_NONE ? CE_None : CE_Failure;
}

/************************************************************************/
/*                        GDALContourGenerate()                         */
/************************************************************************/

/**
 * Create vector contours from raster DEM.
 *
 * This algorithm will generate contour vectors for the input raster band
 * on the requested set of contour levels.  The vector contours are written
 * to the passed in OGR vector layer.  Also, a NODATA value may be specified
 * to identify pixels that should not be considered in contour line generation.
 *
 * The gdal/apps/gdal_contour.cpp mainline can be used as an example of
 * how to use this function.
 *
 * ALGORITHM RULES

For contouring purposes raster pixel values are assumed to represent a point
value at the center of the corresponding pixel region.  For the purpose of
contour generation we virtually connect each pixel center to the values to
the left, right, top and bottom.  We assume that the pixel value is linearly
interpolated between the pixel centers along each line, and determine where
(if any) contour lines will appear along these line segments.  Then the
contour crossings are connected.

This means that contour lines' nodes will not actually be on pixel edges, but
rather along vertical and horizontal lines connecting the pixel centers.

\verbatim
General Case:

      5 |                  | 3
     -- + ---------------- + --
        |                  |
        |                  |
        |                  |
        |                  |
     10 +                  |
        |\                 |
        | \                |
     -- + -+-------------- + --
     12 |  10              | 1

Saddle Point:

      5 |                  | 12
     -- + -------------+-- + --
        |               \  |
        |                 \|
        |                  +
        |                  |
        +                  |
        |\                 |
        | \                |
     -- + -+-------------- + --
     12 |                  | 1

or:

      5 |                  | 12
     -- + -------------+-- + --
        |          __/     |
        |      ___/        |
        |  ___/          __+
        | /           __/  |
        +'         __/     |
        |       __/        |
        |   ,__/           |
     -- + -+-------------- + --
     12 |                  | 1
\endverbatim

Nodata:

In the "nodata" case we treat the whole nodata pixel as a no-mans land.
We extend the corner pixels near the nodata out to half way and then
construct extra lines from those points to the center which is assigned
an averaged value from the two nearby points (in this case (12+3+5)/3).

\verbatim
      5 |                  | 3
     -- + ---------------- + --
        |                  |
        |                  |
        |      6.7         |
        |        +---------+ 3
     10 +___     |
        |   \____+ 10
        |        |
     -- + -------+        +
     12 |       12           (nodata)

\endverbatim

 *
 * @param hBand The band to read raster data from.  The whole band will be
 * processed.
 *
 * @param dfContourInterval The elevation interval between contours generated.
 *
 * @param dfContourBase The "base" relative to which contour intervals are
 * applied.  This is normally zero, but could be different.  To generate 10m
 * contours at 5, 15, 25, ... the ContourBase would be 5.
 *
 * @param  nFixedLevelCount The number of fixed levels. If this is greater than
 * zero, then fixed levels will be used, and ContourInterval and ContourBase
 * are ignored.
 *
 * @param padfFixedLevels The list of fixed contour levels at which contours
 * should be generated.  It will contain FixedLevelCount entries, and may be
 * NULL if fixed levels are disabled (FixedLevelCount = 0).
 *
 * @param bUseNoData If TRUE the dfNoDataValue will be used.
 *
 * @param dfNoDataValue The value to use as a "nodata" value. That is, a
 * pixel value which should be ignored in generating contours as if the value
 * of the pixel were not known.
 *
 * @param hLayer The layer to which new contour vectors will be written.
 * Each contour will have a LINESTRING geometry attached to it.   This
 * is really of type OGRLayerH, but void * is used to avoid pulling the
 * ogr_api.h file in here.
 *
 * @param iIDField If not -1 this will be used as a field index to indicate
 * where a unique id should be written for each feature (contour) written.
 *
 * @param iElevField If not -1 this will be used as a field index to indicate
 * where the elevation value of the contour should be written.
 *
 * @param pfnProgress A GDALProgressFunc that may be used to report progress
 * to the user, or to interrupt the algorithm.  May be NULL if not required.
 *
 * @param pProgressArg The callback data for the pfnProgress function.
 *
 * @return CE_None on success or CE_Failure if an error occurs.
 */

CPLErr GDALContourGenerate( GDALRasterBandH hBand,
                            double dfContourInterval, double dfContourBase,
                            int nFixedLevelCount, double *padfFixedLevels,
                            int bUseNoData, double dfNoDataValue,
                            void *hLayer, int iIDField, int iElevField,
                            GDALProgressFunc pfnProgress, void *pProgressArg )

{
    VALIDATE_POINTER1( hBand, "GDALContourGenerate", CE_Failure );

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
    oCWI.hLayer = static_cast<OGRLayerH>(hLayer);

    oCWI.nElevField = iElevField;
    oCWI.nIDField = iIDField;

    oCWI.adfGeoTransform[0] = 0.0;
    oCWI.adfGeoTransform[1] = 1.0;
    oCWI.adfGeoTransform[2] = 0.0;
    oCWI.adfGeoTransform[3] = 0.0;
    oCWI.adfGeoTransform[4] = 0.0;
    oCWI.adfGeoTransform[5] = 1.0;
    GDALDatasetH hSrcDS = GDALGetBandDataset( hBand );
    if( hSrcDS != NULL )
        GDALGetGeoTransform( hSrcDS, oCWI.adfGeoTransform );
    oCWI.nNextID = 0;

/* -------------------------------------------------------------------- */
/*      Setup contour generator.                                        */
/* -------------------------------------------------------------------- */
    const int nXSize = GDALGetRasterBandXSize( hBand );
    const int nYSize = GDALGetRasterBandYSize( hBand );

    GDALContourGenerator oCG( nXSize, nYSize, OGRContourWriter, &oCWI );
    if( !oCG.Init() )
    {
        return CE_Failure;
    }

    if( nFixedLevelCount > 0 )
        oCG.SetFixedLevels( nFixedLevelCount, padfFixedLevels );
    else
        oCG.SetContourLevels( dfContourInterval, dfContourBase );

    if( bUseNoData )
        oCG.SetNoData( dfNoDataValue );

/* -------------------------------------------------------------------- */
/*      Feed the data into the contour generator.                       */
/* -------------------------------------------------------------------- */
    double *padfScanline =
        static_cast<double *>(VSI_MALLOC2_VERBOSE(sizeof(double), nXSize));
    if( padfScanline == NULL )
    {
        return CE_Failure;
    }

    CPLErr eErr = CE_None;
    for( int iLine = 0; iLine < nYSize && eErr == CE_None; iLine++ )
    {
        eErr = GDALRasterIO( hBand, GF_Read, 0, iLine, nXSize, 1,
                      padfScanline, nXSize, 1, GDT_Float64, 0, 0 );
        if( eErr == CE_None )
            eErr = oCG.FeedLine( padfScanline );

        if( eErr == CE_None &&
            !pfnProgress((iLine + 1) / static_cast<double>(nYSize),
                         "", pProgressArg) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    }

    CPLFree( padfScanline );

    return eErr;
}

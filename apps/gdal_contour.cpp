/******************************************************************************
 * $Id$
 *
 * Project:  Contour Generator
 * Purpose:  Contour Generator mainline.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2003, Applied Coherent Technology (www.actgate.com). 
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
 * Revision 1.1  2003/09/30 18:11:33  warmerda
 * New
 *
 */

#include "gdal.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

class GDALContourItem
{
public:
    int    bRecentlyAccessed;
    double dfLevel;

    int  nPoints;
    int  nMaxPoints;
    double *padfX;
    double *padfY;

    GDALContourItem( double dfLevel );
    ~GDALContourItem();

    int    CheckAttachment( double dfX );
    void   AddSegment( double dfXStart, double dfYStart,
                       double dfXEnd, double dfYEnd );
};

typedef CPLErr (*GDALContourWriter)( GDALContourItem *, void * );

class GDALContourGenerator
{
    int    nWidth;
    int    iLine;

    double *padfLastLine;
    double *padfThisLine;

    int     nActiveContours;
    int     nMaxContours;
    GDALContourItem *papoContours;

    int     nNoDataActive;
    double  dfNoDataValue;

    double  dfContourInterval;
    double  dfContourOffset;

    GDALContourWriter pfnWriter;
    void   *pWriterCBData;

    void   AddSegment( double dfLevel, 
                       double dfXStart, double dfYStart,
                       double dfXEnd, double dfYEnd );

    CPLErr ProcessPixel( int iPixel );

public:
    GDALContourGenerator( int nWidth,
                          GDALContourWriter pfnWriter, void *pWriterCBData );
    ~GDALContourGenerator();

    void                SetNoData( double dfNoDataValue );
    void                SetContourLevels( double dfContourInterval, 
                                          double dfContourOffset = 0.0 );

    CPLErr 		FeedLine( double *padfScanline );
    CPLErr              EjectContours( int bOnlyUnused = FALSE );
};


static CPLErr 
ACTContourGenerate( GDALRasterBandH band, double ContourInterval,
                    double ContourBase, int bHonourNODATA, 
                    const char *ShapefileName, 
                    const char *AttributeName,
                    int bGenerate3D );




/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( 
        "Usage: gdal_contour [-band n] [-off <offset>] [-a <attribute_name>]\n"
        "                    [-3d] [-hnodata] [-inodata] [-snodata n]\n"
        "                    <src_filename> <dst_filename> <interval>\n" );
    exit( 1 );
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int argc, char ** argv )

{
    GDALDatasetH	hSrcDS;
    int i;
    double dfInterval = 0.0;
    const char *pszSrcFilename = NULL;
    const char *pszDstFilename = NULL;

    GDALAllRegister();

/* -------------------------------------------------------------------- */
/*      Parse arguments.                                                */
/* -------------------------------------------------------------------- */
    for( i = 1; i < argc; i++ )
    {
        if( EQUAL(argv[i],"--version") )
        {
            printf( "%s\n", GDALVersionInfo( "--version" ) );
            exit( 0 );
        }
        else if( pszSrcFilename != NULL )
        {
            pszSrcFilename = argv[i];
        }
        else if( pszDstFilename != NULL )
        {
            pszDstFilename = argv[i];
        }
        else if( dfInterval == 0.0 )
        {
            dfInterval = atof(argv[i]);
        }
        else
            Usage();
    }

    if( dfInterval == 0.0 )
    {
        Usage();
    }

/* -------------------------------------------------------------------- */
/*      Open source raster file.                                        */
/* -------------------------------------------------------------------- */
    hSrcDS = GDALOpen( pszSrcFilename, GA_ReadOnly );
    if( hSrcDS == NULL )
        exit( 2 );


}

/************************************************************************/
/*                         ACTContourGenerate()                         */
/************************************************************************/


/************************************************************************/
/* ==================================================================== */
/*                         GDALContourGenerator                         */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                        GDALContourGenerator()                        */
/************************************************************************/

GDALContourGenerator::GDALContourGenerator( int nWidthIn,
                                            GDALContourWriter pfnWriterIn, 
                                            void *pWriterCBDataIn )
{
    nWidth = nWidthIn;
    padfLastLine = (double *) CPLCalloc(sizeof(double),nWidth);
    padfThisLine = (double *) CPLCalloc(sizeof(double),nWidth);

    pfnWriter = pfnWriterIn;
    pWriterCBData = pWriterCBDataIn;

    iLine = -1;
}

/************************************************************************/
/*                       ~GDALContourGenerator()                        */
/************************************************************************/

GDALContourGenerator::~GDALContourGenerator()

{
    
}

/************************************************************************/
/*                            ProcessPixel()                            */
/************************************************************************/

CPLErr GDALContourGenerator::ProcessPixel( int iPixel )

{
    double  dfUpLeft, dfUpRight, dfLoLeft, dfLoRight;

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
/*      Check if we have any nodata, if so, go to a special case of     */
/*      code.                                                           */
/* -------------------------------------------------------------------- */
    // TODO 

    
/* -------------------------------------------------------------------- */
/*      Identify the range of contour levels we have to deal with.      */
/* -------------------------------------------------------------------- */
    int iStartLevel, iEndLevel;

    double dfMin = MIN(MIN(dfUpLeft,dfUpRight),MIN(dfLoLeft,dfLoRight));
    double dfMax = MAX(MAX(dfUpLeft,dfUpRight),MAX(dfLoLeft,dfLoRight));
    
    iStartLevel = floor((dfMin - dfContourOffset) / dfContourInterval);
    iEndLevel = floor((dfMax - dfContourOffset) / dfContourInterval);

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
        
        Intersect( dfUpLeft, iPixel-0.5, iLine-0.5, 
                   dfLoLeft, iPxeil-0.5, iLine+0.5, 
                   dfLevel, &nPoints, adfX, adfY );
        Intersect( dfLoLeft, iPxeil-0.5, iLine+0.5, 
                   dfLoRight, iPixel+0.5, iLine+0.5, 
                   dfLevel, &nPoints, adfX, adfY );
        Intersect( dfLoRight, iPixel+0.5, iLine+0.5, 
                   dfUpRight, iPxeil+0.5, iLine-0.5, 
                   dfLevel, &nPoints, adfX, adfY );
        Intersect( dfUpRight, iPxeil+0.5, iLine-0.5, 
                   dfUpLeft, iPixel-0.5, iLine-0.5, 
                   dfLevel, &nPoints, adfX, adfY );

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
                                      double dfLevel, int *pnPoints, 
                                      double *padfX, double *padfY )

{
    if( dfVal1 <= dfLevel && dfVal2 > dfLevel )
    {
        double dfRatio = (dfLevel - dfVal1) / (dfVal2 - dfVal1);

        *padfX[*pnPoints] = dfX1 * (dfRatio - 1.0) + dfX2 * dfRatio;
        *padfY[*pnPoints] = dfY1 * (dfRatio - 1.0) + dfY2 * dfRatio;
        *(pnPoints)++;
    }
    else if( dfVal1 > dfLevel && dfVal2 <= dfLevel )
    {
        double dfRatio = (dfLevel - dfVal2) / (dfVal1 - dfVal2);

        *padfX[*pnPoints] = dfX2 * (dfRatio - 1.0) + dfX1 * dfRatio;
        *padfY[*pnPoints] = dfY2 * (dfRatio - 1.0) + dfY1 * dfRatio;
        *(pnPoints)++;
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
            && (papoContours[iContour]->CheckAttachment( dfX1 )
                || papoContours[iContour]->CheckAttachment( dfX2 )) )
        {
            papoContours[iContour]->AddSegment( dfX1, dfY1, dfX2, dfY2 );
            return CE_None;
        }
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

    memcpy( padfThisLine, padfScanline, sizeof(double) * nWidth );
    
/* -------------------------------------------------------------------- */
/*      If this is the first line we need to initialize the previous    */
/*      line from the first line of data.                               */
/* -------------------------------------------------------------------- */
    if( iLine == -1 )
        memcpy( padfLastLine, padfThisLine, sizeof(double) * nWidth );

/* -------------------------------------------------------------------- */
/*      Clear the recently used flags on the contours so we can         */
/*      check later which ones were touched for this scanline.          */
/* -------------------------------------------------------------------- */
    int iContour;

    for( iContour = 0; iContour < nActiveContour; iContour++ )
        papoContours[iContour]->bRecentlyAccessed = FALSE;

/* -------------------------------------------------------------------- */
/*      Process each pixel.                                             */
/* -------------------------------------------------------------------- */
    for( iPixel = 0; iPixel < nWidth+1; iPixel++ )
    {
        CPLErr eErr = ProcessPixel( iPixel );
        if( eErr != NULL )
            return eErr;
    }
    
    return EjectContours( TRUE );
}

/************************************************************************/
/*                           EjectContours()                            */
/************************************************************************/

CPLErr GDALContourGenerator::EjectContours( int bOnlyUnused )

{
    int nContoursKept = 0, iContour;
    CPLErr eErr = CE_None;

/* -------------------------------------------------------------------- */
/*      Write candidate contours.                                       */
/* -------------------------------------------------------------------- */
    for( iContour = 0; 
         iContour < nActiveContours && eErr == CE_None; 
         iContour++ )
    {

        if( bOnlyUsed && papoContours[iContour]->bRecentlyAccessed )
        {
            papoContours[nContoursKept++] = papoContours[iContour];
            continue;
        }

        eErr = pfnWriter( papoContours[iContour], pWriterCBData );
        if( eErr != CE_None )
            break;

        delete papoContours[iContour];
        papoContours[iContour] = NULL;
    }

    nActiveContours = nContoursKept;

    return eErr;
}

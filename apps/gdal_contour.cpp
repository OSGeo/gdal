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
 * Revision 1.2  2003/10/09 18:29:27  warmerda
 * basics working now
 *
 * Revision 1.1  2003/09/30 18:11:33  warmerda
 * New
 *
 */

#include "gdal.h"
#include "cpl_conv.h"
#include "ogr_api.h"

// The amount of a contour interval that pixels should be fudged by if they
// match a contour level exactly.

#define FUDGE_EXACT 0.001

// The amount of a pixel that line ends need to be within to be considered to
// match for joining purposes. 

#define JOIN_DIST 0.0001

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

    int    AddSegment( double dfXStart, double dfYStart,
                       double dfXEnd, double dfYEnd );
    void   MakeRoomFor( int );
    int    Merge( GDALContourItem * );
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
    GDALContourItem **papoContours;

    int     bNoDataActive;
    double  dfNoDataValue;

    double  dfContourInterval;
    double  dfContourOffset;

    GDALContourWriter pfnWriter;
    void   *pWriterCBData;

    CPLErr AddSegment( double dfLevel, 
                       double dfXStart, double dfYStart,
                       double dfXEnd, double dfYEnd );

    CPLErr ProcessPixel( int iPixel );
    void   Intersect( double, double, double, 
                      double, double, double, 
                      double, double, int *, double *, double * );

public:
    GDALContourGenerator( int nWidth,
                          GDALContourWriter pfnWriter, void *pWriterCBData );
    ~GDALContourGenerator();

    void                SetNoData( double dfNoDataValue );
    void                SetContourLevels( double dfContourInterval, 
                                          double dfContourOffset = 0.0 )
        { this->dfContourInterval = dfContourInterval;
          this->dfContourOffset = dfContourOffset; }

    CPLErr 		FeedLine( double *padfScanline );
    CPLErr              EjectContours( int bOnlyUnused = FALSE );
};

class OGRContourWriterInfo
{
public:
    OGRLayerH hLayer;

    double adfGeoTransform[6];
    
    int    nElevField;
    int    nNextID;
};

CPLErr OGRContourWriter( GDALContourItem *, void *pInfo );


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
    const char *pszElevAttrib = NULL;

    GDALAllRegister();
    OGRRegisterAll();

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
        else if( EQUAL(argv[i],"-a") && i < argc-1 )
        {
            pszElevAttrib = argv[++i];
        }
        else if( pszSrcFilename == NULL )
        {
            pszSrcFilename = argv[i];
        }
        else if( pszDstFilename == NULL )
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

/* -------------------------------------------------------------------- */
/*      Create the outputfile.                                          */
/* -------------------------------------------------------------------- */
    OGRDataSourceH hDS;
    OGRContourWriterInfo oCWI;
    OGRSFDriverH hDriver = OGRGetDriverByName( "ESRI Shapefile" );
    OGRFieldDefnH hFld;

    hDS = OGR_Dr_CreateDataSource( hDriver, pszDstFilename, NULL );
    if( hDS == NULL )
        exit( 1 );

    oCWI.hLayer = OGR_DS_CreateLayer( hDS, "contour", NULL, wkbLineString25D, 
                                      NULL );
    if( oCWI.hLayer == NULL )
        exit( 1 );

    hFld = OGR_Fld_Create( "ID", OFTInteger );
    OGR_Fld_SetWidth( hFld, 8 );
    OGR_L_CreateField( oCWI.hLayer, hFld, FALSE );
    OGR_Fld_Destroy( hFld );

    oCWI.nElevField = -1;
    if( pszElevAttrib )
    {
        hFld = OGR_Fld_Create( pszElevAttrib, OFTReal );
        OGR_Fld_SetWidth( hFld, 12 );
        OGR_Fld_SetPrecision( hFld, 3 );
        OGR_L_CreateField( oCWI.hLayer, hFld, FALSE );
        OGR_Fld_Destroy( hFld );
        oCWI.nElevField = 1;
    }

    GDALGetGeoTransform( hSrcDS, oCWI.adfGeoTransform );
    oCWI.nNextID = 0;

/* -------------------------------------------------------------------- */
/*      Setup contour generator.                                        */
/* -------------------------------------------------------------------- */
    int nXSize = GDALGetRasterXSize( hSrcDS );
    int nYSize = GDALGetRasterYSize( hSrcDS );

    GDALContourGenerator oCG( nXSize, OGRContourWriter, &oCWI );

    oCG.SetContourLevels( dfInterval );

/* -------------------------------------------------------------------- */
/*      Feed the data into the contour generator.                       */
/* -------------------------------------------------------------------- */
    int iLine;
    double *padfScanline;

    padfScanline = (double *) CPLMalloc(sizeof(double) * nXSize);

    for( iLine = 0; iLine < nYSize; iLine++ )
    {
        GDALRasterIO( GDALGetRasterBand( hSrcDS, 1 ), GF_Read, 
                      0, iLine, nXSize, 1, 
                      padfScanline, nXSize, 1, GDT_Float64, 0, 0 );
        oCG.FeedLine( padfScanline );
    }

    oCG.FeedLine( NULL );

    OGR_DS_Destroy( hDS );
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
    nActiveContours = 0;
    nMaxContours = 0;
    papoContours = NULL;

    bNoDataActive = FALSE;
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

        Intersect( dfUpLeft, iPixel-0.5, iLine-0.5, 
                   dfLoLeft, iPixel-0.5, iLine+0.5, 
                   dfLoRight, dfLevel, &nPoints, adfX, adfY );
        Intersect( dfLoLeft, iPixel-0.5, iLine+0.5, 
                   dfLoRight, iPixel+0.5, iLine+0.5, 
                   dfUpRight, dfLevel, &nPoints, adfX, adfY );
        Intersect( dfLoRight, iPixel+0.5, iLine+0.5, 
                   dfUpRight, iPixel+0.5, iLine-0.5, 
                   dfUpLeft, dfLevel, &nPoints, adfX, adfY );
        Intersect( dfUpRight, iPixel+0.5, iLine-0.5, 
                   dfUpLeft, iPixel-0.5, iLine-0.5, 
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
    if( fabs(padfX[0]-dfXStart) < JOIN_DIST && fabs(padfY[0]-dfYStart) < JOIN_DIST )
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
    else if( fabs(padfX[nPoints-1]-dfXStart) < JOIN_DIST 
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
/*                          OGRContourWriter()                          */
/************************************************************************/

CPLErr OGRContourWriter( GDALContourItem *poContour, void *pInfo )

{
    OGRContourWriterInfo *poInfo = (OGRContourWriterInfo *) pInfo;
    OGRFeatureH hFeat;
    OGRGeometryH hGeom;
    int iPoint;

    hFeat = OGR_F_Create( OGR_L_GetLayerDefn( poInfo->hLayer ) );

    OGR_F_SetFieldInteger( hFeat, 0, poInfo->nNextID++ );
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
                        0.0 /* poContour->dfLevel */ );
    }

    OGR_F_SetGeometryDirectly( hFeat, hGeom );

    OGR_L_CreateFeature( poInfo->hLayer, hFeat );
    OGR_F_Destroy( hFeat );

    return CE_None;
}


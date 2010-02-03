/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Utilities
 * Purpose:  Convert nearly black or nearly white border to exact black/white.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 * ****************************************************************************
 * Copyright (c) 2006, MapShots Inc (www.mapshots.com)
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

#include "gdal.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

static void ProcessLine( GByte *pabyLine, int iStart, int iEnd, int nSrcBands, int nDstBands,
                         int nNearDist, int nMaxNonBlack, int bNearWhite,
                         int *panLastLineCounts, 
                         int bDoHorizontalCheck, 
                         int bDoVerticalCheck );

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

void Usage()
{
    printf( "nearblack [-of format] [-white] [-near dist] [-nb non_black_pixels]\n"
            "          [-setalpha] [-o outfile] [-q] [-co \"NAME=VALUE\"]* infile\n" );
    exit( 1 );
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int argc, char ** argv )

{
    /* Check that we are running against at least GDAL 1.4 (probably older in fact !) */
    /* Note to developers : if we use newer API, please change the requirement */
    if (atoi(GDALVersionInfo("VERSION_NUM")) < 1400)
    {
        fprintf(stderr, "At least, GDAL >= 1.4.0 is required for this version of %s, "
                "which was compiled against GDAL %s\n", argv[0], GDAL_RELEASE_NAME);
        exit(1);
    }

/* -------------------------------------------------------------------- */
/*      Generic arg processing.                                         */
/* -------------------------------------------------------------------- */
    GDALAllRegister();
    GDALSetCacheMax( 100000000 );
    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
    if( argc < 1 )
        exit( -argc );
    
/* -------------------------------------------------------------------- */
/*      Parse arguments.                                                */
/* -------------------------------------------------------------------- */
    int i;
    const char *pszOutFile = NULL;
    const char *pszInFile = NULL;
    int nMaxNonBlack = 2;
    int nNearDist = 15;
    int bNearWhite = FALSE;
    int bSetAlpha = FALSE;
    const char* pszDriverName = "HFA";
    char** papszCreationOptions = NULL;
    int bQuiet = FALSE;
    
    for( i = 1; i < argc; i++ )
    {
        if( EQUAL(argv[i], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and is running against GDAL %s\n",
                   argv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            return 0;
        }
        else if( EQUAL(argv[i], "-o") && i < argc-1 )
            pszOutFile = argv[++i];
        else if( EQUAL(argv[i], "-of") && i < argc-1 )
            pszDriverName = argv[++i];
        else if( EQUAL(argv[i], "-white") )
            bNearWhite = TRUE;
        else if( EQUAL(argv[i], "-nb") && i < argc-1 )
            nMaxNonBlack = atoi(argv[++i]);
        else if( EQUAL(argv[i], "-near") && i < argc-1 )
            nNearDist = atoi(argv[++i]);
        else if( EQUAL(argv[i], "-setalpha") )
            bSetAlpha = TRUE;
        else if( EQUAL(argv[i], "-q") || EQUAL(argv[i], "-quiet") )
            bQuiet = TRUE;
        else if( EQUAL(argv[i], "-co") && i < argc-1 )
            papszCreationOptions = CSLAddString(papszCreationOptions, argv[++i]);
        else if( argv[i][0] == '-' )
            Usage();
        else if( pszInFile == NULL )
            pszInFile = argv[i];
        else
            Usage();
    }

    if( pszInFile == NULL )
        Usage();

    if( pszOutFile == NULL )
        pszOutFile = pszInFile;

/* -------------------------------------------------------------------- */
/*      Open input file.                                                */
/* -------------------------------------------------------------------- */
    GDALDatasetH hInDS, hOutDS = NULL;
    int nXSize, nYSize, nBands;

    if( pszOutFile == pszInFile )
        hInDS = hOutDS = GDALOpen( pszInFile, GA_Update );
    else
        hInDS = GDALOpen( pszInFile, GA_ReadOnly );

    if( hInDS == NULL )
        exit( 1 );

    nXSize = GDALGetRasterXSize( hInDS );
    nYSize = GDALGetRasterYSize( hInDS );
    nBands = GDALGetRasterCount( hInDS );
    int nDstBands = nBands;

    if( hOutDS != NULL && papszCreationOptions != NULL)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                  "Warning: creation options are ignored when writing to an existing file.");
    }

/* -------------------------------------------------------------------- */
/*      Do we need to create output file?                               */
/* -------------------------------------------------------------------- */
    if( hOutDS == NULL )
    {
        GDALDriverH hDriver = GDALGetDriverByName( pszDriverName );
        if (hDriver == NULL)
            exit(1);

        if (bSetAlpha)
        {
            if (nBands == 3)
                nDstBands ++;
            else if (nBands == 4)
                nBands --;
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Number of bands of file = %d. Expected 3 or 4.",
                         nBands);
                exit(1);
            }
        }

        hOutDS = GDALCreate( hDriver, pszOutFile, 
                             nXSize, nYSize, nDstBands, GDT_Byte, 
                             papszCreationOptions );
        if( hOutDS == NULL )
            exit( 1 );

        double adfGeoTransform[6];

        if( GDALGetGeoTransform( hInDS, adfGeoTransform ) == CE_None )
        {
            GDALSetGeoTransform( hOutDS, adfGeoTransform );
            GDALSetProjection( hOutDS, GDALGetProjectionRef( hInDS ) );
        }
    }
    else
    {
        if (bSetAlpha)
        {
            if (nBands != 4)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Number of bands of output file = %d. Expected 4.",
                        nBands);
                exit(1);
            }

            nBands --;
        }
    }

    if (GDALGetRasterXSize(hOutDS) != nXSize ||
        GDALGetRasterYSize(hOutDS) != nYSize)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The dimensions of the output dataset don't match "
                 "the dimensions of the input dataset.");
        exit(1);
    }


    int iBand;
    for( iBand = 0; iBand < nBands; iBand++ )
    {
        GDALRasterBandH hBand = GDALGetRasterBand(hInDS, iBand+1);
        if (GDALGetRasterDataType(hBand) != GDT_Byte)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Band %d is not of type GDT_Byte. It can lead to unexpected results.", iBand+1);
        }
        if (GDALGetRasterColorTable(hBand) != NULL)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Band %d has a color table, which is ignored by nearblack. "
                     "It can lead to unexpected results.", iBand+1);
        }
    }

/* -------------------------------------------------------------------- */
/*      Allocate a line buffer.                                         */
/* -------------------------------------------------------------------- */
    GByte *pabyLine;
    int   *panLastLineCounts;

    pabyLine = (GByte *) CPLMalloc(nXSize * nDstBands);

    panLastLineCounts = (int *) CPLCalloc(sizeof(int),nXSize);

/* -------------------------------------------------------------------- */
/*      Processing data one line at a time.                             */
/* -------------------------------------------------------------------- */
    int iLine;

    for( iLine = 0; iLine < nYSize; iLine++ )
    {
        CPLErr eErr;

        eErr = GDALDatasetRasterIO( hInDS, GF_Read, 0, iLine, nXSize, 1, 
                                    pabyLine, nXSize, 1, GDT_Byte, 
                                    nBands, NULL, nDstBands, nXSize * nDstBands, 1 );
        if( eErr != CE_None )
            break;
        
        if (bSetAlpha)
        {
            int iCol;
            for(iCol = 0; iCol < nXSize; iCol ++)
            {
                pabyLine[iCol * nDstBands + nDstBands - 1] = 255;
            }
        }
        
        ProcessLine( pabyLine, 0, nXSize-1, nBands, nDstBands, nNearDist, nMaxNonBlack,
                     bNearWhite, panLastLineCounts, TRUE, TRUE );
        ProcessLine( pabyLine, nXSize-1, 0, nBands, nDstBands, nNearDist, nMaxNonBlack,
                     bNearWhite, NULL, TRUE, FALSE );
        
        eErr = GDALDatasetRasterIO( hOutDS, GF_Write, 0, iLine, nXSize, 1, 
                                    pabyLine, nXSize, 1, GDT_Byte, 
                                    nDstBands, NULL, nDstBands, nXSize * nDstBands, 1 );
        if( eErr != CE_None )
            break;
        
        if (!bQuiet)
            GDALTermProgress( 0.5 * ((iLine+1) / (double) nYSize), NULL, NULL );
    }

/* -------------------------------------------------------------------- */
/*      Now process from the bottom back up, doing only the vertical pass.*/
/* -------------------------------------------------------------------- */
    memset( panLastLineCounts, 0, sizeof(int) * nXSize);
    
    for( iLine = nYSize-1; iLine >= 0; iLine-- )
    {
        CPLErr eErr;

        eErr = GDALDatasetRasterIO( hOutDS, GF_Read, 0, iLine, nXSize, 1, 
                                    pabyLine, nXSize, 1, GDT_Byte, 
                                    nDstBands, NULL, nDstBands, nXSize * nDstBands, 1 );
        if( eErr != CE_None )
            break;
        
        ProcessLine( pabyLine, 0, nXSize-1, nBands, nDstBands, nNearDist, nMaxNonBlack,
                     bNearWhite, panLastLineCounts, FALSE, TRUE );
        
        eErr = GDALDatasetRasterIO( hOutDS, GF_Write, 0, iLine, nXSize, 1, 
                                    pabyLine, nXSize, 1, GDT_Byte, 
                                    nDstBands, NULL, nDstBands, nXSize * nDstBands, 1 );
        if( eErr != CE_None )
            break;
        
        if (!bQuiet)
            GDALTermProgress( 0.5 + 0.5 * (nYSize-iLine) / (double) nYSize, 
                            NULL, NULL );
    }

    CPLFree(pabyLine);
    CPLFree( panLastLineCounts );

    GDALClose( hOutDS );
    if( hInDS != hOutDS )
        GDALClose( hInDS );
    GDALDumpOpenDatasets( stderr );
    CSLDestroy( argv );
    CSLDestroy( papszCreationOptions );
    GDALDestroyDriverManager();
    
    return 0;
}

/************************************************************************/
/*                            ProcessLine()                             */
/*                                                                      */
/*      Process a single scanline of image data.                        */
/************************************************************************/

static void ProcessLine( GByte *pabyLine, int iStart, int iEnd, int nSrcBands,
                         int nDstBands,
                         int nNearDist, int nMaxNonBlack, int bNearWhite,
                         int *panLastLineCounts, 
                         int bDoHorizontalCheck, 
                         int bDoVerticalCheck )

{
    int iDir, i;

/* -------------------------------------------------------------------- */
/*      Vertical checking.                                              */
/* -------------------------------------------------------------------- */
    if( bDoVerticalCheck )
    {
        int nXSize = MAX(iStart+1,iEnd+1);

        for( i = 0; i < nXSize; i++ )
        {
            int iBand;
            int bIsNonBlack = FALSE;

            // are we already terminated for this column?
            if( panLastLineCounts[i] > nMaxNonBlack )
                continue;

            for( iBand = 0; iBand < nSrcBands; iBand++ )
            {

                if( bNearWhite )
                {
                    if( (255 - pabyLine[i * nDstBands + iBand]) > nNearDist )
                    {
                        bIsNonBlack = TRUE;
                        break;
                    }
                }
                else
                {
                    if( pabyLine[i * nDstBands + iBand] > nNearDist )
                    {
                        bIsNonBlack = TRUE;
                        break;
                    }
                }
            }

            if( bIsNonBlack )
            {
                panLastLineCounts[i]++;

                if( panLastLineCounts[i] > nMaxNonBlack )
                    continue; 
            }
            else
                panLastLineCounts[i] = 0;

            for( iBand = 0; iBand < nSrcBands; iBand++ )
            {
                if( bNearWhite )
                    pabyLine[i * nDstBands + iBand] = 255;
                else
                    pabyLine[i * nDstBands + iBand] = 0;
            }
            if( nSrcBands != nDstBands )
                pabyLine[i * nDstBands + nDstBands - 1] = 0;
        }
    }

/* -------------------------------------------------------------------- */
/*      Horizontal Checking.                                            */
/* -------------------------------------------------------------------- */
    if( bDoHorizontalCheck )
    {
        int nNonBlackPixels = 0;

        if( iStart < iEnd )
            iDir = 1;
        else
            iDir = -1;

        for( i = iStart; i != iEnd; i += iDir )
        {
            int iBand;
            int bIsNonBlack = FALSE;

            for( iBand = 0; iBand < nSrcBands; iBand++ )
            {
                if( bNearWhite )
                {
                    if( (255 - pabyLine[i * nDstBands + iBand]) > nNearDist )
                    {
                        bIsNonBlack = TRUE;
                        break;
                    }
                }
                else
                {
                    if( pabyLine[i * nDstBands + iBand] > nNearDist )
                    {
                        bIsNonBlack = TRUE;
                        break;
                    }
                }
            }

            if( bIsNonBlack )
            {
                nNonBlackPixels++;

                if( nNonBlackPixels > nMaxNonBlack )
                    return;
            }
            else
                nNonBlackPixels = 0;

            for( iBand = 0; iBand < nSrcBands; iBand++ )
            {
                if( bNearWhite )
                    pabyLine[i * nDstBands + iBand] = 255;
                else
                    pabyLine[i * nDstBands + iBand] = 0;
            }
            if( nSrcBands != nDstBands )
                pabyLine[i * nDstBands + nDstBands - 1] = 0;
        }
    }

}

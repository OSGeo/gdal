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
#include <vector>
#include "commonutils.h"

CPL_CVSID("$Id$");

typedef std::vector<int> Color;
typedef std::vector< Color > Colors;

static void ProcessLine( GByte *pabyLine, GByte *pabyMask, int iStart,
                         int iEnd, int nSrcBands, int nDstBands, int nNearDist,
                         int nMaxNonBlack, int bNearWhite, Colors *poColors,
                         int *panLastLineCounts, int bDoHorizontalCheck,
                         int bDoVerticalCheck, int bBottomUp);

/************************************************************************/
/*                            IsInt()                                   */
/************************************************************************/

int IsInt( const char *pszArg )
{
    if( pszArg[0] == '-' )
        pszArg++;

    if( *pszArg == '\0' )
        return FALSE;

    while( *pszArg != '\0' )
    {
        if( *pszArg < '0' || *pszArg > '9' )
            return FALSE;
        pszArg++;
    }

    return TRUE;
}

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

void Usage(const char* pszErrorMsg = NULL)
{
    printf( "nearblack [-of format] [-white | [-color c1,c2,c3...cn]*] [-near dist] [-nb non_black_pixels]\n"
            "          [-setalpha] [-setmask] [-o outfile] [-q] [-co \"NAME=VALUE\"]* infile\n" );

    if( pszErrorMsg != NULL )
        fprintf(stderr, "\nFAILURE: %s\n", pszErrorMsg);

    exit( 1 );
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

#define CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(nExtraArg) \
    do { if (i + nExtraArg >= argc) \
        Usage(CPLSPrintf("%s option requires %d argument(s)", argv[i], nExtraArg)); } while(0)

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
    int bSetMask = FALSE;
    const char* pszDriverName = "HFA";
    int bFormatExplicitelySet = FALSE;
    char** papszCreationOptions = NULL;
    int bQuiet = FALSE;

    Colors oColors;
    
    for( i = 1; i < argc; i++ )
    {
        if( EQUAL(argv[i], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and is running against GDAL %s\n",
                   argv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            return 0;
        }
        else if( EQUAL(argv[i], "--help") )
            Usage();
        else if( EQUAL(argv[i], "-o") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszOutFile = argv[++i];
        }
        else if( EQUAL(argv[i], "-of") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszDriverName = argv[++i];
            bFormatExplicitelySet = TRUE;
        }
        else if( EQUAL(argv[i], "-white") ) {
            bNearWhite = TRUE;
        }

        /***** -color c1,c2,c3...cn *****/
        
        else if( EQUAL(argv[i], "-color") ) {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            Color oColor;
            
            /***** tokenize the arg on , *****/
            
            char **papszTokens;
            papszTokens = CSLTokenizeString2( argv[++i], ",", 0 );

            /***** loop over the tokens *****/
            
            int iToken;
            for( iToken = 0; papszTokens && papszTokens[iToken]; iToken++ )
            {

                /***** ensure the token is an int and add it to the color *****/
                
                if ( IsInt( papszTokens[iToken] ) )
                    oColor.push_back( atoi( papszTokens[iToken] ) );
                else {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Colors must be valid integers." );
                    CSLDestroy( papszTokens );
                    exit(1);
                }
            }
            
            CSLDestroy( papszTokens );

            /***** check if the number of bands is consistant *****/

            if ( oColors.size() > 0 &&
                 oColors.front().size() != oColor.size() )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "ERROR: all -color args must have the same number of values.\n" );
                exit(1);
            }

            /***** add the color to the colors *****/
            
            oColors.push_back( oColor );
            
        }
        
        else if( EQUAL(argv[i], "-nb") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            nMaxNonBlack = atoi(argv[++i]);
        }
        else if( EQUAL(argv[i], "-near") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            nNearDist = atoi(argv[++i]);
        }
        else if( EQUAL(argv[i], "-setalpha") )
            bSetAlpha = TRUE;
        else if( EQUAL(argv[i], "-setmask") )
            bSetMask = TRUE;
        else if( EQUAL(argv[i], "-q") || EQUAL(argv[i], "-quiet") )
            bQuiet = TRUE;
        else if( EQUAL(argv[i], "-co") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            papszCreationOptions = CSLAddString(papszCreationOptions, argv[++i]);
        }
        else if( argv[i][0] == '-' )
            Usage(CPLSPrintf("Unknown option name '%s'", argv[i]));
        else if( pszInFile == NULL )
            pszInFile = argv[i];
        else
            Usage("Too many command options.");
    }

    if( pszInFile == NULL )
        Usage("No input file specified.");

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

        if (!bQuiet && !bFormatExplicitelySet)
            CheckExtensionConsistency(pszOutFile, pszDriverName);

        if (bSetAlpha)
        {
            /***** fixme there should be a way to preserve alpha band data not in the collar *****/
            if (nBands == 4)
                nBands --;
            else
                nDstBands ++;
        }

        if (bSetMask)
        {
            if (nBands == 4)
                nDstBands = nBands = 3;
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
            if (nBands != 4 &&
                (nBands < 2 ||
                 GDALGetRasterColorInterpretation(GDALGetRasterBand(hOutDS, nBands)) != GCI_AlphaBand))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Last band is not an alpha band.");
                exit(1);
            }

            nBands --;
        }

        if (bSetMask)
        {
            if (nBands == 4)
                nDstBands = nBands = 3;
        }
    }

    /***** set a color if there are no colors set? *****/

    if ( oColors.size() == 0) {
        Color oColor;

        /***** loop over the bands to get the right number of values *****/

        int iBand;
        for (iBand = 0; iBand < nBands ; iBand++) {

            /***** black or white? *****/

            if (bNearWhite) 
                oColor.push_back(255);
            else
                oColor.push_back(0);
        }

        /***** add the color to the colors *****/

        oColors.push_back(oColor);
            
    }

    /***** does the number of bands match the number of color values? *****/

    if ( (int)oColors.front().size() != nBands ) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "-color args must have the same number of values as the non alpha input band count.\n" );
        exit(1); 
    }

    /***** check the input and output datasets are the same size *****/
    
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

    GDALRasterBandH hMaskBand = NULL;
    
    if (bSetMask) {

        /***** if there isn't already a mask band on the output file create one *****/
        
        if ( GMF_PER_DATASET != GDALGetMaskFlags( GDALGetRasterBand(hOutDS, 1) ) )
        {

            if ( CE_None != GDALCreateDatasetMaskBand(hOutDS, GMF_PER_DATASET) ) {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to create mask band on output DS");
                bSetMask = FALSE;
            }
        }

        if (bSetMask) {
            hMaskBand = GDALGetMaskBand(GDALGetRasterBand(hOutDS, 1));
        }
    }

/* -------------------------------------------------------------------- */
/*      Allocate a line buffer.                                         */
/* -------------------------------------------------------------------- */
    GByte *pabyLine;
    GByte *pabyMask=NULL;
    
    int   *panLastLineCounts;

    pabyLine = (GByte *) CPLMalloc(nXSize * nDstBands);
    
    if (bSetMask)
        pabyMask = (GByte *) CPLMalloc(nXSize);
    
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
        
        if (bSetMask)
        {
            int iCol;
            for(iCol = 0; iCol < nXSize; iCol ++)
            {
                pabyMask[iCol] = 255;
            }
        }
        
        ProcessLine( pabyLine, pabyMask, 0, nXSize-1, nBands, nDstBands,
                     nNearDist, nMaxNonBlack, bNearWhite, &oColors,
                     panLastLineCounts,
                     TRUE, // bDoHorizontalCheck
                     TRUE, // bDoVerticalCheck
                     FALSE // bBottomUp
                    );
        ProcessLine( pabyLine, pabyMask, nXSize-1, 0, nBands, nDstBands,
                     nNearDist, nMaxNonBlack, bNearWhite, &oColors,
                     panLastLineCounts,
                     TRUE,  // bDoHorizontalCheck
                     FALSE, // bDoVerticalCheck
                     FALSE  // bBottomUp
                    );
        
        eErr = GDALDatasetRasterIO( hOutDS, GF_Write, 0, iLine, nXSize, 1, 
                                    pabyLine, nXSize, 1, GDT_Byte, 
                                    nDstBands, NULL, nDstBands, nXSize * nDstBands, 1 );

        if( eErr != CE_None )
            break;
    
        /***** write out the mask band line *****/

        if (bSetMask) {

            eErr = GDALRasterIO ( hMaskBand, GF_Write, 0, iLine, nXSize, 1,
                                  pabyMask, nXSize, 1, GDT_Byte,
                                  0, 0 );
                             
            if( eErr != CE_None ) {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "ERROR writeing out line to mask band.");
               break;
            }
        }
        
        if (!bQuiet)
            GDALTermProgress( 0.5 * ((iLine+1) / (double) nYSize), NULL, NULL );
    }

/* -------------------------------------------------------------------- */
/*      Now process from the bottom back up                            .*/
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

        /***** read the mask band line back in *****/

        if (bSetMask) {

            eErr = GDALRasterIO ( hMaskBand, GF_Read, 0, iLine, nXSize, 1,
                                  pabyMask, nXSize, 1, GDT_Byte,
                                  0, 0 );
                             
                                
            if( eErr != CE_None )
                break;
        }

        
        ProcessLine( pabyLine, pabyMask, 0, nXSize-1, nBands, nDstBands,
                     nNearDist, nMaxNonBlack, bNearWhite, &oColors,
                     panLastLineCounts,
                     TRUE, // bDoHorizontalCheck
                     TRUE, // bDoVerticalCheck
                     TRUE  // bBottomUp
                   );
        ProcessLine( pabyLine, pabyMask, nXSize-1, 0, nBands, nDstBands,
                     nNearDist, nMaxNonBlack, bNearWhite, &oColors,
                     panLastLineCounts,
                     TRUE,  // bDoHorizontalCheck
                     FALSE, // bDoVerticalCheck
                     TRUE   // bBottomUp
                    );
        
        eErr = GDALDatasetRasterIO( hOutDS, GF_Write, 0, iLine, nXSize, 1, 
                                    pabyLine, nXSize, 1, GDT_Byte, 
                                    nDstBands, NULL, nDstBands, nXSize * nDstBands, 1 );
        if( eErr != CE_None )
            break;

        /***** write out the mask band line *****/

        if (bSetMask) {

            eErr = GDALRasterIO ( hMaskBand, GF_Write, 0, iLine, nXSize, 1,
                                  pabyMask, nXSize, 1, GDT_Byte,
                                  0, 0 );
                             
                                
            if( eErr != CE_None )
                break;
        }

        
        if (!bQuiet)
            GDALTermProgress( 0.5 + 0.5 * (nYSize-iLine) / (double) nYSize, 
                            NULL, NULL );
    }

    CPLFree(pabyLine);
    if (bSetMask)
        CPLFree(pabyMask);
    
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

static void ProcessLine( GByte *pabyLine, GByte *pabyMask, int iStart,
                        int iEnd, int nSrcBands, int nDstBands, int nNearDist,
                        int nMaxNonBlack, int bNearWhite, Colors *poColors,
                        int *panLastLineCounts, int bDoHorizontalCheck,
                        int bDoVerticalCheck, int bBottomUp )
{
    int iDir, i;
    GByte nReplacevalue = 0;
    if( bNearWhite )
        nReplacevalue = 255;

    /* -------------------------------------------------------------------- */
    /*      Vertical checking.                                              */
    /* -------------------------------------------------------------------- */
    
    if( bDoVerticalCheck )
    {
        int nXSize = MAX(iStart+1,iEnd+1);

        for( i = 0; i < nXSize; i++ )
        {

            // are we already terminated for this column?

            if( panLastLineCounts[i] > nMaxNonBlack )
                continue;

            /***** is the pixel valid data? ****/

            int bIsNonBlack = FALSE;

            /***** loop over the colors *****/

            int iColor;
            for (iColor = 0; iColor < (int)poColors->size(); iColor++) {

                Color oColor = (*poColors)[iColor];

                bIsNonBlack = FALSE;

                /***** loop over the bands *****/

                int iBand;
                for( iBand = 0; iBand < nSrcBands; iBand++ )
                {
                    int nPix = pabyLine[i * nDstBands + iBand];

                    if( oColor[iBand] - nPix > nNearDist ||
                       nPix > nNearDist + oColor[iBand] )
                    {
                        bIsNonBlack = TRUE;
                        break;
                    }
                }
                
                if (bIsNonBlack == FALSE)
                    break;          
            }

            if (bIsNonBlack) {
                panLastLineCounts[i]++;

                if( panLastLineCounts[i] > nMaxNonBlack )
                    continue; 
            }
            //else
            //  panLastLineCounts[i] = 0; // not sure this even makes sense 

            /***** replace the pixel values *****/

            int iBand;
            for( iBand = 0; iBand < nSrcBands; iBand++ )
                pabyLine[i * nDstBands + iBand] = nReplacevalue;

            /***** alpha *****/

            if( nDstBands > nSrcBands )
                pabyLine[i * nDstBands + nDstBands - 1] = 0;

            /***** mask *****/

            if (pabyMask != NULL)
                pabyMask[i] = 0;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Horizontal Checking.                                            */
    /* -------------------------------------------------------------------- */
    
    if( bDoHorizontalCheck )
    {
        int nNonBlackPixels = 0;

        /***** on a bottom up pass assume nMaxNonBlack is 0 *****/ 

        if (bBottomUp)
            nMaxNonBlack = 0;

        if( iStart < iEnd )
            iDir = 1;
        else
            iDir = -1;
        int bDoTest = TRUE;

        for( i = iStart; i != iEnd; i += iDir )
        {

            /***** not seen any valid data? *****/

            if ( bDoTest ) {

                /***** is the pixel valid data? ****/

                int bIsNonBlack = FALSE;

                /***** loop over the colors *****/

                int iColor;
                for (iColor = 0; iColor < (int)poColors->size(); iColor++) {

                    Color oColor = (*poColors)[iColor];

                    bIsNonBlack = FALSE;

                    /***** loop over the bands *****/

                    int iBand;
                    for( iBand = 0; iBand < nSrcBands; iBand++ )
                    {
                        int nPix = pabyLine[i * nDstBands + iBand];

                        if( oColor[iBand] - nPix > nNearDist ||
                           nPix > nNearDist + oColor[iBand] )
                        {
                            bIsNonBlack = TRUE;
                            break;
                        }
                    }
                    
                    if (bIsNonBlack == FALSE)
                        break;          
                }

                if (bIsNonBlack) {

                    /***** use nNonBlackPixels in grey areas  *****/
                    /***** from the verical pass's grey areas ****/

                    if( panLastLineCounts[i] <= nMaxNonBlack )
                        nNonBlackPixels = panLastLineCounts[i];
                    else 
                        nNonBlackPixels++;
                }

                if( nNonBlackPixels > nMaxNonBlack ) {
                    bDoTest = FALSE;
                    continue;
                }

                /***** replace the pixel values *****/

                int iBand;
                for( iBand = 0; iBand < nSrcBands; iBand++ )
                    pabyLine[i * nDstBands + iBand] = nReplacevalue;

                /***** alpha *****/

                if( nDstBands > nSrcBands )
                    pabyLine[i * nDstBands + nDstBands - 1] = 0;

                /***** mask *****/

                if (pabyMask != NULL)
                    pabyMask[i] = 0;
            }

            /***** seen valid data but test if the *****/
            /***** vertical pass saw any non valid data *****/

            else if( panLastLineCounts[i] == 0 ) {
                bDoTest = TRUE;
                nNonBlackPixels = 0;
            }        
        }
    }

}


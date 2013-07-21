/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Utilities
 * Purpose:  Commandline application to build overviews. 
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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

#include "gdal_priv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage(const char* pszErrorMsg = NULL)

{
    printf( "Usage: gdaladdo [-r {nearest,average,gauss,cubic,average_mp,average_magphase,mode}]\n"
            "                [-ro] [-clean] [-q] [--help-general] filename levels\n"
            "\n"
            "  -r : choice of resampling method (default: nearest)\n"
            "  -ro : open the dataset in read-only mode, in order to generate\n"
            "        external overview (for GeoTIFF datasets especially)\n"
            "  -clean : remove all overviews\n"
            "  -q : turn off progress display\n" 
            "  -b : band to create overview (if not set overviews will be created for all bands)\n"
            "  filename: The file to build overviews for (or whose overviews must be removed).\n"
            "  levels: A list of integral overview levels to build. Ignored with -clean option.\n"
            "\n"
            "Usefull configuration variables :\n"
            "  --config USE_RRD YES : Use Erdas Imagine format (.aux) as overview format.\n"
            "Below, only for external overviews in GeoTIFF format:\n"
            "  --config COMPRESS_OVERVIEW {JPEG,LZW,PACKBITS,DEFLATE} : TIFF compression\n"
            "  --config PHOTOMETRIC_OVERVIEW {RGB,YCBCR,...} : TIFF photometric interp.\n"
            "  --config INTERLEAVE_OVERVIEW {PIXEL|BAND} : TIFF interleaving method\n"
            "  --config BIGTIFF_OVERVIEW {IF_NEEDED|IF_SAFER|YES|NO} : is BigTIFF used\n"
            "\n"
            "Examples:\n"
            " %% gdaladdo -r average abc.tif 2 4 8 16\n"
            " %% gdaladdo --config COMPRESS_OVERVIEW JPEG\n"
            "             --config PHOTOMETRIC_OVERVIEW YCBCR\n"
            "             --config INTERLEAVE_OVERVIEW PIXEL -ro abc.tif 2 4 8 16\n");

    if( pszErrorMsg != NULL )
        fprintf(stderr, "\nFAILURE: %s\n", pszErrorMsg);

    exit( 1 );
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

#define CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(nExtraArg) \
    do { if (iArg + nExtraArg >= nArgc) \
        Usage(CPLSPrintf("%s option requires %d argument(s)", papszArgv[iArg], nExtraArg)); } while(0)

int main( int nArgc, char ** papszArgv )

{
    GDALDatasetH     hDataset;
    const char      *pszResampling = "nearest";
    const char      *pszFilename = NULL;
    int              anLevels[1024];
    int              nLevelCount = 0;
    int              nResultStatus = 0;
    int              bReadOnly = FALSE;
    int              bClean = FALSE;
    GDALProgressFunc pfnProgress = GDALTermProgress; 
    int             *panBandList = NULL;
    int              nBandCount = 0;

    /* Check that we are running against at least GDAL 1.7 */
    /* Note to developers : if we use newer API, please change the requirement */
    if (atoi(GDALVersionInfo("VERSION_NUM")) < 1700)
    {
        fprintf(stderr, "At least, GDAL >= 1.7.0 is required for this version of %s, "
                        "which was compiled against GDAL %s\n", papszArgv[0], GDAL_RELEASE_NAME);
        exit(1);
    }

    GDALAllRegister();

    nArgc = GDALGeneralCmdLineProcessor( nArgc, &papszArgv, 0 );
    if( nArgc < 1 )
        exit( -nArgc );

/* -------------------------------------------------------------------- */
/*      Parse commandline.                                              */
/* -------------------------------------------------------------------- */
    for( int iArg = 1; iArg < nArgc; iArg++ )
    {
        if( EQUAL(papszArgv[iArg], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and is running against GDAL %s\n",
                   papszArgv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            return 0;
        }
        else if( EQUAL(papszArgv[iArg],"--help") )
            Usage();
        else if( EQUAL(papszArgv[iArg],"-r") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszResampling = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg],"-ro"))
            bReadOnly = TRUE;
        else if( EQUAL(papszArgv[iArg],"-clean"))
            bClean = TRUE;
        else if( EQUAL(papszArgv[iArg],"-q") || EQUAL(papszArgv[iArg],"-quiet") ) 
            pfnProgress = GDALDummyProgress; 
        else if( EQUAL(papszArgv[iArg],"-b"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            const char* pszBand = papszArgv[iArg+1];
            int nBand = atoi(pszBand);
            if( nBand < 1 )
            {
                printf( "Unrecognizable band number (%s).\n", papszArgv[iArg+1] );
                Usage();
                GDALDestroyDriverManager();
                exit( 2 );
            }
            iArg++;

            nBandCount++;
            panBandList = (int *) 
                CPLRealloc(panBandList, sizeof(int) * nBandCount);
            panBandList[nBandCount-1] = nBand;
        }
        else if( papszArgv[iArg][0] == '-' )
            Usage(CPLSPrintf("Unknown option name '%s'", papszArgv[iArg]));
        else if( pszFilename == NULL )
            pszFilename = papszArgv[iArg];
        else if( atoi(papszArgv[iArg]) > 0 )
            anLevels[nLevelCount++] = atoi(papszArgv[iArg]);
        else
            Usage("Too many command options.");
    }

    if( pszFilename == NULL )
        Usage("No datasource specified.");

    if( nLevelCount == 0 && !bClean )
        Usage("No overview level specified.");

/* -------------------------------------------------------------------- */
/*      Open data file.                                                 */
/* -------------------------------------------------------------------- */
    if (bReadOnly)
        hDataset = NULL;
    else
    {
        CPLPushErrorHandler( CPLQuietErrorHandler );
        hDataset = GDALOpen( pszFilename, GA_Update );
        CPLPopErrorHandler();
    }

    if( hDataset == NULL )
        hDataset = GDALOpen( pszFilename, GA_ReadOnly );

    if( hDataset == NULL )
        exit( 2 );

/* -------------------------------------------------------------------- */
/*      Clean overviews.                                                */
/* -------------------------------------------------------------------- */
    if ( bClean &&
        GDALBuildOverviews( hDataset,pszResampling, 0, 0, 
                             0, NULL, pfnProgress, NULL ) != CE_None )
    {
        printf( "Cleaning overviews failed.\n" );
        nResultStatus = 200;
    }

/* -------------------------------------------------------------------- */
/*      Generate overviews.                                             */
/* -------------------------------------------------------------------- */

    //Only HFA support selected layers
    if(nBandCount > 0)
        CPLSetConfigOption( "USE_RRD", "YES" );

    if (nLevelCount > 0 && nResultStatus == 0 &&
        GDALBuildOverviews( hDataset,pszResampling, nLevelCount, anLevels,
                             nBandCount, panBandList, pfnProgress, NULL ) != CE_None )
    {
        printf( "Overview building failed.\n" );
        nResultStatus = 100;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    GDALClose(hDataset);

    CSLDestroy( papszArgv );
    CPLFree(panBandList);
    GDALDestroyDriverManager();

    return nResultStatus;
}

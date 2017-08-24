/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  GDAL Image Translator Program
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, 2002, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_string.h"
#include "gdal_priv.h"
#include "ogr_spatialref.h"
#include "commonutils.h"
#include "gdal_utils_priv.h"

CPL_CVSID("$Id$")

/*  ******************************************************************* */
/*                               Usage()                                */
/* ******************************************************************** */

static void Usage(const char* pszErrorMsg = NULL, int bShort = TRUE) CPL_NO_RETURN;

static void Usage(const char* pszErrorMsg, int bShort)

{
    int iDr;

    printf( "Usage: gdal_translate [--help-general] [--long-usage]\n"
            "       [-ot {Byte/Int16/UInt16/UInt32/Int32/Float32/Float64/\n"
            "             CInt16/CInt32/CFloat32/CFloat64}] [-strict]\n"
            "       [-of format] [-b band] [-mask band] [-expand {gray|rgb|rgba}]\n"
            "       [-outsize xsize[%%]|0 ysize[%%]|0] [-tr xres yres]\n"
            "       [-r {nearest,bilinear,cubic,cubicspline,lanczos,average,mode}]\n"
            "       [-unscale] [-scale[_bn] [src_min src_max [dst_min dst_max]]]* [-exponent[_bn] exp_val]*\n"
            "       [-srcwin xoff yoff xsize ysize] [-epo] [-eco]\n"
            "       [-projwin ulx uly lrx lry] [-projwin_srs srs_def]\n"
            "       [-a_srs srs_def] [-a_ullr ulx uly lrx lry] [-a_nodata value]\n"
            "       [-gcp pixel line easting northing [elevation]]*\n"
            "       [-mo \"META-TAG=VALUE\"]* [-q] [-sds]\n"
            "       [-co \"NAME=VALUE\"]* [-stats] [-norat]\n"
            "       [-oo NAME=VALUE]*\n"
            "       src_dataset dst_dataset\n" );

    if( !bShort )
    {
        printf( "\n%s\n\n", GDALVersionInfo( "--version" ) );
        printf( "The following format drivers are configured and support output:\n" );
        for( iDr = 0; iDr < GDALGetDriverCount(); iDr++ )
        {
            GDALDriverH hDriver = GDALGetDriver(iDr);

            if( GDALGetMetadataItem( hDriver, GDAL_DCAP_RASTER, NULL) != NULL &&
                (GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATE, NULL ) != NULL
                || GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATECOPY, NULL ) != NULL) )
            {
                printf( "  %s: %s\n",
                        GDALGetDriverShortName( hDriver ),
                        GDALGetDriverLongName( hDriver ) );
            }
        }
    }

    if( pszErrorMsg != NULL )
        fprintf(stderr, "\nFAILURE: %s\n", pszErrorMsg);

    exit(1);
}

/************************************************************************/
/*                       GDALTranslateOptionsForBinaryNew()             */
/************************************************************************/

static GDALTranslateOptionsForBinary *GDALTranslateOptionsForBinaryNew(void)
{
    return static_cast<GDALTranslateOptionsForBinary *>(
        CPLCalloc(1, sizeof(GDALTranslateOptionsForBinary)));
}

/************************************************************************/
/*                       GDALTranslateOptionsForBinaryFree()            */
/************************************************************************/

static void GDALTranslateOptionsForBinaryFree( GDALTranslateOptionsForBinary* psOptionsForBinary )
{
    if( psOptionsForBinary )
    {
        CPLFree(psOptionsForBinary->pszSource);
        CPLFree(psOptionsForBinary->pszDest);
        CSLDestroy(psOptionsForBinary->papszOpenOptions);
        CPLFree(psOptionsForBinary->pszFormat);
        CPLFree(psOptionsForBinary);
    }
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int argc, char ** argv )

{
    GDALDatasetH    hDataset, hOutDS;
    int bUsageError;

    /* Check strict compilation and runtime library version as we use C++ API */
    if (! GDAL_CHECK_VERSION(argv[0]))
        exit(1);

    EarlySetConfigOptions(argc, argv);

/* -------------------------------------------------------------------- */
/*      Register standard GDAL drivers, and process generic GDAL        */
/*      command options.                                                */
/* -------------------------------------------------------------------- */
    GDALAllRegister();
    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
    if( argc < 1 )
        exit( -argc );

    for( int i = 0; argv != NULL && argv[i] != NULL; i++ )
    {
        if( EQUAL(argv[i], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and is running against GDAL %s\n",
                   argv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            CSLDestroy( argv );
            return 0;
        }
        else if( EQUAL(argv[i],"--help") )
        {
            Usage(NULL);
        }
        else if ( EQUAL(argv[i], "--long-usage") )
        {
            Usage(NULL, FALSE);
        }
    }

/* -------------------------------------------------------------------- */
/*      Set optimal setting for best performance with huge input VRT.   */
/*      The rationale for 450 is that typical Linux process allow       */
/*      only 1024 file descriptors per process and we need to keep some */
/*      spare for shared libraries, etc. so let's go down to 900.       */
/*      And some datasets may need 2 file descriptors, so divide by 2   */
/*      for security.                                                   */
/* -------------------------------------------------------------------- */
    if( CPLGetConfigOption("GDAL_MAX_DATASET_POOL_SIZE", NULL) == NULL )
    {
#if defined(__MACH__) && defined(__APPLE__)
        // On Mach, the default limit is 256 files per process
        // TODO We should eventually dynamically query the limit for all OS
        CPLSetConfigOption("GDAL_MAX_DATASET_POOL_SIZE", "100");
#else
        CPLSetConfigOption("GDAL_MAX_DATASET_POOL_SIZE", "450");
#endif
    }

    GDALTranslateOptionsForBinary* psOptionsForBinary = GDALTranslateOptionsForBinaryNew();
    GDALTranslateOptions *psOptions = GDALTranslateOptionsNew(argv + 1, psOptionsForBinary);
    CSLDestroy( argv );

    if( psOptions == NULL )
    {
        Usage(NULL);
    }

    if( psOptionsForBinary->pszSource == NULL )
    {
        Usage("No source dataset specified.");
    }

    if( psOptionsForBinary->pszDest == NULL )
    {
        Usage("No target dataset specified.");
    }

    if( strcmp(psOptionsForBinary->pszDest, "/vsistdout/") == 0 )
    {
        psOptionsForBinary->bQuiet = TRUE;
    }

    if( !(psOptionsForBinary->bQuiet) )
    {
        GDALTranslateOptionsSetProgress(psOptions, GDALTermProgress, NULL);
    }

    if( psOptionsForBinary->pszFormat )
    {
        GDALDriverH hDriver = GDALGetDriverByName( psOptionsForBinary->pszFormat );
        if( hDriver == NULL )
        {
            int iDr;

            fprintf(stderr, "Output driver `%s' not recognised.\n",
                    psOptionsForBinary->pszFormat);
            fprintf(stderr, "The following format drivers are configured and support output:\n" );
            for( iDr = 0; iDr < GDALGetDriverCount(); iDr++ )
            {
                hDriver = GDALGetDriver(iDr);

                if( GDALGetMetadataItem( hDriver, GDAL_DCAP_RASTER, NULL) != NULL &&
                    (GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATE, NULL ) != NULL
                    || GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATECOPY, NULL ) != NULL) )
                {
                    fprintf(stderr, "  %s: %s\n",
                            GDALGetDriverShortName( hDriver  ),
                            GDALGetDriverLongName( hDriver ) );
                }
            }

            GDALTranslateOptionsFree(psOptions);
            GDALTranslateOptionsForBinaryFree(psOptionsForBinary);

            GDALDestroyDriverManager();
            exit(1);
        }
    }

    if (!psOptionsForBinary->bQuiet && !psOptionsForBinary->bFormatExplicitlySet)
        CheckExtensionConsistency(psOptionsForBinary->pszDest, psOptionsForBinary->pszFormat);

/* -------------------------------------------------------------------- */
/*      Attempt to open source file.                                    */
/* -------------------------------------------------------------------- */

    hDataset = GDALOpenEx( psOptionsForBinary->pszSource, GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR, NULL,
                           (const char* const* )psOptionsForBinary->papszOpenOptions, NULL );

    if( hDataset == NULL )
    {
        GDALDestroyDriverManager();
        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Handle subdatasets.                                             */
/* -------------------------------------------------------------------- */
    if( !psOptionsForBinary->bCopySubDatasets
        && GDALGetRasterCount(hDataset) == 0
        && CSLCount(GDALGetMetadata( hDataset, "SUBDATASETS" )) > 0 )
    {
        fprintf( stderr,
                 "Input file contains subdatasets. Please, select one of them for reading.\n" );
        GDALClose( hDataset );
        GDALDestroyDriverManager();
        exit( 1 );
    }

    if( psOptionsForBinary->bCopySubDatasets &&
        CSLCount(GDALGetMetadata( hDataset, "SUBDATASETS" )) > 0 )
    {
        char **papszSubdatasets = GDALGetMetadata(hDataset,"SUBDATASETS");
        char *pszSubDest = static_cast<char *>(
            CPLMalloc(strlen(psOptionsForBinary->pszDest) + 32));
        int i;

        CPLString osPath = CPLGetPath(psOptionsForBinary->pszDest);
        CPLString osBasename = CPLGetBasename(psOptionsForBinary->pszDest);
        CPLString osExtension = CPLGetExtension(psOptionsForBinary->pszDest);
        CPLString osTemp;

        const char* pszFormat = NULL;
        if ( CSLCount(papszSubdatasets)/2 < 10 )
        {
            pszFormat = "%s_%d";
        }
        else if ( CSLCount(papszSubdatasets)/2 < 100 )
        {
            pszFormat = "%s_%002d";
        }
        else
        {
            pszFormat = "%s_%003d";
        }

        const char* pszDest = pszSubDest;

        for( i = 0; papszSubdatasets[i] != NULL; i += 2 )
        {
            char* pszSource = CPLStrdup(strstr(papszSubdatasets[i],"=")+1);
            osTemp = CPLSPrintf( pszFormat, osBasename.c_str(), i/2 + 1 );
            osTemp = CPLFormFilename( osPath, osTemp, osExtension );
            strcpy( pszSubDest, osTemp.c_str() );
            hDataset = GDALOpenEx( pszSource, GDAL_OF_RASTER, NULL,
                           (const char* const* )psOptionsForBinary->papszOpenOptions, NULL );
            CPLFree(pszSource);
            if( !psOptionsForBinary->bQuiet )
                printf("Input file size is %d, %d\n", GDALGetRasterXSize(hDataset), GDALGetRasterYSize(hDataset));
            hOutDS = GDALTranslate(pszDest, hDataset, psOptions, &bUsageError);
            if(bUsageError == TRUE)
                Usage();
            if (hOutDS == NULL)
                break;
            GDALClose(hOutDS);
        }

        GDALClose(hDataset);
        GDALTranslateOptionsFree(psOptions);
        GDALTranslateOptionsForBinaryFree(psOptionsForBinary);
        CPLFree(pszSubDest);

        GDALDestroyDriverManager();
        return 0;
    }

    if( !psOptionsForBinary->bQuiet )
        printf("Input file size is %d, %d\n", GDALGetRasterXSize(hDataset), GDALGetRasterYSize(hDataset));

    hOutDS = GDALTranslate(psOptionsForBinary->pszDest, hDataset, psOptions, &bUsageError);
    if(bUsageError == TRUE)
        Usage();
    int nRetCode = (hOutDS) ? 0 : 1;

    /* Close hOutDS before hDataset for the -f VRT case */
    GDALClose(hOutDS);
    GDALClose(hDataset);
    GDALTranslateOptionsFree(psOptions);
    GDALTranslateOptionsForBinaryFree(psOptionsForBinary);

    GDALDestroyDriverManager();

    return nRetCode;
}

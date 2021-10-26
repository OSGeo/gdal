/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Command line application to build overviews.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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
#include "gdal_version.h"
#include "gdal_priv.h"
#include "commonutils.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage( const char* pszErrorMsg = nullptr )

{
    printf("Usage: gdaladdo [-r {nearest,average,rms,gauss,cubic,cubicspline,lanczos,average_mp,average_magphase,mode}]\n"
           "                [-ro] [-clean] [-q] [-oo NAME=VALUE]* [-minsize val]\n"
           "                [--help-general] filename [levels]\n"
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
           "Useful configuration variables :\n"
           "  --config USE_RRD YES : Use Erdas Imagine format (.aux) as overview format.\n"
           "Below, only for external overviews in GeoTIFF format:\n"
           "  --config COMPRESS_OVERVIEW {JPEG,LZW,PACKBITS,DEFLATE} : TIFF compression\n"
           "  --config PHOTOMETRIC_OVERVIEW {RGB,YCBCR,...} : TIFF photometric interp.\n"
           "  --config INTERLEAVE_OVERVIEW {PIXEL|BAND} : TIFF interleaving method\n"
           "  --config BIGTIFF_OVERVIEW {IF_NEEDED|IF_SAFER|YES|NO} : is BigTIFF used\n"
           "\n"
           "Examples:\n"
           " %% gdaladdo -r average abc.tif\n"
           " %% gdaladdo --config COMPRESS_OVERVIEW JPEG\n"
           "            --config PHOTOMETRIC_OVERVIEW YCBCR\n"
           "            --config INTERLEAVE_OVERVIEW PIXEL -ro abc.tif\n");

    if( pszErrorMsg != nullptr )
        fprintf(stderr, "\nFAILURE: %s\n", pszErrorMsg);

    exit(1);
}

/************************************************************************/
/*                        GDALAddoErrorHandler()                        */
/************************************************************************/

class GDALError
{
  public:
      CPLErr            m_eErr;
      CPLErrorNum       m_errNum;
      CPLString         m_osMsg;

      GDALError( CPLErr eErr = CE_None, CPLErrorNum errNum= CPLE_None,
                 const char * pszMsg = "" ) :
          m_eErr(eErr), m_errNum(errNum), m_osMsg(pszMsg ? pszMsg : "")
      {
      }
};

std::vector<GDALError> aoErrors;

static void CPL_STDCALL GDALAddoErrorHandler( CPLErr eErr, CPLErrorNum errNum,
                                              const char * pszMsg )
{
    aoErrors.push_back(GDALError(eErr, errNum, pszMsg));
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

#define CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(nExtraArg) \
    do { if (iArg + nExtraArg >= nArgc) \
        Usage(CPLSPrintf("%s option requires %d argument(s)", \
                         papszArgv[iArg], nExtraArg)); } while(false)

MAIN_START(nArgc, papszArgv)

{
    // Check that we are running against at least GDAL 1.7.
    // Note to developers: if we use newer API, please change the requirement.
    if (atoi(GDALVersionInfo("VERSION_NUM")) < 1700)
    {
        fprintf(stderr,
                "At least, GDAL >= 1.7.0 is required for this version of %s, "
                "which was compiled against GDAL %s\n",
                papszArgv[0], GDAL_RELEASE_NAME);
        exit(1);
    }

    GDALAllRegister();

    nArgc = GDALGeneralCmdLineProcessor(nArgc, &papszArgv, 0);
    if( nArgc < 1 )
        exit(-nArgc);

    const char *pszResampling = "nearest";
    const char *pszFilename = nullptr;
    int anLevels[1024] = {};
    int nLevelCount = 0;
    int nResultStatus = 0;
    bool bReadOnly = false;
    bool bClean = false;
    GDALProgressFunc pfnProgress = GDALTermProgress;
    int *panBandList = nullptr;
    int nBandCount = 0;
    char **papszOpenOptions = nullptr;
    int nMinSize = 256;

/* -------------------------------------------------------------------- */
/*      Parse command line.                                              */
/* -------------------------------------------------------------------- */
    for( int iArg = 1; iArg < nArgc; iArg++ )
    {
        if( EQUAL(papszArgv[iArg], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and "
                   "is running against GDAL %s\n",
                   papszArgv[0], GDAL_RELEASE_NAME,
                   GDALVersionInfo("RELEASE_NAME"));
            CSLDestroy(papszArgv);
            return 0;
        }
        else if( EQUAL(papszArgv[iArg], "--help") )
        {
            Usage();
        }
        else if( EQUAL(papszArgv[iArg], "-r") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszResampling = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg], "-ro"))
        {
            bReadOnly = true;
        }
        else if( EQUAL(papszArgv[iArg], "-clean"))
        {
            bClean = true;
        }
        else if( EQUAL(papszArgv[iArg], "-q") ||
                 EQUAL(papszArgv[iArg], "-quiet") )
        {
            pfnProgress = GDALDummyProgress;
        }
        else if( EQUAL(papszArgv[iArg], "-b"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            const char* pszBand = papszArgv[iArg+1];
            const int nBand = atoi(pszBand);
            if( nBand < 1 )
            {
                printf("Unrecognizable band number (%s).\n", papszArgv[iArg+1]);
                Usage();
            }
            iArg++;

            nBandCount++;
            panBandList = static_cast<int *>(
                CPLRealloc(panBandList, sizeof(int) * nBandCount));
            panBandList[nBandCount-1] = nBand;
        }
        else if( EQUAL(papszArgv[iArg], "-oo") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            papszOpenOptions =
                CSLAddString(papszOpenOptions, papszArgv[++iArg]);
        }
        else if( EQUAL(papszArgv[iArg], "-minsize") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            nMinSize = atoi(papszArgv[++iArg]);
        }
        else if( papszArgv[iArg][0] == '-' )
        {
            Usage(CPLSPrintf("Unknown option name '%s'", papszArgv[iArg]));
        }
        else if( pszFilename == nullptr )
        {
            pszFilename = papszArgv[iArg];
        }
        else if( atoi(papszArgv[iArg]) > 0 &&
                 static_cast<size_t>(nLevelCount) < CPL_ARRAYSIZE(anLevels) )
        {
            anLevels[nLevelCount++] = atoi(papszArgv[iArg]);
            if( anLevels[nLevelCount-1] == 1 )
            {
                printf(
                    "Warning: Overview with subsampling factor of 1 requested. "
                    "This will copy the full resolution dataset in the "
                    "overview!\n");
            }
        }
        else
        {
            Usage("Too many command options.");
        }
    }

    if( pszFilename == nullptr )
        Usage("No datasource specified.");

/* -------------------------------------------------------------------- */
/*      Open data file.                                                 */
/* -------------------------------------------------------------------- */
    GDALDatasetH hDataset = nullptr;
    if( !bReadOnly )
    {
        CPLPushErrorHandler(GDALAddoErrorHandler);
        CPLSetCurrentErrorHandlerCatchDebug(FALSE);
        hDataset = GDALOpenEx(pszFilename, GDAL_OF_RASTER | GDAL_OF_UPDATE,
                              nullptr, papszOpenOptions, nullptr);
        CPLPopErrorHandler();
        if( hDataset != nullptr )
        {
            for(size_t i=0;i<aoErrors.size();i++)
            {
                CPLError(aoErrors[i].m_eErr, aoErrors[i].m_errNum, "%s",
                         aoErrors[i].m_osMsg.c_str());
            }
        }
    }

    if( hDataset == nullptr )
        hDataset =
            GDALOpenEx(pszFilename, GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
                       nullptr, papszOpenOptions, nullptr);

    CSLDestroy(papszOpenOptions);
    papszOpenOptions = nullptr;

    if( hDataset == nullptr )
        exit(2);

/* -------------------------------------------------------------------- */
/*      Clean overviews.                                                */
/* -------------------------------------------------------------------- */
    if ( bClean )
    {
        if( GDALBuildOverviews(hDataset,pszResampling, 0, nullptr,
                               0, nullptr, pfnProgress, nullptr) != CE_None )
        {
            printf("Cleaning overviews failed.\n");
            nResultStatus = 200;
        }
    }
    else
    {
/* -------------------------------------------------------------------- */
/*      Generate overviews.                                             */
/* -------------------------------------------------------------------- */

        if( nLevelCount == 0 )
        {
            const int nXSize = GDALGetRasterXSize(hDataset);
            const int nYSize = GDALGetRasterYSize(hDataset);
            int nOvrFactor = 1;
            while( DIV_ROUND_UP(nXSize, nOvrFactor) > nMinSize ||
                   DIV_ROUND_UP(nYSize, nOvrFactor) > nMinSize )
            {
                nOvrFactor *= 2;
                anLevels[nLevelCount++] = nOvrFactor;
            }
        }

        // Only HFA supports selected layers
        if( nBandCount > 0 )
            CPLSetConfigOption("USE_RRD", "YES");

        if( nLevelCount > 0 &&
            GDALBuildOverviews(hDataset,pszResampling, nLevelCount, anLevels,
                               nBandCount, panBandList, pfnProgress,
                               nullptr) != CE_None)
        {
            printf("Overview building failed.\n");
            nResultStatus = 100;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    GDALClose(hDataset);

    CSLDestroy(papszArgv);
    CPLFree(panBandList);
    GDALDestroyDriverManager();

    return nResultStatus;
}
MAIN_END

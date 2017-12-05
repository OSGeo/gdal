/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Helper code to implement overview and mask support for many
 *           drivers with no inherent format support.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, 2007, Frank Warmerdam
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
#include "gdal_priv.h"

#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <string>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"

CPL_CVSID("$Id$")

//! @cond Doxygen_Suppress
/************************************************************************/
/*                        GDALDefaultOverviews()                        */
/************************************************************************/

GDALDefaultOverviews::GDALDefaultOverviews() :
    poDS(NULL),
    poODS(NULL),
    bOvrIsAux(false),
    bCheckedForMask(false),
    bOwnMaskDS(false),
    poMaskDS(NULL),
    poBaseDS(NULL),
    bCheckedForOverviews(FALSE),
    pszInitName(NULL),
    bInitNameIsOVR(false),
    papszInitSiblingFiles(NULL)
{}

/************************************************************************/
/*                       ~GDALDefaultOverviews()                        */
/************************************************************************/

GDALDefaultOverviews::~GDALDefaultOverviews()

{
    CPLFree( pszInitName );
    CSLDestroy( papszInitSiblingFiles );

    CloseDependentDatasets();
}

/************************************************************************/
/*                       CloseDependentDatasets()                       */
/************************************************************************/

int GDALDefaultOverviews::CloseDependentDatasets()
{
    bool bHasDroppedRef = false;
    if( poODS != NULL )
    {
        bHasDroppedRef = true;
        poODS->FlushCache();
        GDALClose( poODS );
        poODS = NULL;
    }

    if( poMaskDS != NULL )
    {
        if( bOwnMaskDS )
        {
            bHasDroppedRef = true;
            poMaskDS->FlushCache();
            GDALClose( poMaskDS );
        }
        poMaskDS = NULL;
    }

    return bHasDroppedRef;
}

/************************************************************************/
/*                           IsInitialized()                            */
/*                                                                      */
/*      Returns TRUE if we are initialized.                             */
/************************************************************************/

int GDALDefaultOverviews::IsInitialized()

{
    OverviewScan();
    return poDS != NULL;
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

void GDALDefaultOverviews::Initialize( GDALDataset *poDSIn,
                                       const char * pszBasename,
                                       char **papszSiblingFiles,
                                       int bNameIsOVR )

{
    poDS = poDSIn;

/* -------------------------------------------------------------------- */
/*      If we were already initialized, destroy the old overview        */
/*      file handle.                                                    */
/* -------------------------------------------------------------------- */
    if( poODS != NULL )
    {
        GDALClose( poODS );
        poODS = NULL;

        CPLDebug(
            "GDAL",
            "GDALDefaultOverviews::Initialize() called twice - "
            "this is odd and perhaps dangerous!" );
    }

/* -------------------------------------------------------------------- */
/*      Store the initialization information for later use in           */
/*      OverviewScan()                                                  */
/* -------------------------------------------------------------------- */
    bCheckedForOverviews = FALSE;

    CPLFree( pszInitName );
    pszInitName = NULL;
    if( pszBasename != NULL )
        pszInitName = CPLStrdup(pszBasename);
    bInitNameIsOVR = CPL_TO_BOOL(bNameIsOVR);

    CSLDestroy( papszInitSiblingFiles );
    papszInitSiblingFiles = NULL;
    if( papszSiblingFiles != NULL )
        papszInitSiblingFiles = CSLDuplicate(papszSiblingFiles);
}

/************************************************************************/
/*                         TransferSiblingFiles()                       */
/*                                                                      */
/*      Contrary to Initialize(), this sets papszInitSiblingFiles but   */
/*      without duplicating the passed list. Which must be              */
/*      "de-allocatable" with CSLDestroy()                              */
/************************************************************************/

void GDALDefaultOverviews::TransferSiblingFiles( char** papszSiblingFiles )
{
    CSLDestroy( papszInitSiblingFiles );
    papszInitSiblingFiles = papszSiblingFiles;
}

/************************************************************************/
/*                            OverviewScan()                            */
/*                                                                      */
/*      This is called to scan for overview files when a first          */
/*      request is made with regard to overviews.  It uses the          */
/*      pszInitName, bInitNameIsOVR and papszInitSiblingFiles           */
/*      information that was stored at Initialization() time.           */
/************************************************************************/

void GDALDefaultOverviews::OverviewScan()

{
    if( bCheckedForOverviews || poDS == NULL )
        return;

    bCheckedForOverviews = true;

    CPLDebug( "GDAL", "GDALDefaultOverviews::OverviewScan()" );

/* -------------------------------------------------------------------- */
/*      Open overview dataset if it exists.                             */
/* -------------------------------------------------------------------- */
    if( pszInitName == NULL )
        pszInitName = CPLStrdup(poDS->GetDescription());

    if( !EQUAL(pszInitName,":::VIRTUAL:::") &&
        GDALCanFileAcceptSidecarFile(pszInitName) )
    {
        if( bInitNameIsOVR )
            osOvrFilename = pszInitName;
        else
            osOvrFilename.Printf( "%s.ovr", pszInitName );

        std::vector<char> achOvrFilename;
        achOvrFilename.resize(osOvrFilename.size() + 1);
        memcpy(&(achOvrFilename[0]),
               osOvrFilename.c_str(),
               osOvrFilename.size() + 1);
        bool bExists = CPL_TO_BOOL(
            CPLCheckForFile( &achOvrFilename[0], papszInitSiblingFiles ) );
        osOvrFilename = &achOvrFilename[0];

#if !defined(WIN32)
        if( !bInitNameIsOVR && !bExists && !papszInitSiblingFiles )
        {
            osOvrFilename.Printf( "%s.OVR", pszInitName );
            memcpy(&(achOvrFilename[0]),
                   osOvrFilename.c_str(),
                   osOvrFilename.size() + 1);
            bExists = CPL_TO_BOOL(
                CPLCheckForFile( &achOvrFilename[0], papszInitSiblingFiles ) );
            osOvrFilename = &achOvrFilename[0];
            if( !bExists )
                osOvrFilename.Printf( "%s.ovr", pszInitName );
        }
#endif

        if( bExists )
        {
           poODS = static_cast<GDALDataset *>( GDALOpenEx(
                osOvrFilename,
                GDAL_OF_RASTER |
                (poDS->GetAccess() == GA_Update ? GDAL_OF_UPDATE : 0),
                NULL, NULL, papszInitSiblingFiles ) );
        }
    }

/* -------------------------------------------------------------------- */
/*      We didn't find that, so try and find a corresponding aux        */
/*      file.  Check that we are the dependent file of the aux          */
/*      file.                                                           */
/*                                                                      */
/*      We only use the .aux file for overviews if they already have    */
/*      overviews existing, or if USE_RRD is set true.                  */
/* -------------------------------------------------------------------- */
    if( !poODS && !EQUAL(pszInitName,":::VIRTUAL:::") &&
        GDALCanFileAcceptSidecarFile(pszInitName) )
    {
        bool bTryFindAssociatedAuxFile = true;
        if( papszInitSiblingFiles )
        {
            CPLString osAuxFilename = CPLResetExtension( pszInitName, "aux");
            int iSibling = CSLFindString( papszInitSiblingFiles,
                                          CPLGetFilename(osAuxFilename) );
            if( iSibling < 0 )
            {
                osAuxFilename = pszInitName;
                osAuxFilename += ".aux";
                iSibling = CSLFindString( papszInitSiblingFiles,
                                        CPLGetFilename(osAuxFilename) );
                if( iSibling < 0 )
                    bTryFindAssociatedAuxFile = false;
            }
        }

        if( bTryFindAssociatedAuxFile )
        {
            poODS = GDALFindAssociatedAuxFile( pszInitName, poDS->GetAccess(),
                                            poDS );
        }

        if( poODS )
        {
            const bool bUseRRD = CPLTestBool(CPLGetConfigOption("USE_RRD","NO"));

            bOvrIsAux = true;
            if( GetOverviewCount(1) == 0 && !bUseRRD )
            {
                bOvrIsAux = false;
                GDALClose( poODS );
                poODS = NULL;
            }
            else
            {
                osOvrFilename = poODS->GetDescription();
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      If we still don't have an overview, check to see if we have     */
/*      overview metadata referencing a remote (i.e. proxy) or local    */
/*      subdataset overview dataset.                                    */
/* -------------------------------------------------------------------- */
    if( poODS == NULL )
    {
        const char *pszProxyOvrFilename =
            poDS->GetMetadataItem( "OVERVIEW_FILE", "OVERVIEWS" );

        if( pszProxyOvrFilename != NULL )
        {
            if( STARTS_WITH_CI(pszProxyOvrFilename, ":::BASE:::") )
            {
                const CPLString osPath = CPLGetPath(poDS->GetDescription());

                osOvrFilename =
                    CPLFormFilename( osPath, pszProxyOvrFilename+10, NULL );
            }
            else
            {
                osOvrFilename = pszProxyOvrFilename;
            }

            CPLPushErrorHandler(CPLQuietErrorHandler);
            poODS = static_cast<GDALDataset *>(GDALOpen(osOvrFilename, poDS->GetAccess()));
            CPLPopErrorHandler();
        }
    }

/* -------------------------------------------------------------------- */
/*      If we have an overview dataset, then mark all the overviews     */
/*      with the base dataset  Used later for finding overviews         */
/*      masks.  Uggg.                                                   */
/* -------------------------------------------------------------------- */
    if( poODS )
    {
        const int nOverviewCount = GetOverviewCount(1);

        for( int iOver = 0; iOver < nOverviewCount; iOver++ )
        {
            GDALRasterBand * const poBand = GetOverview( 1, iOver );
            GDALDataset * const poOverDS = poBand != NULL ?
                poBand->GetDataset() : NULL;

            if( poOverDS != NULL )
            {
                poOverDS->oOvManager.poBaseDS = poDS;
                poOverDS->oOvManager.poDS = poOverDS;
            }
        }
    }
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int GDALDefaultOverviews::GetOverviewCount( int nBand )

{
    if( poODS == NULL || nBand < 1 || nBand > poODS->GetRasterCount() )
        return 0;

    GDALRasterBand * poBand = poODS->GetRasterBand( nBand );
    if( poBand == NULL )
        return 0;

    if( bOvrIsAux )
        return poBand->GetOverviewCount();

    return poBand->GetOverviewCount() + 1;
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *
GDALDefaultOverviews::GetOverview( int nBand, int iOverview )

{
    if( poODS == NULL || nBand < 1 || nBand > poODS->GetRasterCount() )
        return NULL;

    GDALRasterBand * const poBand = poODS->GetRasterBand( nBand );
    if( poBand == NULL )
        return NULL;

    if( bOvrIsAux )
        return poBand->GetOverview( iOverview );

    // TIFF case, base is overview 0.
    if( iOverview == 0 )
        return poBand;

    if( iOverview-1 >= poBand->GetOverviewCount() )
        return NULL;

    return poBand->GetOverview( iOverview-1 );
}

/************************************************************************/
/*                         GDALOvLevelAdjust()                          */
/*                                                                      */
/*      Some overview levels cannot be achieved closely enough to be    */
/*      recognised as the desired overview level.  This function        */
/*      will adjust an overview level to one that is achievable on      */
/*      the given raster size.                                          */
/*                                                                      */
/*      For instance a 1200x1200 image on which a 256 level overview    */
/*      is request will end up generating a 5x5 overview.  However,     */
/*      this will appear to the system be a level 240 overview.         */
/*      This function will adjust 256 to 240 based on knowledge of      */
/*      the image size.                                                 */
/************************************************************************/

int GDALOvLevelAdjust( int nOvLevel, int nXSize )

{
    int nOXSize = (nXSize + nOvLevel - 1) / nOvLevel;

    return (int) (0.5 + nXSize / (double) nOXSize);
}

int GDALOvLevelAdjust2( int nOvLevel, int nXSize, int nYSize )

{
    // Select the larger dimension to have increased accuracy, but
    // with a slight preference to x even if (a bit) smaller than y
    // in an attempt to behave closer as previous behaviour.
    if( nXSize >= nYSize / 2 && !(nXSize < nYSize && nXSize < nOvLevel) )
    {
        const int nOXSize = (nXSize + nOvLevel - 1) / nOvLevel;

        return static_cast<int>(0.5 + nXSize / static_cast<double>(nOXSize));
    }

    const int nOYSize = (nYSize + nOvLevel - 1) / nOvLevel;

    return static_cast<int>(0.5 + nYSize / static_cast<double>(nOYSize));
}

/************************************************************************/
/*                         GDALComputeOvFactor()                        */
/************************************************************************/

int GDALComputeOvFactor( int nOvrXSize, int nRasterXSize,
                         int nOvrYSize, int nRasterYSize )
{
    // Select the larger dimension to have increased accuracy, but
    // with a slight preference to x even if (a bit) smaller than y
    // in an attempt to behave closer as previous behaviour.
    if( nRasterXSize >= nRasterYSize / 2 )
    {
        return static_cast<int>(0.5 + nRasterXSize / static_cast<double>(nOvrXSize));
    }

    return static_cast<int>(0.5 + nRasterYSize / static_cast<double>(nOvrYSize));
}

/************************************************************************/
/*                           CleanOverviews()                           */
/*                                                                      */
/*      Remove all existing overviews.                                  */
/************************************************************************/

CPLErr GDALDefaultOverviews::CleanOverviews()

{
    // Anything to do?
    if( poODS == NULL )
        return CE_None;

    // Delete the overview file(s).
    GDALDriver *poOvrDriver = poODS->GetDriver();
    GDALClose( poODS );
    poODS = NULL;

    const CPLErr eErr = poOvrDriver != NULL ?
        poOvrDriver->Delete( osOvrFilename ) : CE_None;

    // Reset the saved overview filename.
    if( !EQUAL(poDS->GetDescription(),":::VIRTUAL:::") )
    {
        const bool bUseRRD = CPLTestBool(CPLGetConfigOption("USE_RRD","NO"));

        if( bUseRRD )
            osOvrFilename = CPLResetExtension( poDS->GetDescription(), "aux" );
        else
            osOvrFilename.Printf( "%s.ovr", poDS->GetDescription() );
    }
    else
    {
        osOvrFilename = "";
    }

    return eErr;
}

/************************************************************************/
/*                      BuildOverviewsSubDataset()                      */
/************************************************************************/

CPLErr
GDALDefaultOverviews::BuildOverviewsSubDataset(
    const char * pszPhysicalFile,
    const char * pszResampling,
    int nOverviews, int * panOverviewList,
    int nBands, int * panBandList,
    GDALProgressFunc pfnProgress, void * pProgressData)

{
    if( osOvrFilename.length() == 0 && nOverviews > 0 )
    {
        VSIStatBufL sStatBuf;

        int iSequence = 0;  // Used after for.
        for( iSequence = 0; iSequence < 100; iSequence++ )
        {
            osOvrFilename.Printf( "%s_%d.ovr", pszPhysicalFile, iSequence );
            if( VSIStatExL( osOvrFilename, &sStatBuf,
                            VSI_STAT_EXISTS_FLAG ) != 0 )
            {
                CPLString osAdjustedOvrFilename;

                if( poDS->GetMOFlags() & GMO_PAM_CLASS )
                {
                    osAdjustedOvrFilename.Printf(
                        ":::BASE:::%s_%d.ovr",
                        CPLGetFilename(pszPhysicalFile),
                        iSequence );
                }
                else
                {
                    osAdjustedOvrFilename = osOvrFilename;
                }

                poDS->SetMetadataItem( "OVERVIEW_FILE",
                                       osAdjustedOvrFilename,
                                       "OVERVIEWS" );
                break;
            }
        }

        if( iSequence == 100 )
            osOvrFilename = "";
    }

    return BuildOverviews( NULL, pszResampling, nOverviews, panOverviewList,
                           nBands, panBandList, pfnProgress, pProgressData );
}

/************************************************************************/
/*                           BuildOverviews()                           */
/************************************************************************/

CPLErr
GDALDefaultOverviews::BuildOverviews(
    const char * pszBasename,
    const char * pszResampling,
    int nOverviews, int * panOverviewList,
    int nBands, int * panBandList,
    GDALProgressFunc pfnProgress, void * pProgressData)

{
    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

    if( nOverviews == 0 )
        return CleanOverviews();

/* -------------------------------------------------------------------- */
/*      If we don't already have an overview file, we need to decide    */
/*      what format to use.                                             */
/* -------------------------------------------------------------------- */
    if( poODS == NULL )
    {
        bOvrIsAux = CPLTestBool(CPLGetConfigOption( "USE_RRD", "NO" ));
        if( bOvrIsAux )
        {
            osOvrFilename = CPLResetExtension(poDS->GetDescription(),"aux");

            VSIStatBufL sStatBuf;
            if( VSIStatExL( osOvrFilename, &sStatBuf,
                            VSI_STAT_EXISTS_FLAG ) == 0 )
                osOvrFilename.Printf( "%s.aux", poDS->GetDescription() );
        }
    }
/* -------------------------------------------------------------------- */
/*      If we already have the overviews open, but they are             */
/*      read-only, then try and reopen them read-write.                 */
/* -------------------------------------------------------------------- */
    else if( poODS->GetAccess() == GA_ReadOnly )
    {
        GDALClose( poODS );
        poODS = static_cast<GDALDataset *>(
            GDALOpen( osOvrFilename, GA_Update ));
        if( poODS == NULL )
            return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Our TIFF overview support currently only works safely if all    */
/*      bands are handled at the same time.                             */
/* -------------------------------------------------------------------- */
    if( !bOvrIsAux && nBands != poDS->GetRasterCount() )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Generation of overviews in external TIFF currently only "
                  "supported when operating on all bands.  "
                  "Operation failed." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      If a basename is provided, use it to override the internal      */
/*      overview filename.                                              */
/* -------------------------------------------------------------------- */
    if( pszBasename == NULL && osOvrFilename.length() == 0  )
        pszBasename = poDS->GetDescription();

    if( pszBasename != NULL )
    {
        if( bOvrIsAux )
            osOvrFilename.Printf( "%s.aux", pszBasename );
        else
            osOvrFilename.Printf( "%s.ovr", pszBasename );
    }

/* -------------------------------------------------------------------- */
/*      Establish which of the overview levels we already have, and     */
/*      which are new.  We assume that band 1 of the file is            */
/*      representative.                                                 */
/* -------------------------------------------------------------------- */
    GDALRasterBand *poBand = poDS->GetRasterBand( 1 );

    int nNewOverviews = 0;
    int *panNewOverviewList = static_cast<int *>(
        CPLCalloc(sizeof(int), nOverviews) );
    double dfAreaNewOverviews = 0;
    double dfAreaRefreshedOverviews = 0;
    for( int i = 0; i < nOverviews && poBand != NULL; i++ )
    {
        for( int j = 0; j < poBand->GetOverviewCount(); j++ )
        {
            GDALRasterBand * poOverview = poBand->GetOverview( j );
            if( poOverview == NULL )
                continue;

            int nOvFactor =
                GDALComputeOvFactor(poOverview->GetXSize(),
                                    poBand->GetXSize(),
                                    poOverview->GetYSize(),
                                    poBand->GetYSize());

            if( nOvFactor == panOverviewList[i]
                || nOvFactor == GDALOvLevelAdjust2( panOverviewList[i],
                                                   poBand->GetXSize(),
                                                   poBand->GetYSize() ) )
            {
                panOverviewList[i] *= -1;
            }
        }

        const double dfArea = 1.0 / (panOverviewList[i] * panOverviewList[i]);
        dfAreaRefreshedOverviews += dfArea;
        if( panOverviewList[i] > 0 )
        {
            dfAreaNewOverviews += dfArea;
            panNewOverviewList[nNewOverviews++] = panOverviewList[i];
        }
    }

/* -------------------------------------------------------------------- */
/*      Build band list.                                                */
/* -------------------------------------------------------------------- */
    GDALRasterBand **pahBands = static_cast<GDALRasterBand **>(
        CPLCalloc(sizeof(GDALRasterBand *), nBands) );
    for( int i = 0; i < nBands; i++ )
        pahBands[i] = poDS->GetRasterBand( panBandList[i] );

/* -------------------------------------------------------------------- */
/*      Build new overviews - Imagine.  Keep existing file open if      */
/*      we have it.  But mark all overviews as in need of               */
/*      regeneration, since HFAAuxBuildOverviews() doesn't actually     */
/*      produce the imagery.                                            */
/* -------------------------------------------------------------------- */

    CPLErr eErr = CE_None;

    void* pScaledProgress = GDALCreateScaledProgress(
            0, dfAreaNewOverviews / dfAreaRefreshedOverviews,
            pfnProgress, pProgressData );
    if( bOvrIsAux )
    {
        if( nNewOverviews == 0 )
        {
            /* if we call HFAAuxBuildOverviews() with nNewOverviews == 0 */
            /* because that there's no new, this will wipe existing */
            /* overviews (#4831) */
            // eErr = CE_None;
        }
        else
        {
            eErr = HFAAuxBuildOverviews( osOvrFilename, poDS, &poODS,
                                     nBands, panBandList,
                                     nNewOverviews, panNewOverviewList,
                                     pszResampling,
                                     GDALScaledProgress, pScaledProgress );
        }
        for( int j = 0; j < nOverviews; j++ )
        {
            if( panOverviewList[j] > 0 )
                panOverviewList[j] *= -1;
        }
    }

/* -------------------------------------------------------------------- */
/*      Build new overviews - TIFF.  Close TIFF files while we          */
/*      operate on it.                                                  */
/* -------------------------------------------------------------------- */
    else
    {
        if( poODS != NULL )
        {
            delete poODS;
            poODS = NULL;
        }

        eErr = GTIFFBuildOverviews( osOvrFilename, nBands, pahBands,
                                    nNewOverviews, panNewOverviewList,
                                    pszResampling,
                                    GDALScaledProgress, pScaledProgress );

        // Probe for proxy overview filename.
        if( eErr == CE_Failure )
        {
            const char *pszProxyOvrFilename =
                poDS->GetMetadataItem("FILENAME","ProxyOverviewRequest");

            if( pszProxyOvrFilename != NULL )
            {
                osOvrFilename = pszProxyOvrFilename;
                eErr = GTIFFBuildOverviews( osOvrFilename, nBands, pahBands,
                                            nNewOverviews, panNewOverviewList,
                                            pszResampling,
                                            GDALScaledProgress, pScaledProgress );
            }
        }

        if( eErr == CE_None )
        {
            poODS = static_cast<GDALDataset *>(
                GDALOpen( osOvrFilename, GA_Update ) );
            if( poODS == NULL )
                eErr = CE_Failure;
        }
    }

    GDALDestroyScaledProgress( pScaledProgress );

/* -------------------------------------------------------------------- */
/*      Refresh old overviews that were listed.                         */
/* -------------------------------------------------------------------- */
    GDALRasterBand **papoOverviewBands = static_cast<GDALRasterBand **>(
        CPLCalloc(sizeof(void*), nOverviews) );

    for( int iBand = 0; iBand < nBands && eErr == CE_None; iBand++ )
    {
        poBand = poDS->GetRasterBand( panBandList[iBand] );

        nNewOverviews = 0;
        for( int i = 0; i < nOverviews && poBand != NULL; i++ )
        {
            for( int j = 0; j < poBand->GetOverviewCount(); j++ )
            {
                GDALRasterBand * poOverview = poBand->GetOverview( j );
                if( poOverview == NULL )
                    continue;

                int bHasNoData = FALSE;
                double noDataValue = poBand->GetNoDataValue(&bHasNoData);

                if( bHasNoData )
                  poOverview->SetNoDataValue(noDataValue);

                const int nOvFactor =
                    GDALComputeOvFactor(poOverview->GetXSize(),
                                        poBand->GetXSize(),
                                        poOverview->GetYSize(),
                                        poBand->GetYSize());

                if( nOvFactor == - panOverviewList[i]
                    || (panOverviewList[i] < 0 &&
                        nOvFactor == GDALOvLevelAdjust2( -panOverviewList[i],
                                                       poBand->GetXSize(),
                                                       poBand->GetYSize() )) )
                {
                    papoOverviewBands[nNewOverviews++] = poOverview;
                    break;
                }
            }
        }

        if( nNewOverviews > 0 )
        {
            const double dfOffset = dfAreaNewOverviews / dfAreaRefreshedOverviews;
            const double dfScale = 1.0 - dfOffset;
            pScaledProgress = GDALCreateScaledProgress(
                    dfOffset + dfScale * iBand / nBands,
                    dfOffset + dfScale * (iBand+1) / nBands,
                    pfnProgress, pProgressData );
            eErr = GDALRegenerateOverviews( (GDALRasterBandH) poBand,
                                            nNewOverviews,
                                            (GDALRasterBandH*)papoOverviewBands,
                                            pszResampling,
                                            GDALScaledProgress, pScaledProgress );
            GDALDestroyScaledProgress( pScaledProgress );
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CPLFree( papoOverviewBands );
    CPLFree( panNewOverviewList );
    CPLFree( pahBands );

/* -------------------------------------------------------------------- */
/*      If we have a mask file, we need to build its overviews too.     */
/* -------------------------------------------------------------------- */
    if( HaveMaskFile() && poMaskDS )
    {
        // Some config option are not compatible with mask overviews
        // so unset them, and define more sensible values.
        const bool bJPEG =
            EQUAL(CPLGetConfigOption("COMPRESS_OVERVIEW", ""), "JPEG");
        const bool bPHOTOMETRIC_YCBCR =
            EQUAL(CPLGetConfigOption("PHOTOMETRIC_OVERVIEW", ""), "YCBCR");
        if( bJPEG )
            CPLSetThreadLocalConfigOption("COMPRESS_OVERVIEW", "DEFLATE");
        if( bPHOTOMETRIC_YCBCR )
            CPLSetThreadLocalConfigOption("PHOTOMETRIC_OVERVIEW", "");

        poMaskDS->BuildOverviews( pszResampling, nOverviews, panOverviewList,
                                  0, NULL, pfnProgress, pProgressData );

        // Restore config option.
        if( bJPEG )
            CPLSetThreadLocalConfigOption("COMPRESS_OVERVIEW", "JPEG");
        if( bPHOTOMETRIC_YCBCR )
            CPLSetThreadLocalConfigOption("PHOTOMETRIC_OVERVIEW", "YCBCR");

        if( bOwnMaskDS )
        {
            // Reset the poMask member of main dataset bands, since it
            // will become invalid after poMaskDS closing.
            for( int iBand = 1; iBand <= poDS->GetRasterCount(); iBand ++ )
            {
                GDALRasterBand *poOtherBand = poDS->GetRasterBand(iBand);
                if( poOtherBand != NULL )
                    poOtherBand->InvalidateMaskBand();
            }

            GDALClose( poMaskDS );
        }

        // force next request to reread mask file.
        poMaskDS = NULL;
        bOwnMaskDS = false;
        bCheckedForMask = false;
    }

/* -------------------------------------------------------------------- */
/*      If we have an overview dataset, then mark all the overviews     */
/*      with the base dataset  Used later for finding overviews         */
/*      masks.  Uggg.                                                   */
/* -------------------------------------------------------------------- */
    if( poODS )
    {
        const int nOverviewCount = GetOverviewCount(1);

        for( int iOver = 0; iOver < nOverviewCount; iOver++ )
        {
            GDALRasterBand *poOtherBand = GetOverview( 1, iOver );
            GDALDataset *poOverDS = poOtherBand != NULL ?
                poOtherBand->GetDataset() : NULL;

            if( poOverDS != NULL )
            {
                poOverDS->oOvManager.poBaseDS = poDS;
                poOverDS->oOvManager.poDS = poOverDS;
            }
        }
    }

    return eErr;
}

/************************************************************************/
/*                           CreateMaskBand()                           */
/************************************************************************/

CPLErr GDALDefaultOverviews::CreateMaskBand( int nFlags, int nBand )

{
    if( nBand < 1 )
        nFlags |= GMF_PER_DATASET;

/* -------------------------------------------------------------------- */
/*      ensure existing file gets opened if there is one.               */
/* -------------------------------------------------------------------- */
    CPL_IGNORE_RET_VAL(HaveMaskFile());

/* -------------------------------------------------------------------- */
/*      Try creating the mask file.                                     */
/* -------------------------------------------------------------------- */
    if( poMaskDS == NULL )
    {
        GDALDriver * const poDr =
            static_cast<GDALDriver *>( GDALGetDriverByName( "GTiff" ) );

        if( poDr == NULL )
            return CE_Failure;

        GDALRasterBand * const poTBand = poDS->GetRasterBand(1);
        if( poTBand == NULL )
            return CE_Failure;

        const int nBands = (nFlags & GMF_PER_DATASET) ?
            1 : poDS->GetRasterCount();

        char **papszOpt = CSLSetNameValue( NULL, "COMPRESS", "DEFLATE" );
        papszOpt = CSLSetNameValue( papszOpt, "INTERLEAVE", "BAND" );

        int nBX = 0;
        int nBY = 0;
        poTBand->GetBlockSize( &nBX, &nBY );

        // Try to create matching tile size if legal in TIFF.
        if( (nBX % 16) == 0 && (nBY % 16) == 0 )
        {
            papszOpt = CSLSetNameValue( papszOpt, "TILED", "YES" );
            papszOpt = CSLSetNameValue( papszOpt, "BLOCKXSIZE",
                                        CPLString().Printf("%d",nBX) );
            papszOpt = CSLSetNameValue( papszOpt, "BLOCKYSIZE",
                                        CPLString().Printf("%d",nBY) );
        }

        CPLString osMskFilename;
        osMskFilename.Printf( "%s.msk", poDS->GetDescription() );
        poMaskDS = poDr->Create( osMskFilename,
                                 poDS->GetRasterXSize(),
                                 poDS->GetRasterYSize(),
                                 nBands, GDT_Byte, papszOpt );
        CSLDestroy( papszOpt );

        if( poMaskDS == NULL )  // Presumably error already issued.
            return CE_Failure;

        bOwnMaskDS = true;
    }

/* -------------------------------------------------------------------- */
/*      Save the mask flags for this band.                              */
/* -------------------------------------------------------------------- */
    if( nBand > poMaskDS->GetRasterCount() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to create a mask band for band %d of %s, "
                  "but the .msk file has a PER_DATASET mask.",
                  nBand, poDS->GetDescription() );
        return CE_Failure;
    }

    for( int iBand = 0; iBand < poDS->GetRasterCount(); iBand++ )
    {
        // we write only the info for this band, unless we are
        // using PER_DATASET in which case we write for all.
        if( nBand != iBand + 1 && !(nFlags & GMF_PER_DATASET) )
            continue;

        poMaskDS->SetMetadataItem(
            CPLString().Printf("INTERNAL_MASK_FLAGS_%d", iBand+1 ),
            CPLString().Printf("%d", nFlags ) );
    }

    return CE_None;
}

/************************************************************************/
/*                            GetMaskBand()                             */
/************************************************************************/

// Secret code meaning we don't handle this band.
static const int MISSING_FLAGS = 0x8000;

GDALRasterBand *GDALDefaultOverviews::GetMaskBand( int nBand )

{
    const int nFlags = GetMaskFlags( nBand );

    if( nFlags == MISSING_FLAGS )
        return NULL;

    if( nFlags & GMF_PER_DATASET )
        return poMaskDS->GetRasterBand(1);

    if( nBand > 0 )
        return poMaskDS->GetRasterBand( nBand );

    return NULL;
}

/************************************************************************/
/*                            GetMaskFlags()                            */
/************************************************************************/

int GDALDefaultOverviews::GetMaskFlags( int nBand )

{
/* -------------------------------------------------------------------- */
/*      Fetch this band's metadata entry.  They are of the form:        */
/*        INTERNAL_MASK_FLAGS_n: flags                                  */
/* -------------------------------------------------------------------- */
    if( !HaveMaskFile() )
        return 0;

    const char *pszValue =
        poMaskDS->GetMetadataItem(
            CPLString().Printf( "INTERNAL_MASK_FLAGS_%d", std::max(nBand, 1)) );

    if( pszValue == NULL )
        return MISSING_FLAGS;

    return atoi(pszValue);
}

/************************************************************************/
/*                            HaveMaskFile()                            */
/*                                                                      */
/*      Check for a mask file if we haven't already done so.            */
/*      Returns TRUE if we have one, otherwise FALSE.                   */
/************************************************************************/

int GDALDefaultOverviews::HaveMaskFile( char ** papszSiblingFiles,
                                        const char *pszBasename )

{
/* -------------------------------------------------------------------- */
/*      Have we already checked for masks?                              */
/* -------------------------------------------------------------------- */
    if( bCheckedForMask )
        return poMaskDS != NULL;

    if( papszSiblingFiles == NULL )
        papszSiblingFiles = papszInitSiblingFiles;

/* -------------------------------------------------------------------- */
/*      Are we an overview?  If so we need to find the corresponding    */
/*      overview in the base files mask file (if there is one).         */
/* -------------------------------------------------------------------- */
    if( poBaseDS != NULL && poBaseDS->oOvManager.HaveMaskFile() )
    {
        GDALRasterBand * const poBaseBand = poBaseDS->GetRasterBand(1);
        GDALRasterBand * poBaseMask = poBaseBand != NULL ?
            poBaseBand->GetMaskBand() : NULL;

        const int nOverviewCount = poBaseMask != NULL ?
            poBaseMask->GetOverviewCount() : 0;

        for( int iOver = 0; iOver < nOverviewCount; iOver++ )
        {
            GDALRasterBand * const poOverBand =
                poBaseMask->GetOverview( iOver );
            if( poOverBand == NULL )
                continue;

            if( poOverBand->GetXSize() == poDS->GetRasterXSize()
                && poOverBand->GetYSize() == poDS->GetRasterYSize() )
            {
                poMaskDS = poOverBand->GetDataset();
                break;
            }
        }

        bCheckedForMask = true;
        bOwnMaskDS = false;

        CPLAssert( poMaskDS != poDS );

        return poMaskDS != NULL;
    }

/* -------------------------------------------------------------------- */
/*      Are we even initialized?  If not, we apparently don't want      */
/*      to support overviews and masks.                                 */
/* -------------------------------------------------------------------- */
    if( poDS == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Check for .msk file.                                            */
/* -------------------------------------------------------------------- */
    bCheckedForMask = true;

    if( pszBasename == NULL )
        pszBasename = poDS->GetDescription();

    // Don't bother checking for masks of masks.
    if( EQUAL(CPLGetExtension(pszBasename),"msk") )
        return FALSE;

    if( !GDALCanFileAcceptSidecarFile(pszBasename) )
        return FALSE;
    CPLString osMskFilename;
    osMskFilename.Printf( "%s.msk", pszBasename );

    std::vector<char> achMskFilename;
    achMskFilename.resize(osMskFilename.size() + 1);
    memcpy(&(achMskFilename[0]),
           osMskFilename.c_str(),
           osMskFilename.size() + 1);
    bool bExists = CPL_TO_BOOL(
        CPLCheckForFile( &achMskFilename[0],
                         papszSiblingFiles ) );
    osMskFilename = &achMskFilename[0];

#if !defined(WIN32)
    if( !bExists && !papszSiblingFiles )
    {
        osMskFilename.Printf( "%s.MSK", pszBasename );
        memcpy(&(achMskFilename[0]),
               osMskFilename.c_str(),
               osMskFilename.size() + 1);
        bExists = CPL_TO_BOOL(
            CPLCheckForFile( &achMskFilename[0],
                             papszSiblingFiles ) );
        osMskFilename = &achMskFilename[0];
    }
#endif

    if( !bExists )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    poMaskDS = static_cast<GDALDataset *>(
        GDALOpenEx( osMskFilename,
                    GDAL_OF_RASTER |
                    (poDS->GetAccess() == GA_Update ? GDAL_OF_UPDATE : 0),
                    NULL, NULL, papszInitSiblingFiles ));
    CPLAssert( poMaskDS != poDS );

    if( poMaskDS == NULL )
        return FALSE;

    bOwnMaskDS = true;

    return TRUE;
}
//! @endcond

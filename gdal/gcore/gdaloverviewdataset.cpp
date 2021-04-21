/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of a dataset overview warping class
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault, <even dot rouault at spatialys dot com>
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

#include <cstring>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "gdal.h"
#include "gdal_mdreader.h"
#include "gdal_proxy.h"

CPL_CVSID("$Id$")

/** In GDAL, GDALRasterBand::GetOverview() returns a stand-alone band, that may
    have no parent dataset. This can be inconvenient in certain contexts, where
    cross-band processing must be done, or when API expect a fully fledged
    dataset.  Furthermore even if overview band has a container dataset, that
    one often fails to declare its projection, geotransform, etc... which make
    it somehow useless. GDALOverviewDataset remedies to those deficiencies.
*/

class GDALOverviewBand;

/* ******************************************************************** */
/*                          GDALOverviewDataset                         */
/* ******************************************************************** */

class GDALOverviewDataset final: public GDALDataset
{
  private:
    friend class GDALOverviewBand;

    GDALDataset* poMainDS = nullptr;

    GDALDataset* poOvrDS = nullptr;  // Will be often NULL.
    int          nOvrLevel = 0;
    int          bThisLevelOnly = 0;

    int          nGCPCount = 0;
    GDAL_GCP    *pasGCPList = nullptr;
    char       **papszMD_RPC = nullptr;
    char       **papszMD_GEOLOCATION = nullptr;
    GDALOverviewBand* m_poMaskBand = nullptr;

    static void  Rescale( char**& papszMD, const char* pszItem,
                          double dfRatio, double dfDefaultVal );

  protected:
    CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                      void *, int, int, GDALDataType,
                      int, int *,
                      GSpacing, GSpacing, GSpacing,
                      GDALRasterIOExtraArg* psExtraArg ) override;

  public:
    GDALOverviewDataset( GDALDataset* poMainDS,
                         int nOvrLevel,
                         int bThisLevelOnly );
    ~GDALOverviewDataset() override;

    const OGRSpatialReference* GetSpatialRef() const override;
    CPLErr GetGeoTransform( double * ) override;

    int GetGCPCount() override;
    const OGRSpatialReference *GetGCPSpatialRef() const override;
    const GDAL_GCP *GetGCPs() override;

    char  **GetMetadata( const char * pszDomain = "" ) override;
    const char *GetMetadataItem( const char * pszName,
                                 const char * pszDomain = "" ) override;

    int CloseDependentDatasets() override;

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALOverviewDataset)
};

/* ******************************************************************** */
/*                           GDALOverviewBand                           */
/* ******************************************************************** */

class GDALOverviewBand final: public GDALProxyRasterBand
{
  protected:
    friend class GDALOverviewDataset;

    GDALRasterBand*         poUnderlyingBand = nullptr;
    GDALRasterBand* RefUnderlyingRasterBand() override;

    CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                      void *, int, int, GDALDataType,
                      GSpacing, GSpacing,
                      GDALRasterIOExtraArg* psExtraArg ) override;

  public:
    GDALOverviewBand( GDALOverviewDataset* poDS, int nBand );
    ~GDALOverviewBand() override;

    CPLErr FlushCache() override;

    int GetOverviewCount() override;
    GDALRasterBand *GetOverview( int ) override;

    int GetMaskFlags() override;
    GDALRasterBand* GetMaskBand() override;

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALOverviewBand)
};

/************************************************************************/
/*                           GetOverviewEx()                            */
/************************************************************************/

static GDALRasterBand* GetOverviewEx(GDALRasterBand* poBand, int nLevel)
{
    if( nLevel == -1 )
        return poBand;
    return poBand->GetOverview(nLevel);
}

/************************************************************************/
/*                       GDALCreateOverviewDataset()                    */
/************************************************************************/

// Takes a reference on poMainDS in case of success.
// nOvrLevel=-1 means the full resolution dataset (only useful if
// bThisLevelOnly = false to expose a dataset without its overviews)
GDALDataset* GDALCreateOverviewDataset( GDALDataset* poMainDS, int nOvrLevel,
                                        int bThisLevelOnly )
{
    // Sanity checks.
    const int nBands = poMainDS->GetRasterCount();
    if( nBands == 0 )
        return nullptr;

    auto poFirstBand = GetOverviewEx(poMainDS->GetRasterBand(1), nOvrLevel);
    for( int i = 1; i<= nBands; ++i )
    {
        auto poBand = GetOverviewEx(poMainDS->GetRasterBand(i), nOvrLevel);
        if( poBand == nullptr )
        {
            return nullptr;
        }
        if( poBand->GetXSize() != poFirstBand->GetXSize() ||
            poBand->GetYSize() != poFirstBand->GetYSize() )
        {
            return nullptr;
        }
    }

    return new GDALOverviewDataset(poMainDS, nOvrLevel, bThisLevelOnly);
}

/************************************************************************/
/*                        GDALOverviewDataset()                         */
/************************************************************************/

GDALOverviewDataset::GDALOverviewDataset( GDALDataset* poMainDSIn,
                                          int nOvrLevelIn,
                                          int bThisLevelOnlyIn ) :
    poMainDS(poMainDSIn),
    nOvrLevel(nOvrLevelIn),
    bThisLevelOnly(bThisLevelOnlyIn)
{
    poMainDSIn->Reference();
    eAccess = poMainDS->GetAccess();
    auto poFirstBand = GetOverviewEx(poMainDS->GetRasterBand(1), nOvrLevel);
    nRasterXSize = poFirstBand->GetXSize();
    nRasterYSize = poFirstBand->GetYSize();
    poOvrDS = poFirstBand->GetDataset();
    if( nOvrLevel != -1 && poOvrDS != nullptr && poOvrDS == poMainDS )
    {
        CPLDebug( "GDAL",
                  "Dataset of overview is the same as the main band. "
                  "This is not expected");
        poOvrDS = nullptr;
    }
    nBands = poMainDS->GetRasterCount();
    for( int i = 0; i < nBands; ++i )
    {
        SetBand(i+1, new GDALOverviewBand(this, i+1));
    }

    if( poFirstBand->GetMaskFlags() == GMF_PER_DATASET )
    {
        auto poOvrMaskBand = poFirstBand->GetMaskBand();
        if( poOvrMaskBand  && poOvrMaskBand->GetXSize() == nRasterXSize &&
            poOvrMaskBand->GetYSize() == nRasterYSize )
        {
            m_poMaskBand = new GDALOverviewBand(this, 0);
        }
    }

    // We create a fake driver that has the same name as the original
    // one, but we cannot use the real driver object, so that code
    // doesn't try to cast the GDALOverviewDataset* as a native dataset
    // object.
    if( poMainDS->GetDriver() != nullptr )
    {
        poDriver = new GDALDriver();
        poDriver->SetDescription(poMainDS->GetDriver()->GetDescription());
        poDriver->SetMetadata(poMainDS->GetDriver()->GetMetadata());
    }

    if( poOvrDS )
        poOvrDS->SetEnableOverviews(false);

    SetDescription( poMainDS->GetDescription() );

    CPLDebug( "GDAL", "GDALOverviewDataset(%s, this=%p) creation.",
              poMainDS->GetDescription(), this );

    papszOpenOptions = CSLDuplicate(poMainDS->GetOpenOptions());
    // Add OVERVIEW_LEVEL if not called from GDALOpenEx(), but directly.
    papszOpenOptions = CSLSetNameValue(papszOpenOptions, "OVERVIEW_LEVEL",
        nOvrLevel == -1 ? "NONE" :
        CPLSPrintf("%d%s", nOvrLevel, bThisLevelOnly ? " only" :""));
}

/************************************************************************/
/*                       ~GDALOverviewDataset()                         */
/************************************************************************/

GDALOverviewDataset::~GDALOverviewDataset()
{
    GDALOverviewDataset::FlushCache();

    GDALOverviewDataset::CloseDependentDatasets();

    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }
    CSLDestroy(papszMD_RPC);

    CSLDestroy(papszMD_GEOLOCATION);

    delete poDriver;
}

/************************************************************************/
/*                      CloseDependentDatasets()                        */
/************************************************************************/

int GDALOverviewDataset::CloseDependentDatasets()
{
    bool bRet = false;

    if( poMainDS )
    {
        for( int i = 0; i < nBands; ++i )
        {
            GDALOverviewBand* const band =
                cpl::down_cast<GDALOverviewBand*>(papoBands[i]);
            band->poUnderlyingBand = nullptr;
        }
        if( poMainDS->ReleaseRef() )
            bRet = true;
        poMainDS = nullptr;
    }

    if( m_poMaskBand )
    {
        m_poMaskBand->poUnderlyingBand = nullptr;
        delete m_poMaskBand;
        m_poMaskBand = nullptr;
    }

    return bRet;
}

/************************************************************************/
/*                             IRasterIO()                              */
/*                                                                      */
/*      The default implementation of IRasterIO() is to pass the        */
/*      request off to each band objects rasterio methods with          */
/*      appropriate arguments.                                          */
/************************************************************************/

CPLErr GDALOverviewDataset::IRasterIO( GDALRWFlag eRWFlag,
                                       int nXOff, int nYOff,
                                       int nXSize, int nYSize,
                                       void * pData,
                                       int nBufXSize, int nBufYSize,
                                       GDALDataType eBufType,
                                       int nBandCount, int *panBandMap,
                                       GSpacing nPixelSpace,
                                       GSpacing nLineSpace,
                                       GSpacing nBandSpace,
                                       GDALRasterIOExtraArg* psExtraArg )

{
    // Try to pass the request to the most appropriate overview dataset.
    if( nBufXSize < nXSize && nBufYSize < nYSize )
    {
        int bTried = FALSE;
        const CPLErr eErr =
            TryOverviewRasterIO( eRWFlag,
                                 nXOff, nYOff, nXSize, nYSize,
                                 pData, nBufXSize, nBufYSize,
                                 eBufType,
                                 nBandCount, panBandMap,
                                 nPixelSpace, nLineSpace,
                                 nBandSpace,
                                 psExtraArg,
                                 &bTried );
        if( bTried )
            return eErr;
    }

    // In case the overview bands are really linked to a dataset, then issue
    // the request to that dataset.
    if( nOvrLevel != -1 && poOvrDS != nullptr )
    {
        return poOvrDS->RasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, nBandCount, panBandMap, nPixelSpace,
            nLineSpace, nBandSpace,
            psExtraArg);
    }

    GDALProgressFunc  pfnProgressGlobal = psExtraArg->pfnProgress;
    void *pProgressDataGlobal = psExtraArg->pProgressData;
    CPLErr eErr = CE_None;

    for( int iBandIndex = 0;
         iBandIndex < nBandCount && eErr == CE_None;
         ++iBandIndex )
    {
        GDALOverviewBand *poBand =
            cpl::down_cast<GDALOverviewBand *>(
                GetRasterBand(panBandMap[iBandIndex]) );
        GByte *pabyBandData =
            static_cast<GByte *>(pData) + iBandIndex * nBandSpace;

        psExtraArg->pfnProgress = GDALScaledProgress;
        psExtraArg->pProgressData =
            GDALCreateScaledProgress( 1.0 * iBandIndex / nBandCount,
                                      1.0 * (iBandIndex + 1) / nBandCount,
                                      pfnProgressGlobal,
                                      pProgressDataGlobal );

        eErr = poBand->IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                  pabyBandData,
                                  nBufXSize, nBufYSize,
                                  eBufType, nPixelSpace,
                                  nLineSpace, psExtraArg );

        GDALDestroyScaledProgress( psExtraArg->pProgressData );
    }

    psExtraArg->pfnProgress = pfnProgressGlobal;
    psExtraArg->pProgressData = pProgressDataGlobal;

    return eErr;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

const OGRSpatialReference *GDALOverviewDataset::GetSpatialRef() const

{
    return poMainDS->GetSpatialRef();
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GDALOverviewDataset::GetGeoTransform( double * padfTransform )

{
    double adfGeoTransform[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
    if( poMainDS->GetGeoTransform(adfGeoTransform) != CE_None )
        return CE_Failure;

    adfGeoTransform[1] *=
        static_cast<double>(poMainDS->GetRasterXSize()) / nRasterXSize;
    adfGeoTransform[2] *=
        static_cast<double>(poMainDS->GetRasterYSize()) / nRasterYSize;
    adfGeoTransform[4] *=
        static_cast<double>(poMainDS->GetRasterXSize()) / nRasterXSize;
    adfGeoTransform[5] *=
        static_cast<double>(poMainDS->GetRasterYSize()) / nRasterYSize;

    memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );

    return CE_None;
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int GDALOverviewDataset::GetGCPCount()

{
    return poMainDS->GetGCPCount();
}

/************************************************************************/
/*                          GetGCPSpatialRef()                          */
/************************************************************************/

const OGRSpatialReference *GDALOverviewDataset::GetGCPSpatialRef() const

{
    return poMainDS->GetGCPSpatialRef();
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *GDALOverviewDataset::GetGCPs()

{
    if( pasGCPList != nullptr )
        return pasGCPList;

    const GDAL_GCP* pasGCPsMain = poMainDS->GetGCPs();
    if( pasGCPsMain == nullptr )
        return nullptr;
    nGCPCount = poMainDS->GetGCPCount();

    pasGCPList = GDALDuplicateGCPs( nGCPCount, pasGCPsMain );
    for( int i = 0; i < nGCPCount; ++i )
    {
        pasGCPList[i].dfGCPPixel *= static_cast<double>(nRasterXSize) /
            poMainDS->GetRasterXSize();
        pasGCPList[i].dfGCPLine *= static_cast<double>(nRasterYSize) /
            poMainDS->GetRasterYSize();
    }
    return pasGCPList;
}

/************************************************************************/
/*                             Rescale()                                */
/************************************************************************/

void GDALOverviewDataset::Rescale( char**& papszMD, const char* pszItem,
                                   double dfRatio, double dfDefaultVal )
{
    double dfVal =
        CPLAtofM( CSLFetchNameValueDef(papszMD, pszItem,
                                       CPLSPrintf("%.18g", dfDefaultVal)) );
    dfVal *= dfRatio;
    papszMD = CSLSetNameValue(papszMD, pszItem, CPLSPrintf("%.18g", dfVal));
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char  **GDALOverviewDataset::GetMetadata( const char * pszDomain )
{
    if( poOvrDS != nullptr )
    {
        char** papszMD = poOvrDS->GetMetadata(pszDomain);
        if( papszMD != nullptr )
            return papszMD;
    }

    char** papszMD = poMainDS->GetMetadata(pszDomain);

    // We may need to rescale some values from the RPC metadata domain.
    if( pszDomain != nullptr && EQUAL(pszDomain, MD_DOMAIN_RPC) &&
        papszMD != nullptr )
    {
        if( papszMD_RPC )
            return papszMD_RPC;
        papszMD_RPC = CSLDuplicate(papszMD);

        Rescale( papszMD_RPC, RPC_LINE_OFF,
                 static_cast<double>(nRasterYSize) / poMainDS->GetRasterYSize(),
                 0.0 );
        Rescale( papszMD_RPC, RPC_LINE_SCALE,
                 static_cast<double>(nRasterYSize) / poMainDS->GetRasterYSize(),
                 1.0 );
        Rescale( papszMD_RPC, RPC_SAMP_OFF,
                 static_cast<double>(nRasterXSize) / poMainDS->GetRasterXSize(),
                 0.0 );
        Rescale( papszMD_RPC, RPC_SAMP_SCALE,
                 static_cast<double>(nRasterXSize) / poMainDS->GetRasterXSize(),
                 1.0 );

        papszMD = papszMD_RPC;
    }

    // We may need to rescale some values from the GEOLOCATION metadata domain.
    if( pszDomain != nullptr && EQUAL(pszDomain, "GEOLOCATION") &&
        papszMD != nullptr )
    {
        if( papszMD_GEOLOCATION )
            return papszMD_GEOLOCATION;
        papszMD_GEOLOCATION = CSLDuplicate(papszMD);

        Rescale( papszMD_GEOLOCATION, "PIXEL_OFFSET",
                 static_cast<double>(poMainDS->GetRasterXSize()) /
                 nRasterXSize, 0.0 );
        Rescale( papszMD_GEOLOCATION, "LINE_OFFSET",
                 static_cast<double>(poMainDS->GetRasterYSize()) /
                 nRasterYSize, 0.0 );

        Rescale( papszMD_GEOLOCATION, "PIXEL_STEP",
                 static_cast<double>(nRasterXSize) / poMainDS->GetRasterXSize(),
                 1.0 );
        Rescale( papszMD_GEOLOCATION, "LINE_STEP",
                 static_cast<double>(nRasterYSize) / poMainDS->GetRasterYSize(),
                 1.0 );

        papszMD = papszMD_GEOLOCATION;
    }

    return papszMD;
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *GDALOverviewDataset::GetMetadataItem( const char * pszName,
                                                  const char * pszDomain )
{
    if( poOvrDS != nullptr )
    {
        const char* pszValue = poOvrDS->GetMetadataItem(pszName, pszDomain);
        if( pszValue != nullptr )
            return pszValue;
    }

    if( pszDomain != nullptr && (EQUAL(pszDomain, "RPC") ||
                              EQUAL(pszDomain, "GEOLOCATION")) )
    {
        char** papszMD = GetMetadata(pszDomain);
        return CSLFetchNameValue(papszMD, pszName);
    }

    return poMainDS->GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                          GDALOverviewBand()                          */
/************************************************************************/

GDALOverviewBand::GDALOverviewBand( GDALOverviewDataset* poDSIn, int nBandIn )
{
    poDS = poDSIn;
    nBand = nBandIn;
    nRasterXSize = poDSIn->nRasterXSize;
    nRasterYSize = poDSIn->nRasterYSize;
    if( nBandIn == 0 )
    {
        poUnderlyingBand =
            GetOverviewEx(poDSIn->poMainDS->GetRasterBand(1), poDSIn->nOvrLevel)->GetMaskBand();
    }
    else
    {
        poUnderlyingBand =
            GetOverviewEx(poDSIn->poMainDS->GetRasterBand(nBandIn), poDSIn->nOvrLevel);
    }
    eDataType = poUnderlyingBand->GetRasterDataType();
    poUnderlyingBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
}

/************************************************************************/
/*                         ~GDALOverviewBand()                          */
/************************************************************************/

GDALOverviewBand::~GDALOverviewBand()
{
    GDALOverviewBand::FlushCache();
}

/************************************************************************/
/*                              FlushCache()                            */
/************************************************************************/

CPLErr GDALOverviewBand::FlushCache()
{
    if( poUnderlyingBand )
        return poUnderlyingBand->FlushCache();
    return CE_None;
}

/************************************************************************/
/*                        RefUnderlyingRasterBand()                     */
/************************************************************************/

GDALRasterBand* GDALOverviewBand::RefUnderlyingRasterBand()
{
    if( poUnderlyingBand )
        return poUnderlyingBand;

    return nullptr;
}

/************************************************************************/
/*                         GetOverviewCount()                           */
/************************************************************************/

int GDALOverviewBand::GetOverviewCount()
{
    GDALOverviewDataset * const poOvrDS =
        cpl::down_cast<GDALOverviewDataset *>(poDS);
    if( poOvrDS->bThisLevelOnly )
        return 0;
    GDALDataset * const poMainDS = poOvrDS->poMainDS;
    GDALRasterBand* poMainBand =
        ( nBand == 0 ) ? poMainDS->GetRasterBand(1)->GetMaskBand() :
                         poMainDS->GetRasterBand(nBand);
    auto poUnderlyingDS = poUnderlyingBand ? poUnderlyingBand->GetDataset() : nullptr;
    if( poUnderlyingDS )
        poUnderlyingDS->SetEnableOverviews(true);
    const int nRet = poMainBand->GetOverviewCount() - poOvrDS->nOvrLevel - 1;
    if( poUnderlyingDS )
        poUnderlyingDS->SetEnableOverviews(false);
    return nRet;
}

/************************************************************************/
/*                           GetOverview()                              */
/************************************************************************/

GDALRasterBand *GDALOverviewBand::GetOverview( int iOvr )
{
    if( iOvr < 0 || iOvr >= GetOverviewCount() )
        return nullptr;
    GDALOverviewDataset * const poOvrDS =
        cpl::down_cast<GDALOverviewDataset *>(poDS);
    GDALDataset * const poMainDS = poOvrDS->poMainDS;
    GDALRasterBand* poMainBand =
        ( nBand == 0 ) ? poMainDS->GetRasterBand(1)->GetMaskBand() :
                         poMainDS->GetRasterBand(nBand);
    auto poUnderlyingDS = poUnderlyingBand ? poUnderlyingBand->GetDataset() : nullptr;
    if( poUnderlyingDS )
        poUnderlyingDS->SetEnableOverviews(true);
    auto poRet = poMainBand->GetOverview(iOvr + poOvrDS->nOvrLevel + 1);
    if( poUnderlyingDS )
        poUnderlyingDS->SetEnableOverviews(false);
    return poRet;
}

/************************************************************************/
/*                           GetMaskFlags()                             */
/************************************************************************/

int GDALOverviewBand::GetMaskFlags()
{
    GDALOverviewDataset * const poOvrDS = cpl::down_cast<GDALOverviewDataset *>(poDS);
    if( nBand != 0 && poOvrDS->m_poMaskBand )
        return GMF_PER_DATASET;
    return GDALProxyRasterBand::GetMaskFlags();
}

/************************************************************************/
/*                           GetMaskBand()                              */
/************************************************************************/

GDALRasterBand* GDALOverviewBand::GetMaskBand()
{
    GDALOverviewDataset * const poOvrDS = cpl::down_cast<GDALOverviewDataset *>(poDS);
    if( nBand != 0 && poOvrDS->m_poMaskBand )
        return poOvrDS->m_poMaskBand;
    return GDALProxyRasterBand::GetMaskBand();
}

/************************************************************************/
/*                            IRasterIO()                               */
/************************************************************************/

CPLErr GDALOverviewBand::IRasterIO( GDALRWFlag eRWFlag,
                                   int nXOff, int nYOff, int nXSize, int nYSize,
                                   void * pData, int nBufXSize, int nBufYSize,
                                   GDALDataType eBufType,
                                   GSpacing nPixelSpace, GSpacing nLineSpace,
                                   GDALRasterIOExtraArg* psExtraArg )
{
    // Try to pass the request to the most appropriate overview.
    if( nBufXSize < nXSize && nBufYSize < nYSize )
    {
        int bTried = FALSE;
        const CPLErr eErr =
            TryOverviewRasterIO( eRWFlag,
                                 nXOff, nYOff, nXSize, nYSize,
                                 pData, nBufXSize, nBufYSize,
                                 eBufType,
                                 nPixelSpace, nLineSpace,
                                 psExtraArg,
                                 &bTried );
        if( bTried )
            return eErr;
    }

    return GDALProxyRasterBand::IRasterIO(eRWFlag,
                                 nXOff, nYOff, nXSize, nYSize,
                                 pData, nBufXSize, nBufYSize,
                                 eBufType,
                                 nPixelSpace, nLineSpace,
                                 psExtraArg);
}

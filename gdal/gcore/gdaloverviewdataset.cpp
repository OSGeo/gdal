/******************************************************************************
 * $Id$
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

#include "gdal_proxy.h"

CPL_CVSID("$Id$");

/** In GDAL, GDALRasterBand::GetOverview() returns a stand-alone band, that
    may have no parent dataset. This can be inconvenient in certain contexts, where
    cross-band processing must be done, or when API expect a fully fledge dataset.
    Furthermore even if overview band has a container dataset, that one often
    fails to declare its projection, geotransform, etc... which make it somehow
    useless. GDALOverviewDataset remedies to those deficiencies.
*/

class GDALOverviewBand;

/* ******************************************************************** */
/*                          GDALOverviewDataset                         */
/* ******************************************************************** */

class GDALOverviewDataset : public GDALDataset
{
    private:

        friend class GDALOverviewBand;

        GDALDataset* poMainDS;
        int          bOwnDS;

        GDALDataset* poOvrDS; /* will be often NULL */
        int          nOvrLevel;
        int          bThisLevelOnly;

        int          nGCPCount;
        GDAL_GCP    *pasGCPList;
        char       **papszMD_RPC;
        char       **papszMD_GEOLOCATION;

        static void  Rescale(char**& papszMD, const char* pszItem,
                             double dfRatio, double dfDefaultVal);

    protected:
        virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                                void *, int, int, GDALDataType,
                                int, int *, int, int, int );

    public:
                        GDALOverviewDataset(GDALDataset* poMainDS, int nOvrLevel,
                                            int bThisLevelOnly,
                                            int bOwnDS);
        virtual        ~GDALOverviewDataset();

        virtual const char *GetProjectionRef(void);
        virtual CPLErr GetGeoTransform( double * );

        virtual int    GetGCPCount();
        virtual const char *GetGCPProjection();
        virtual const GDAL_GCP *GetGCPs();

        virtual char  **GetMetadata( const char * pszDomain = "" );
        virtual const char *GetMetadataItem( const char * pszName,
                                             const char * pszDomain = "" );

        virtual int        CloseDependentDatasets();
};

/* ******************************************************************** */
/*                           GDALOverviewBand                           */
/* ******************************************************************** */

class GDALOverviewBand : public GDALProxyRasterBand
{
    protected:
        friend class GDALOverviewDataset;

        GDALRasterBand*         poUnderlyingBand;
        virtual GDALRasterBand* RefUnderlyingRasterBand();

    public:
                    GDALOverviewBand(GDALOverviewDataset* poDS, int nBand);
        virtual    ~GDALOverviewBand();

        virtual CPLErr FlushCache();

        virtual int GetOverviewCount();
        virtual GDALRasterBand *GetOverview(int);
};

/************************************************************************/
/*                       GDALCreateOverviewDataset()                    */
/************************************************************************/

GDALDataset* GDALCreateOverviewDataset(GDALDataset* poMainDS, int nOvrLevel,
                                       int bThisLevelOnly, int bOwnDS)
{
    /* Sanity checks */
    int nBands = poMainDS->GetRasterCount();
    if( nBands == 0 )
        return NULL;

    for(int i = 1; i<= nBands; i++ )
    {
        if( poMainDS->GetRasterBand(i)->GetOverview(nOvrLevel) == NULL )
        {
            return NULL;
        }
        if( poMainDS->GetRasterBand(i)->GetOverview(nOvrLevel)->GetXSize() !=
            poMainDS->GetRasterBand(1)->GetOverview(nOvrLevel)->GetXSize() ||
            poMainDS->GetRasterBand(i)->GetOverview(nOvrLevel)->GetYSize() !=
            poMainDS->GetRasterBand(1)->GetOverview(nOvrLevel)->GetYSize() )
        {
            return NULL;
        }
    }

    return new GDALOverviewDataset(poMainDS, nOvrLevel, bThisLevelOnly, bOwnDS);
}

/************************************************************************/
/*                        GDALOverviewDataset()                         */
/************************************************************************/

GDALOverviewDataset::GDALOverviewDataset(GDALDataset* poMainDS,
                                         int nOvrLevel,
                                         int bThisLevelOnly,
                                         int bOwnDS)
{
    this->poMainDS = poMainDS;
    this->nOvrLevel = nOvrLevel;
    this->bOwnDS = bOwnDS;
    this->bThisLevelOnly = bThisLevelOnly;
    eAccess = poMainDS->GetAccess();
    nRasterXSize = poMainDS->GetRasterBand(1)->GetOverview(nOvrLevel)->GetXSize();
    nRasterYSize = poMainDS->GetRasterBand(1)->GetOverview(nOvrLevel)->GetYSize();
    poOvrDS = poMainDS->GetRasterBand(1)->GetOverview(nOvrLevel)->GetDataset();
    if( poOvrDS != NULL && poOvrDS == poMainDS )
    {
        CPLDebug("GDAL", "Dataset of overview is the same as the main band. This is not expected");
        poOvrDS = NULL;
    }
    nBands = poMainDS->GetRasterCount();
    for(int i=0;i<nBands;i++)
    {
        SetBand(i+1, new GDALOverviewBand(this, i+1));
    }
    nGCPCount = 0;
    pasGCPList = NULL;
    papszMD_RPC = NULL;
    papszMD_GEOLOCATION = NULL;

    /* We create a fake driver that has the same name as the original */
    /* one, but we cannot use the real driver object, so that code */
    /* doesn't try to cast the GDALOverviewDataset* as a native dataset */
    /* object */
    if( poMainDS->GetDriver() != NULL )
    {
        poDriver = new GDALDriver();
        poDriver->SetDescription(poMainDS->GetDriver()->GetDescription());
        poDriver->SetMetadata(poMainDS->GetDriver()->GetMetadata());
    }

    SetDescription( poMainDS->GetDescription() );

    CPLDebug( "GDAL", "GDALOverviewDataset(%s, this=%p) creation.",
                          poMainDS->GetDescription(), this );

    papszOpenOptions = CSLDuplicate(poMainDS->GetOpenOptions());
    /* Add OVERVIEW_LEVEL if not called from GDALOpenEx() but directly */
    papszOpenOptions = CSLSetNameValue(papszOpenOptions, "OVERVIEW_LEVEL",
                                       CPLSPrintf("%d", nOvrLevel));
}

/************************************************************************/
/*                       ~GDALOverviewDataset()                         */
/************************************************************************/

GDALOverviewDataset::~GDALOverviewDataset()
{
    FlushCache();

    CloseDependentDatasets();

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
    int bRet = FALSE;

    if( bOwnDS )
    {
        for(int i=0;i<nBands;i++)
        {
            ((GDALOverviewBand*)papoBands[i])->poUnderlyingBand = NULL;
        }
        GDALClose( poMainDS );
        poMainDS = NULL;
        bOwnDS = FALSE;
        bRet = TRUE;
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
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType, 
                               int nBandCount, int *panBandMap,
                               int nPixelSpace, int nLineSpace, int nBandSpace)
    
{
    int iBandIndex; 
    CPLErr eErr = CE_None;

    /* In case the overview bands are really linked to a dataset, then issue */
    /* the request to that dataset */
    if( poOvrDS != NULL )
    {
        return poOvrDS->RasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, nBandCount, panBandMap, nPixelSpace, nLineSpace, nBandSpace);
    }

    for( iBandIndex = 0; 
         iBandIndex < nBandCount && eErr == CE_None; 
         iBandIndex++ )
    {
        GDALOverviewBand *poBand = (GDALOverviewBand*) GetRasterBand(panBandMap[iBandIndex]);
        GByte *pabyBandData;

        if (poBand == NULL)
        {
            eErr = CE_Failure;
            break;
        }

        pabyBandData = ((GByte *) pData) + iBandIndex * nBandSpace;
        
        eErr = poBand->IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize, 
                                  (void *) pabyBandData, nBufXSize, nBufYSize,
                                  eBufType, nPixelSpace, nLineSpace );
    }

    return eErr;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *GDALOverviewDataset::GetProjectionRef()

{
    return poMainDS->GetProjectionRef();
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GDALOverviewDataset::GetGeoTransform( double * padfTransform )

{
    double adfGeoTransform[6];
    if( poMainDS->GetGeoTransform(adfGeoTransform) == CE_None )
    {
        if( adfGeoTransform[2] == 0.0 && adfGeoTransform[4] == 0.0 )
        {
            adfGeoTransform[1] *= (double)poMainDS->GetRasterXSize() / nRasterXSize;
            adfGeoTransform[5] *= (double)poMainDS->GetRasterYSize() / nRasterYSize;
        }
        else
        {
            /* If the x and y ratios are not equal, then we cannot really */
            /* compute a geotransform */
            double dfRatio = (double)poMainDS->GetRasterXSize() / nRasterXSize;
            adfGeoTransform[1] *= dfRatio;
            adfGeoTransform[2] *= dfRatio;
            adfGeoTransform[4] *= dfRatio;
            adfGeoTransform[5] *= dfRatio;
        }
        memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );

        return CE_None;
    }
    else
        return CE_Failure;
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int GDALOverviewDataset::GetGCPCount()

{
    return poMainDS->GetGCPCount();
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *GDALOverviewDataset::GetGCPProjection()

{
    return poMainDS->GetGCPProjection();
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *GDALOverviewDataset::GetGCPs()

{
    if( pasGCPList != NULL )
        return pasGCPList;
    
    const GDAL_GCP* pasGCPsMain = poMainDS->GetGCPs();
    if( pasGCPsMain == NULL )
        return NULL;
    nGCPCount = poMainDS->GetGCPCount();

    pasGCPList = GDALDuplicateGCPs( nGCPCount, pasGCPsMain );
    for(int i = 0; i < nGCPCount; i++)
    {
        pasGCPList[i].dfGCPPixel *= (double)nRasterXSize / poMainDS->GetRasterXSize();
        pasGCPList[i].dfGCPLine *= (double)nRasterYSize / poMainDS->GetRasterYSize();
    }
    return pasGCPList;
}

/************************************************************************/
/*                             Rescale()                                */
/************************************************************************/

void GDALOverviewDataset::Rescale(char**& papszMD, const char* pszItem,
                                  double dfRatio, double dfDefaultVal)
{
    double dfVal = CPLAtofM(CSLFetchNameValueDef(papszMD, pszItem,
                                            CPLSPrintf("%.18g", dfDefaultVal)));
    dfVal *= dfRatio;
    papszMD = CSLSetNameValue(papszMD, pszItem, CPLSPrintf("%.18g", dfVal));
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char  **GDALOverviewDataset::GetMetadata( const char * pszDomain )
{
    char** papszMD;
    if (poOvrDS != NULL)
    {
        papszMD = poOvrDS->GetMetadata(pszDomain);
        if( papszMD != NULL )
            return papszMD;
    }

    papszMD = poMainDS->GetMetadata(pszDomain);

    /* We may need to rescale some values from the RPC metadata domain */
    if( pszDomain != NULL && EQUAL(pszDomain, "RPC") && papszMD != NULL )
    {
        if( papszMD_RPC )
            return papszMD_RPC;
        papszMD_RPC = CSLDuplicate(papszMD);

        Rescale(papszMD_RPC, "LINE_OFF",
                (double)nRasterYSize / poMainDS->GetRasterYSize(), 0.0);
        Rescale(papszMD_RPC, "LINE_SCALE",
                (double)nRasterYSize / poMainDS->GetRasterYSize(), 1.0);
        Rescale(papszMD_RPC, "SAMP_OFF",
                (double)nRasterXSize / poMainDS->GetRasterXSize(), 0.0);
        Rescale(papszMD_RPC, "SAMP_SCALE",
                (double)nRasterXSize / poMainDS->GetRasterXSize(), 1.0);

        papszMD = papszMD_RPC;
    }

    /* We may need to rescale some values from the GEOLOCATION metadata domain */
    if( pszDomain != NULL && EQUAL(pszDomain, "GEOLOCATION") && papszMD != NULL )
    {
        if( papszMD_GEOLOCATION )
            return papszMD_GEOLOCATION;
        papszMD_GEOLOCATION = CSLDuplicate(papszMD);

        Rescale(papszMD_GEOLOCATION, "PIXEL_OFFSET",
                (double)poMainDS->GetRasterXSize() / nRasterXSize, 0.0);
        Rescale(papszMD_GEOLOCATION, "LINE_OFFSET",
                (double)poMainDS->GetRasterYSize() / nRasterYSize, 0.0);

        Rescale(papszMD_GEOLOCATION, "PIXEL_STEP",
                (double)nRasterXSize / poMainDS->GetRasterXSize(), 1.0);
        Rescale(papszMD_GEOLOCATION, "LINE_STEP",
                (double)nRasterYSize / poMainDS->GetRasterYSize(), 1.0);

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
    if (poOvrDS != NULL)
    {
        const char* pszValue = poOvrDS->GetMetadataItem(pszName, pszDomain);
        if( pszValue != NULL )
            return pszValue;
    }

    if( pszDomain != NULL && (EQUAL(pszDomain, "RPC") ||
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

GDALOverviewBand::GDALOverviewBand(GDALOverviewDataset* poDS, int nBand)
{
    this->poDS = poDS;
    this->nBand = nBand;
    poUnderlyingBand = poDS->poMainDS->GetRasterBand(nBand)->GetOverview(poDS->nOvrLevel);
    nRasterXSize = poDS->nRasterXSize;
    nRasterYSize = poDS->nRasterYSize;
    eDataType = poUnderlyingBand->GetRasterDataType();
    poUnderlyingBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
}

/************************************************************************/
/*                         ~GDALOverviewBand()                          */
/************************************************************************/

GDALOverviewBand::~GDALOverviewBand()
{
    FlushCache();
}

/************************************************************************/
/*                              FlushCache()                            */
/************************************************************************/

CPLErr GDALOverviewBand::FlushCache()
{
    if( ((GDALOverviewDataset*)poDS)->poMainDS )
        return poUnderlyingBand->FlushCache();
    return CE_None;
}

/************************************************************************/
/*                        RefUnderlyingRasterBand()                     */
/************************************************************************/

GDALRasterBand* GDALOverviewBand::RefUnderlyingRasterBand()
{
    if( ((GDALOverviewDataset*)poDS)->poMainDS )
        return poUnderlyingBand;
    else
        return NULL;
}

/************************************************************************/
/*                         GetOverviewCount()                           */
/************************************************************************/

int GDALOverviewBand::GetOverviewCount()
{
    GDALOverviewDataset* poOvrDS = (GDALOverviewDataset*)poDS;
    if( poOvrDS->bThisLevelOnly )
        return 0;
    GDALDataset* poMainDS = poOvrDS->poMainDS;
    return poMainDS->GetRasterBand(nBand)->GetOverviewCount() - poOvrDS->nOvrLevel - 1;
}

/************************************************************************/
/*                           GetOverview()                              */
/************************************************************************/

GDALRasterBand *GDALOverviewBand::GetOverview(int iOvr)
{
    if( iOvr < 0 || iOvr >= GetOverviewCount() )
        return NULL;
    GDALOverviewDataset* poOvrDS = (GDALOverviewDataset*)poDS;
    GDALDataset* poMainDS = poOvrDS->poMainDS;
    return poMainDS->GetRasterBand(nBand)->GetOverview(iOvr + poOvrDS->nOvrLevel + 1);
}

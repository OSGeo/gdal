/******************************************************************************
 *
 * Project:  Earth Engine Data API Images driver
 * Purpose:  Earth Engine Data API Images driver
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2017-2018, Planet Labs
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
#include "cpl_http.h"
#include "cpl_conv.h"
#include "ogrgeojsonreader.h"
#include "ogrgeojsonwriter.h"
#include "eeda.h"

#include <algorithm>
#include <vector>
#include <map>
#include <limits>

extern "C" void GDALRegister_EEDAI();

static const int DEFAULT_BLOCK_SIZE = 256;

const GUInt32 RETRY_PER_BAND = 1;
const GUInt32 RETRY_SPATIAL_SPLIT = 2;

// Eart engine server only allows up to 16 MB per request
const int SERVER_BYTE_LIMIT = 16 * 1024 * 1024;
const int SERVER_SIMUTANEOUS_BAND_LIMIT = 100;
const int SERVER_DIMENSION_LIMIT = 10000;

/************************************************************************/
/*                          GDALEEDAIDataset                            */
/************************************************************************/

class GDALEEDAIDataset final: public GDALEEDABaseDataset
{
            CPL_DISALLOW_COPY_ASSIGN(GDALEEDAIDataset)

            friend class GDALEEDAIRasterBand;

            int         m_nBlockSize;
            CPLString   m_osAsset{};
            CPLString   m_osAssetName{};
            GDALEEDAIDataset* m_poParentDS;
#ifdef DEBUG_VERBOSE
            int         m_iOvrLevel;
#endif
            CPLString   m_osPixelEncoding{};
            bool        m_bQueryMultipleBands;
            CPLString   m_osWKT{};
            double      m_adfGeoTransform[6];
            std::vector<GDALEEDAIDataset*> m_apoOverviewDS{};

                        GDALEEDAIDataset(GDALEEDAIDataset* poParentDS,
                                         int iOvrLevel);

            void        SetMetadataFromProperties(
                            json_object* poProperties,
                            const std::map<CPLString, int>& aoMapBandNames );
    public:
                GDALEEDAIDataset();
                virtual ~GDALEEDAIDataset();

                virtual const char* _GetProjectionRef() override;
                const OGRSpatialReference* GetSpatialRef() const override {
                    return GetSpatialRefFromOldGetProjectionRef();
                }
                virtual CPLErr GetGeoTransform( double* ) override;

                virtual CPLErr IRasterIO( GDALRWFlag eRWFlag,
                                 int nXOff, int nYOff, int nXSize, int nYSize,
                                 void * pData, int nBufXSize, int nBufYSize,
                                 GDALDataType eBufType,
                                 int nBandCount, int *panBandMap,
                                 GSpacing nPixelSpace, GSpacing nLineSpace,
                                 GSpacing nBandSpace,
                                 GDALRasterIOExtraArg* psExtraArg ) override;

                bool ComputeQueryStrategy();

                bool Open(GDALOpenInfo* poOpenInfo);
};

/************************************************************************/
/*                        GDALEEDAIRasterBand                           */
/************************************************************************/

class GDALEEDAIRasterBand final: public GDALRasterBand
{
                CPL_DISALLOW_COPY_ASSIGN(GDALEEDAIRasterBand)

                friend class GDALEEDAIDataset;

                GDALColorInterp m_eInterp;

                bool    DecodeNPYArray( const GByte* pabyData,
                                        int nDataLen,
                                        bool bQueryAllBands,
                                        void* pDstBuffer,
                                        int nBlockXOff, int nBlockYOff,
                                        int nXBlocks, int nYBlocks,
                                        int nReqXSize, int nReqYSize ) const;
                bool    DecodeGDALDataset( const GByte* pabyData,
                                         int nDataLen,
                                         bool bQueryAllBands,
                                         void* pDstBuffer,
                                         int nBlockXOff, int nBlockYOff,
                                         int nXBlocks, int nYBlocks,
                                         int nReqXSize, int nReqYSize );

                CPLErr  GetBlocks(    int nBlockXOff, int nBlockYOff,
                                      int nXBlocks, int nYBlocks,
                                      bool bQueryAllBands,
                                      void* pBuffer);
                GUInt32 PrefetchBlocks(int nXOff, int nYOff,
                                       int nXSize, int nYSize,
                                       int nBufXSize, int nBufYSize,
                                       bool bQueryAllBands);

    public:
                GDALEEDAIRasterBand(GDALEEDAIDataset *poDSIn,
                                    GDALDataType eDT,
                                    bool bSignedByte);
                virtual ~GDALEEDAIRasterBand();

                virtual CPLErr IRasterIO( GDALRWFlag eRWFlag,
                                  int nXOff, int nYOff, int nXSize, int nYSize,
                                  void * pData, int nBufXSize, int nBufYSize,
                                  GDALDataType eBufType,
                                  GSpacing nPixelSpace, GSpacing nLineSpace,
                                  GDALRasterIOExtraArg* psExtraArg ) CPL_OVERRIDE;

                virtual CPLErr IReadBlock( int, int, void * ) CPL_OVERRIDE;
                virtual int GetOverviewCount() CPL_OVERRIDE;
                virtual GDALRasterBand* GetOverview(int) CPL_OVERRIDE;
                virtual CPLErr SetColorInterpretation(GDALColorInterp eInterp) CPL_OVERRIDE
                                        { m_eInterp = eInterp; return CE_None;}
                virtual GDALColorInterp GetColorInterpretation() CPL_OVERRIDE
                                        { return m_eInterp; }
};

/************************************************************************/
/*                         GDALEEDAIDataset()                           */
/************************************************************************/

GDALEEDAIDataset::GDALEEDAIDataset() :
    m_nBlockSize(DEFAULT_BLOCK_SIZE),
    m_poParentDS(nullptr),
#ifdef DEBUG_VERBOSE
    m_iOvrLevel(0),
#endif
    m_bQueryMultipleBands(false)
{
    m_adfGeoTransform[0] = 0.0;
    m_adfGeoTransform[1] = 1.0;
    m_adfGeoTransform[2] = 0.0;
    m_adfGeoTransform[3] = 0.0;
    m_adfGeoTransform[4] = 0.0;
    m_adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                         GDALEEDAIDataset()                           */
/************************************************************************/

GDALEEDAIDataset::GDALEEDAIDataset(GDALEEDAIDataset* poParentDS,
                                   int iOvrLevel ) :
    m_nBlockSize(poParentDS->m_nBlockSize),
    m_osAsset(poParentDS->m_osAsset),
    m_osAssetName(poParentDS->m_osAssetName),
    m_poParentDS(poParentDS),
#ifdef DEBUG_VERBOSE
    m_iOvrLevel(iOvrLevel),
#endif
    m_osPixelEncoding(poParentDS->m_osPixelEncoding),
    m_bQueryMultipleBands(poParentDS->m_bQueryMultipleBands),
    m_osWKT(poParentDS->m_osWKT)
{
    m_osBaseURL = poParentDS->m_osBaseURL;
    nRasterXSize = m_poParentDS->nRasterXSize >> iOvrLevel;
    nRasterYSize = m_poParentDS->nRasterYSize >> iOvrLevel;
    m_adfGeoTransform[0] = m_poParentDS->m_adfGeoTransform[0];
    m_adfGeoTransform[1] = m_poParentDS->m_adfGeoTransform[1] *
                                    m_poParentDS->nRasterXSize / nRasterXSize;
    m_adfGeoTransform[2] = m_poParentDS->m_adfGeoTransform[2];
    m_adfGeoTransform[3] = m_poParentDS->m_adfGeoTransform[3];
    m_adfGeoTransform[4] = m_poParentDS->m_adfGeoTransform[4];
    m_adfGeoTransform[5] = m_poParentDS->m_adfGeoTransform[5] *
                                    m_poParentDS->nRasterYSize / nRasterYSize;
}

/************************************************************************/
/*                        ~GDALEEDAIDataset()                           */
/************************************************************************/

GDALEEDAIDataset::~GDALEEDAIDataset()
{
    for(size_t i = 0; i < m_apoOverviewDS.size(); i++ )
    {
        delete m_apoOverviewDS[i];
    }
}

/************************************************************************/
/*                        GDALEEDAIRasterBand()                         */
/************************************************************************/

GDALEEDAIRasterBand::GDALEEDAIRasterBand(GDALEEDAIDataset* poDSIn,
                                         GDALDataType eDT,
                                         bool bSignedByte) :
    m_eInterp(GCI_Undefined)
{
    eDataType = eDT;
    nBlockXSize = poDSIn->m_nBlockSize;
    nBlockYSize = poDSIn->m_nBlockSize;
    if( bSignedByte )
    {
        SetMetadataItem("PIXELTYPE", "SIGNEDBYTE", "IMAGE_STRUCTURE");
    }
}

/************************************************************************/
/*                       ~GDALEEDAIRasterBand()                         */
/************************************************************************/

GDALEEDAIRasterBand::~GDALEEDAIRasterBand()
{
}

/************************************************************************/
/*                           GetOverviewCount()                         */
/************************************************************************/

int GDALEEDAIRasterBand::GetOverviewCount()
{
    GDALEEDAIDataset* poGDS = reinterpret_cast<GDALEEDAIDataset*>(poDS);
    return static_cast<int>(poGDS->m_apoOverviewDS.size());
}

/************************************************************************/
/*                              GetOverview()                           */
/************************************************************************/

GDALRasterBand* GDALEEDAIRasterBand::GetOverview(int iIndex)
{
    GDALEEDAIDataset* poGDS = reinterpret_cast<GDALEEDAIDataset*>(poDS);
    if( iIndex >= 0 &&
        iIndex < static_cast<int>(poGDS->m_apoOverviewDS.size()) )
    {
        return poGDS->m_apoOverviewDS[iIndex]->GetRasterBand(nBand);
    }
    return nullptr;
}


/************************************************************************/
/*                            DecodeNPYArray()                          */
/************************************************************************/

bool GDALEEDAIRasterBand::DecodeNPYArray( const GByte* pabyData,
                                          int nDataLen,
                                          bool bQueryAllBands,
                                          void* pDstBuffer,
                                          int nBlockXOff, int nBlockYOff,
                                          int nXBlocks, int nYBlocks,
                                          int nReqXSize, int nReqYSize ) const
{
    GDALEEDAIDataset* poGDS = reinterpret_cast<GDALEEDAIDataset*>(poDS);

    // See https://docs.scipy.org/doc/numpy-1.13.0/neps/npy-format.html
    // for description of NPY array serialization format
    if( nDataLen < 10 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Non NPY array returned");
        return false;
    }

    if( memcmp(pabyData, "\x93NUMPY", 6) != 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Non NPY array returned");
        return false;
    }
    const int nVersionMajor = pabyData[6];
    if( nVersionMajor != 1 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Only version 1 of NPY array supported. Here found %d",
                 nVersionMajor);
        return false;
    }
    // Ignore version minor
    const int nHeaderLen = pabyData[8] | (pabyData[9] << 8);
    if( nDataLen < 10 + nHeaderLen )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Corrupted NPY array returned: not enough bytes for header");
        return false;
    }

#ifdef DEBUG
    CPLString osDescr;
    osDescr.assign(reinterpret_cast<const char *>(pabyData) + 10, nHeaderLen);
    // Should be something like
    // {'descr': [('B2', '<u2'), ('B3', '<u2'), ('B4', '<u2'), ('B8', '<u2'),
    // ('QA10', '<u2')], 'fortran_order': False, 'shape': (256, 256), }
    CPLDebug("EEDAI", "NPY descr: %s", osDescr.c_str());
    // TODO: validate that the descr is the one expected
#endif

    int nTotalDataTypeSize = 0;
    for( int i = 1; i <= poGDS->GetRasterCount(); i++ )
    {
        if( bQueryAllBands || i == nBand )
        {
            nTotalDataTypeSize += GDALGetDataTypeSizeBytes(
                        poGDS->GetRasterBand(i)->GetRasterDataType());
        }
    }
    int nDataSize = nTotalDataTypeSize * nReqXSize * nReqYSize;
    if( nDataLen < 10 + nHeaderLen + nDataSize )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Corrupted NPY array returned: not enough bytes for payload. "
                 "%d needed, only %d found",
                 10 + nHeaderLen + nDataSize, nDataLen);
        return false;
    }
    else if( nDataLen > 10 + nHeaderLen + nDataSize )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Possibly corrupted NPY array returned: "
                 "expected bytes for payload. "
                 "%d needed, got %d found",
                 10 + nHeaderLen + nDataSize, nDataLen);
    }

    for( int iYBlock = 0; iYBlock < nYBlocks; iYBlock++ )
    {
        int nBlockActualYSize = nBlockYSize;
        if( (iYBlock + nBlockYOff + 1) * nBlockYSize > nRasterYSize )
        {
            nBlockActualYSize = nRasterYSize -
                                    (iYBlock + nBlockYOff) * nBlockYSize;
        }

        for( int iXBlock = 0; iXBlock < nXBlocks; iXBlock++ )
        {
            int nBlockActualXSize = nBlockXSize;
            if( (iXBlock + nBlockXOff + 1) * nBlockXSize > nRasterXSize )
            {
                nBlockActualXSize = nRasterXSize -
                                        (iXBlock + nBlockXOff) * nBlockXSize;
            }

            int nOffsetBand = 10 + nHeaderLen +
                (iYBlock * nBlockYSize * nReqXSize + iXBlock * nBlockXSize) *
                        nTotalDataTypeSize;

            for( int i = 1; i <= poGDS->GetRasterCount(); i++ )
            {
                GDALRasterBlock* poBlock = nullptr;
                GByte* pabyDstBuffer;
                if( i == nBand && pDstBuffer != nullptr )
                    pabyDstBuffer = reinterpret_cast<GByte*>(pDstBuffer);
                else if( bQueryAllBands || (i == nBand && pDstBuffer == nullptr) )
                {
                    GDALEEDAIRasterBand* poOtherBand =
                        reinterpret_cast<GDALEEDAIRasterBand*>(
                                                    poGDS->GetRasterBand(i) );
                    poBlock = poOtherBand->TryGetLockedBlockRef(
                        nBlockXOff + iXBlock, nBlockYOff + iYBlock);
                    if (poBlock != nullptr)
                    {
                        poBlock->DropLock();
                        continue;
                    }
                    poBlock = poOtherBand->GetLockedBlockRef(
                        nBlockXOff + iXBlock, nBlockYOff + iYBlock, TRUE);
                    if (poBlock == nullptr)
                    {
                        continue;
                    }
                    pabyDstBuffer =
                            reinterpret_cast<GByte*>(poBlock->GetDataRef());
                }
                else
                {
                    continue;
                }

                GDALDataType eDT = poGDS->GetRasterBand(i)->GetRasterDataType();
                const int nDTSize = GDALGetDataTypeSizeBytes(eDT);

                for( int iLine = 0; iLine < nBlockActualYSize; iLine++ )
                {
                    GByte* pabyLineDest =
                        pabyDstBuffer + iLine * nDTSize * nBlockXSize;
                    GDALCopyWords(
                        const_cast<GByte*>(pabyData) + nOffsetBand +
                                    iLine * nTotalDataTypeSize * nReqXSize,
                        eDT, nTotalDataTypeSize,
                        pabyLineDest,
                        eDT, nDTSize,
                        nBlockActualXSize);
#ifdef CPL_MSB
                    if( nDTSize > 1 )
                    {
                        GDALSwapWords(pabyLineDest, nDTSize,
                                      nBlockActualXSize, nDTSize);
                    }
#endif
                }

                nOffsetBand += nDTSize;

                if( poBlock )
                    poBlock->DropLock();
            }
        }
    }
    return true;
}
/************************************************************************/
/*                            DecodeGDALDataset()                         */
/************************************************************************/

bool GDALEEDAIRasterBand::DecodeGDALDataset( const GByte* pabyData,
                                           int nDataLen,
                                           bool bQueryAllBands,
                                           void* pDstBuffer,
                                           int nBlockXOff, int nBlockYOff,
                                           int nXBlocks, int nYBlocks,
                                           int nReqXSize, int nReqYSize )
{
    GDALEEDAIDataset* poGDS = reinterpret_cast<GDALEEDAIDataset*>(poDS);

    CPLString osTmpFilename(CPLSPrintf("/vsimem/eeai/%p", this));
    VSIFCloseL(VSIFileFromMemBuffer(osTmpFilename,
                                    const_cast<GByte*>(pabyData),
                                    nDataLen,
                                    false));
    const char* const apszDrivers[] = { "PNG", "JPEG", "GTIFF", nullptr };
    GDALDataset* poTileDS = reinterpret_cast<GDALDataset*>(
        GDALOpenEx(osTmpFilename, GDAL_OF_RASTER, apszDrivers, nullptr, nullptr));
    if( poTileDS == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot decode buffer returned by the "
                    "server as a PNG, JPEG or GeoTIFF image");
        VSIUnlink(osTmpFilename);
        return false;
    }
    if( poTileDS->GetRasterXSize() != nReqXSize ||
        poTileDS->GetRasterYSize() != nReqYSize ||
        // The server might return a RGBA image even if only 3 bands are requested
        poTileDS->GetRasterCount() <
                (bQueryAllBands ? poGDS->GetRasterCount() : 1) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Bad dimensions/band count for image returned "
                 "by server: %dx%dx%d",
                 poTileDS->GetRasterXSize(),
                 poTileDS->GetRasterYSize(),
                 poTileDS->GetRasterCount());
        delete poTileDS;
        VSIUnlink(osTmpFilename);
        return false;
    }

    for( int iYBlock = 0; iYBlock < nYBlocks; iYBlock++ )
    {
        int nBlockActualYSize = nBlockYSize;
        if( (iYBlock + nBlockYOff + 1) * nBlockYSize > nRasterYSize )
        {
            nBlockActualYSize = nRasterYSize -
                                        (iYBlock + nBlockYOff) * nBlockYSize;
        }

        for( int iXBlock = 0; iXBlock < nXBlocks; iXBlock++ )
        {
            int nBlockActualXSize = nBlockXSize;
            if( (iXBlock + nBlockXOff + 1) * nBlockXSize > nRasterXSize )
            {
                nBlockActualXSize = nRasterXSize -
                                        (iXBlock + nBlockXOff) * nBlockXSize;
            }

            for(int i=1; i <= poGDS->GetRasterCount(); i++)
            {
                GDALRasterBlock* poBlock = nullptr;
                GByte* pabyDstBuffer;
                if( i == nBand && pDstBuffer != nullptr )
                    pabyDstBuffer = reinterpret_cast<GByte*>(pDstBuffer);
                else if( bQueryAllBands || (i == nBand && pDstBuffer == nullptr) )
                {
                    GDALEEDAIRasterBand* poOtherBand =
                        reinterpret_cast<GDALEEDAIRasterBand*>(
                                                    poGDS->GetRasterBand(i) );
                    poBlock = poOtherBand->TryGetLockedBlockRef(
                        nBlockXOff + iXBlock, nBlockYOff + iYBlock);
                    if (poBlock != nullptr)
                    {
                        poBlock->DropLock();
                        continue;
                    }
                    poBlock = poOtherBand->GetLockedBlockRef(
                        nBlockXOff + iXBlock, nBlockYOff + iYBlock, TRUE);
                    if (poBlock == nullptr)
                    {
                        continue;
                    }
                    pabyDstBuffer =
                            reinterpret_cast<GByte*>(poBlock->GetDataRef());
                }
                else
                {
                    continue;
                }

                GDALDataType eDT = poGDS->GetRasterBand(i)->GetRasterDataType();
                const int nDTSize = GDALGetDataTypeSizeBytes(eDT);
                const int nTileBand = bQueryAllBands ? i : 1;
                CPLErr eErr = poTileDS->GetRasterBand(nTileBand)->
                    RasterIO(GF_Read,
                             iXBlock * nBlockXSize,
                             iYBlock * nBlockYSize,
                             nBlockActualXSize, nBlockActualYSize,
                             pabyDstBuffer,
                             nBlockActualXSize, nBlockActualYSize,
                             eDT,
                             nDTSize, nDTSize * nBlockXSize, nullptr);

                if( poBlock )
                    poBlock->DropLock();
                if( eErr != CE_None )
                {
                    delete poTileDS;
                    VSIUnlink(osTmpFilename);
                    return false;
                }
            }
        }
    }

    delete poTileDS;
    VSIUnlink(osTmpFilename);
    return true;
}

CPLErr GDALEEDAIRasterBand::GetBlocks(int nBlockXOff, int nBlockYOff,
                                      int nXBlocks, int nYBlocks,
                                      bool bQueryAllBands,
                                      void* pBuffer)
{
    GDALEEDAIDataset* poGDS = reinterpret_cast<GDALEEDAIDataset*>(poDS);

    // Build request content
    json_object* poReq = json_object_new_object();
    json_object_object_add(poReq, "fileFormat",
                           json_object_new_string(poGDS->m_osPixelEncoding));
    json_object* poBands = json_object_new_array();
    for( int i = 1; i <= poGDS->GetRasterCount(); i++ )
    {
        if( bQueryAllBands || i == nBand )
        {
            json_object_array_add(poBands,
                json_object_new_string(
                    poGDS->GetRasterBand(i)->GetDescription()));
        }
    }
    json_object_object_add(poReq, "bandIds",
                           poBands);

    int nReqXSize = nBlockXSize * nXBlocks;
    if( (nBlockXOff + nXBlocks) * nBlockXSize > nRasterXSize )
        nReqXSize = nRasterXSize - nBlockXOff * nBlockXSize;
    int nReqYSize = nBlockYSize * nYBlocks;
    if( (nBlockYOff + nYBlocks) * nBlockYSize > nRasterYSize )
        nReqYSize = nRasterYSize - nBlockYOff * nBlockYSize;
    const double dfX0 = poGDS->m_adfGeoTransform[0] +
        nBlockXOff * nBlockXSize * poGDS->m_adfGeoTransform[1];
    const double dfY0 = poGDS->m_adfGeoTransform[3] +
        nBlockYOff * nBlockYSize * poGDS->m_adfGeoTransform[5];
#ifdef DEBUG_VERBOSE
    CPLDebug("EEDAI", "nBlockYOff=%d nBlockYOff=%d "
             "nXBlocks=%d nYBlocks=%d nReqXSize=%d nReqYSize=%d",
             nBlockYOff, nBlockYOff, nXBlocks, nYBlocks, nReqXSize, nReqYSize);
#endif

    json_object* poPixelGrid = json_object_new_object();

    json_object* poAffineTransform = json_object_new_object();
    json_object_object_add(poAffineTransform, "translateX",
        json_object_new_double_with_significant_figures(dfX0, 18));
    json_object_object_add(poAffineTransform, "translateY",
        json_object_new_double_with_significant_figures(dfY0, 18));
    json_object_object_add(poAffineTransform, "scaleX",
        json_object_new_double_with_significant_figures(
            poGDS->m_adfGeoTransform[1], 18));
    json_object_object_add(poAffineTransform, "scaleY",
        json_object_new_double_with_significant_figures(
            poGDS->m_adfGeoTransform[5], 18));
    json_object_object_add(poAffineTransform, "shearX",
        json_object_new_double_with_significant_figures(0.0, 18));
    json_object_object_add(poAffineTransform, "shearY",
        json_object_new_double_with_significant_figures(0.0, 18));
    json_object_object_add(poPixelGrid, "affineTransform", poAffineTransform);

    json_object* poDimensions = json_object_new_object();
    json_object_object_add(poDimensions, "width",
                           json_object_new_int(nReqXSize));
    json_object_object_add(poDimensions, "height",
                           json_object_new_int(nReqYSize));
    json_object_object_add(poPixelGrid, "dimensions", poDimensions);
    json_object_object_add(poReq, "grid", poPixelGrid);

    CPLString osPostContent = json_object_get_string(poReq);
    json_object_put(poReq);

    // Issue request
    char** papszOptions = (poGDS->m_poParentDS) ?
        poGDS->m_poParentDS->GetBaseHTTPOptions() :
        poGDS->GetBaseHTTPOptions();
    papszOptions = CSLSetNameValue(papszOptions, "CUSTOMREQUEST", "POST");
    CPLString osHeaders = CSLFetchNameValueDef(papszOptions, "HEADERS", "");
    if( !osHeaders.empty() )
        osHeaders += "\r\n";
    osHeaders += "Content-Type: application/json";
    papszOptions = CSLSetNameValue(papszOptions, "HEADERS", osHeaders);
    papszOptions = CSLSetNameValue(papszOptions, "POSTFIELDS", osPostContent);
    CPLHTTPResult* psResult = EEDAHTTPFetch(
        (poGDS->m_osBaseURL + poGDS->m_osAssetName + ":getPixels").c_str(),
        papszOptions);
    CSLDestroy(papszOptions);
    if( psResult == nullptr )
        return CE_Failure;

    if( psResult->pszErrBuf != nullptr )
    {
        if( psResult->pabyData )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s: %s",
                     psResult->pszErrBuf,
                     reinterpret_cast<const char*>(psResult->pabyData));
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s",
                     psResult->pabyData ? reinterpret_cast<const char*>(psResult->pabyData) :
                     psResult->pszErrBuf);
        }
        CPLHTTPDestroyResult(psResult);
        return CE_Failure;
    }

    if( psResult->pabyData == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Empty content returned by server");
        CPLHTTPDestroyResult(psResult);
        return CE_Failure;
    }
#ifdef DEBUG_VERBOSE
    CPLDebug("EEADI", "Result: %s (%d bytes)",
             reinterpret_cast<const char*>(psResult->pabyData),
             psResult->nDataLen);
#endif

    if( EQUAL(poGDS->m_osPixelEncoding, "NPY") )
    {
        if( !DecodeNPYArray( psResult->pabyData, psResult->nDataLen,
                             bQueryAllBands,
                             pBuffer,
                             nBlockXOff, nBlockYOff,
                             nXBlocks, nYBlocks,
                             nReqXSize, nReqYSize ) )
        {
            CPLHTTPDestroyResult(psResult);
            return CE_Failure;
        }
    }
    else
    {
        if( !DecodeGDALDataset( psResult->pabyData, psResult->nDataLen,
                              bQueryAllBands,
                              pBuffer,
                              nBlockXOff, nBlockYOff,
                              nXBlocks, nYBlocks,
                              nReqXSize, nReqYSize ) )
        {
            CPLHTTPDestroyResult(psResult);
            return CE_Failure;
        }
    }

    CPLHTTPDestroyResult(psResult);

    return CE_None;
}


/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GDALEEDAIRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                        void *pBuffer )
{
    GDALEEDAIDataset* poGDS = reinterpret_cast<GDALEEDAIDataset*>(poDS);
#ifdef DEBUG_VERBOSE
    CPLDebug("EEDAI", "ReadBlock x=%d y=%d band=%d level=%d",
             nBlockXOff, nBlockYOff, nBand, poGDS->m_iOvrLevel);
#endif

    return GetBlocks(nBlockXOff, nBlockYOff, 1, 1,
                     poGDS->m_bQueryMultipleBands,
                     pBuffer);
}

/************************************************************************/
/*                          PrefetchBlocks()                            */
/************************************************************************/

// Return or'ed flags among 0, RETRY_PER_BAND, RETRY_SPATIAL_SPLIT if the user
// should try to split the request in smaller chunks

GUInt32 GDALEEDAIRasterBand::PrefetchBlocks(int nXOff, int nYOff,
                                         int nXSize, int nYSize,
                                         int nBufXSize, int nBufYSize,
                                         bool bQueryAllBands)
{
    CPL_IGNORE_RET_VAL(nBufXSize);
    CPL_IGNORE_RET_VAL(nBufYSize);

    GDALEEDAIDataset* poGDS = reinterpret_cast<GDALEEDAIDataset*>(poDS);
    int nBlockXOff = nXOff / nBlockXSize;
    int nBlockYOff = nYOff / nBlockYSize;
    int nXBlocks = (nXOff + nXSize - 1) / nBlockXSize - nBlockXOff + 1;
    int nYBlocks = (nYOff + nYSize - 1) / nBlockYSize - nBlockYOff + 1;

    const int nThisDTSize = GDALGetDataTypeSizeBytes(GetRasterDataType());
    int nTotalDataTypeSize = 0;
    int nQueriedBands = 0;
    for( int i = 1; i <= poGDS->GetRasterCount(); i++ )
    {
        if( bQueryAllBands || i == nBand )
        {
            nQueriedBands ++;
            nTotalDataTypeSize += GDALGetDataTypeSizeBytes(
                        poGDS->GetRasterBand(i)->GetRasterDataType());
        }
    }

    // Check the number of already cached blocks, and remove fully
    // cached lines at the top of the area of interest from the queried
    // blocks
    int nBlocksCached = 0;
    int nBlocksCachedForThisBand = 0;
    bool bAllLineCached = true;
    for( int iYBlock = 0; iYBlock < nYBlocks; )
    {
        for( int iXBlock = 0; iXBlock < nXBlocks; iXBlock++ )
        {
            for(int i=1; i <= poGDS->GetRasterCount(); i++)
            {
                GDALRasterBlock* poBlock = nullptr;
                if( bQueryAllBands || i == nBand )
                {
                    GDALEEDAIRasterBand* poOtherBand =
                        reinterpret_cast<GDALEEDAIRasterBand*>(
                                                poGDS->GetRasterBand(i) );
                    poBlock = poOtherBand->TryGetLockedBlockRef(
                        nBlockXOff + iXBlock, nBlockYOff + iYBlock);
                    if (poBlock != nullptr)
                    {
                        nBlocksCached ++;
                        if( i == nBand )
                            nBlocksCachedForThisBand ++;
                        poBlock->DropLock();
                        continue;
                    }
                    else
                    {
                        bAllLineCached = false;
                    }
                }
            }
        }

        if( bAllLineCached )
        {
            nBlocksCached -= nXBlocks * nQueriedBands;
            nBlocksCachedForThisBand -= nXBlocks;
            nBlockYOff ++;
            nYBlocks --;
        }
        else
        {
            iYBlock ++;
        }
    }

    if( nXBlocks > 0 && nYBlocks > 0 )
    {
        bool bMustReturn = false;
        GUInt32 nRetryFlags = 0;

        // Get the blocks if the number of already cached blocks is lesser
        // than 25% of the to be queried blocks
        if( nBlocksCached > (nQueriedBands * nXBlocks * nYBlocks) / 4 )
        {
            if( nBlocksCachedForThisBand <= (nXBlocks * nYBlocks) / 4 )
            {
                nRetryFlags |= RETRY_PER_BAND;
            }
            else
            {
                bMustReturn = true;
            }
        }

        // Don't request too many pixels in one dimension
        if( nXBlocks * nBlockXSize > SERVER_DIMENSION_LIMIT ||
            nYBlocks * nBlockYSize > SERVER_DIMENSION_LIMIT )
        {
            bMustReturn = true;
            nRetryFlags |= RETRY_SPATIAL_SPLIT;
        }

        // Make sure that we have enough cache (with a margin of 50%)
        // and the number of queried pixels isn't too big w.r.t server
        // limit
        const GIntBig nUncompressedSize =
            static_cast<GIntBig>(nXBlocks) * nYBlocks *
                        nBlockXSize * nBlockYSize * nTotalDataTypeSize;
        const GIntBig nCacheMax = GDALGetCacheMax64()/2;
        if( nUncompressedSize > nCacheMax ||
            nUncompressedSize > SERVER_BYTE_LIMIT )
        {
            if( bQueryAllBands && poGDS->GetRasterCount() > 1 )
            {
                const GIntBig nUncompressedSizeThisBand =
                    static_cast<GIntBig>(nXBlocks) * nYBlocks *
                            nBlockXSize * nBlockYSize * nThisDTSize;
                if( nUncompressedSizeThisBand <= SERVER_BYTE_LIMIT &&
                    nUncompressedSizeThisBand <= nCacheMax )
                {
                    nRetryFlags |= RETRY_PER_BAND;
                }
            }
            if( nXBlocks > 1 || nYBlocks > 1 )
            {
                nRetryFlags |= RETRY_SPATIAL_SPLIT;
            }
            return nRetryFlags;
        }
        if( bMustReturn )
            return nRetryFlags;

        GetBlocks(nBlockXOff, nBlockYOff, nXBlocks, nYBlocks,
                   bQueryAllBands, nullptr);
    }

    return 0;
}

/************************************************************************/
/*                              IRasterIO()                             */
/************************************************************************/

CPLErr GDALEEDAIRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                  int nXOff, int nYOff, int nXSize, int nYSize,
                                  void * pData, int nBufXSize, int nBufYSize,
                                  GDALDataType eBufType,
                                  GSpacing nPixelSpace, GSpacing nLineSpace,
                                  GDALRasterIOExtraArg* psExtraArg )

{

/* ==================================================================== */
/*      Do we have overviews that would be appropriate to satisfy       */
/*      this request?                                                   */
/* ==================================================================== */
    if( (nBufXSize < nXSize || nBufYSize < nYSize)
        && GetOverviewCount() > 0 && eRWFlag == GF_Read )
    {
        GDALRasterIOExtraArg sExtraArg;
        GDALCopyRasterIOExtraArg(&sExtraArg, psExtraArg);

        const int nOverview =
            GDALBandGetBestOverviewLevel2( this, nXOff, nYOff, nXSize, nYSize,
                                           nBufXSize, nBufYSize, &sExtraArg );
        if (nOverview >= 0)
        {
            GDALRasterBand* poOverviewBand = GetOverview(nOverview);
            if (poOverviewBand == nullptr)
                return CE_Failure;

            return poOverviewBand->RasterIO(
                eRWFlag, nXOff, nYOff, nXSize, nYSize,
                pData, nBufXSize, nBufYSize, eBufType,
                nPixelSpace, nLineSpace, &sExtraArg );
        }
    }

    GDALEEDAIDataset* poGDS = reinterpret_cast<GDALEEDAIDataset*>(poDS);
    GUInt32 nRetryFlags = PrefetchBlocks(
        nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize,
        poGDS->m_bQueryMultipleBands);
    if( (nRetryFlags & RETRY_SPATIAL_SPLIT) &&
        nXSize == nBufXSize && nYSize == nBufYSize && nYSize > nBlockYSize )
    {
        GDALRasterIOExtraArg sExtraArg;
        INIT_RASTERIO_EXTRA_ARG(sExtraArg);

        int nHalf = std::max(nBlockYSize,
                             ((nYSize / 2 ) / nBlockYSize) * nBlockYSize);
        CPLErr eErr = IRasterIO(eRWFlag, nXOff, nYOff,
                                nXSize, nHalf,
                                pData,
                                nXSize, nHalf,
                                eBufType,
                                nPixelSpace, nLineSpace,
                                &sExtraArg);
        if( eErr == CE_None )
        {
            eErr = IRasterIO(eRWFlag,
                                nXOff, nYOff + nHalf,
                                nXSize, nYSize - nHalf,
                                static_cast<GByte*>(pData) +
                                        nHalf * nLineSpace,
                                nXSize, nYSize - nHalf,
                                eBufType,
                                nPixelSpace, nLineSpace,
                                &sExtraArg);
        }
        return eErr;
    }
    else if( (nRetryFlags & RETRY_SPATIAL_SPLIT) &&
        nXSize == nBufXSize && nYSize == nBufYSize && nXSize > nBlockXSize )
    {
        GDALRasterIOExtraArg sExtraArg;
        INIT_RASTERIO_EXTRA_ARG(sExtraArg);

        int nHalf = std::max(nBlockXSize,
                             ((nXSize / 2 ) / nBlockXSize) * nBlockXSize);
        CPLErr eErr = IRasterIO(eRWFlag, nXOff, nYOff,
                                nHalf, nYSize,
                                pData,
                                nHalf, nYSize,
                                eBufType,
                                nPixelSpace, nLineSpace,
                                &sExtraArg);
        if( eErr == CE_None )
        {
            eErr = IRasterIO(eRWFlag,
                                nXOff + nHalf, nYOff,
                                nXSize - nHalf, nYSize,
                                static_cast<GByte*>(pData) +
                                        nHalf * nPixelSpace,
                                nXSize - nHalf, nYSize,
                                eBufType,
                                nPixelSpace, nLineSpace,
                                &sExtraArg);
        }
        return eErr;
    }
    else if( (nRetryFlags & RETRY_PER_BAND) &&
             poGDS->m_bQueryMultipleBands && poGDS->nBands > 1 )
    {
        CPL_IGNORE_RET_VAL(PrefetchBlocks(
            nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize, false));
    }

    return GDALRasterBand::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize,
                                     eBufType,
                                     nPixelSpace, nLineSpace,
                                     psExtraArg);
}


/************************************************************************/
/*                              IRasterIO()                             */
/************************************************************************/


CPLErr
GDALEEDAIDataset::IRasterIO( GDALRWFlag eRWFlag,
                                 int nXOff, int nYOff, int nXSize, int nYSize,
                                 void * pData, int nBufXSize, int nBufYSize,
                                 GDALDataType eBufType,
                                 int nBandCount, int *panBandMap,
                                 GSpacing nPixelSpace, GSpacing nLineSpace,
                                 GSpacing nBandSpace,
                                 GDALRasterIOExtraArg* psExtraArg )
{

/* ==================================================================== */
/*      Do we have overviews that would be appropriate to satisfy       */
/*      this request?                                                   */
/* ==================================================================== */
    if( (nBufXSize < nXSize || nBufYSize < nYSize)
        && GetRasterBand(1)->GetOverviewCount() > 0 && eRWFlag == GF_Read )
    {
        GDALRasterIOExtraArg sExtraArg;
        GDALCopyRasterIOExtraArg(&sExtraArg, psExtraArg);

        const int nOverview =
            GDALBandGetBestOverviewLevel2( GetRasterBand(1),
                                            nXOff, nYOff, nXSize, nYSize,
                                           nBufXSize, nBufYSize, &sExtraArg );
        if (nOverview >= 0)
        {
            GDALRasterBand* poOverviewBand =
                        GetRasterBand(1)->GetOverview(nOverview);
            if (poOverviewBand == nullptr ||
                poOverviewBand->GetDataset() == nullptr)
            {
                return CE_Failure;
            }

            return poOverviewBand->GetDataset()->RasterIO(
                eRWFlag, nXOff, nYOff, nXSize, nYSize,
                pData, nBufXSize, nBufYSize, eBufType,
                nBandCount, panBandMap,
                nPixelSpace, nLineSpace, nBandSpace, &sExtraArg );
        }
    }

    GDALEEDAIRasterBand* poBand =
        cpl::down_cast<GDALEEDAIRasterBand*>(GetRasterBand(1));

    GUInt32 nRetryFlags = poBand->PrefetchBlocks(
                                nXOff, nYOff, nXSize, nYSize,
                                nBufXSize, nBufYSize,
                                m_bQueryMultipleBands);
    int nBlockXSize, nBlockYSize;
    poBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
    if( (nRetryFlags & RETRY_SPATIAL_SPLIT) &&
        nXSize == nBufXSize && nYSize == nBufYSize && nYSize > nBlockYSize )
    {
        GDALRasterIOExtraArg sExtraArg;
        INIT_RASTERIO_EXTRA_ARG(sExtraArg);

        int nHalf = std::max(nBlockYSize,
                         ((nYSize / 2 ) / nBlockYSize) * nBlockYSize);
        CPLErr eErr = IRasterIO(eRWFlag, nXOff, nYOff,
                                nXSize, nHalf,
                                pData,
                                nXSize, nHalf,
                                eBufType,
                                nBandCount, panBandMap,
                                nPixelSpace, nLineSpace, nBandSpace,
                                &sExtraArg);
        if( eErr == CE_None )
        {
            eErr = IRasterIO(eRWFlag,
                                nXOff, nYOff + nHalf,
                                nXSize, nYSize - nHalf,
                                static_cast<GByte*>(pData) +
                                    nHalf * nLineSpace,
                                nXSize, nYSize - nHalf,
                                eBufType,
                                nBandCount, panBandMap,
                                nPixelSpace, nLineSpace, nBandSpace,
                                &sExtraArg);
        }
        return eErr;
    }
    else if( (nRetryFlags & RETRY_SPATIAL_SPLIT) &&
        nXSize == nBufXSize && nYSize == nBufYSize && nXSize > nBlockXSize )
    {
        GDALRasterIOExtraArg sExtraArg;
        INIT_RASTERIO_EXTRA_ARG(sExtraArg);

        int nHalf = std::max(nBlockXSize,
                         ((nXSize / 2 ) / nBlockXSize) * nBlockXSize);
        CPLErr eErr = IRasterIO(eRWFlag, nXOff, nYOff,
                                nHalf, nYSize,
                                pData,
                                nHalf, nYSize,
                                eBufType,
                                nBandCount, panBandMap,
                                nPixelSpace, nLineSpace, nBandSpace,
                                &sExtraArg);
        if( eErr == CE_None )
        {
            eErr = IRasterIO(eRWFlag,
                                nXOff + nHalf, nYOff,
                                nXSize - nHalf, nYSize,
                                static_cast<GByte*>(pData) +
                                        nHalf * nPixelSpace,
                                nXSize - nHalf, nYSize,
                                eBufType,
                                nBandCount, panBandMap,
                                nPixelSpace, nLineSpace, nBandSpace,
                                &sExtraArg);
        }
        return eErr;
    }
    else if( (nRetryFlags & RETRY_PER_BAND) &&
             m_bQueryMultipleBands && nBands > 1 )
    {
        for( int iBand = 1; iBand <= nBands; iBand++ )
        {
            poBand =
                cpl::down_cast<GDALEEDAIRasterBand*>(GetRasterBand(iBand));
            CPL_IGNORE_RET_VAL(poBand->PrefetchBlocks(
                                    nXOff, nYOff, nXSize, nYSize,
                                    nBufXSize, nBufYSize, false));
        }
    }

    return GDALDataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                  pData, nBufXSize, nBufYSize,
                                  eBufType,
                                  nBandCount, panBandMap,
                                  nPixelSpace, nLineSpace, nBandSpace,
                                  psExtraArg);
}

/************************************************************************/
/*                        ComputeQueryStrategy()                        */
/************************************************************************/

bool GDALEEDAIDataset::ComputeQueryStrategy()
{
    m_bQueryMultipleBands = true;
    m_osPixelEncoding.toupper();

    bool bHeterogeneousDataTypes = false;
    if( nBands >= 2 )
    {
        GDALDataType eDTFirstBand = GetRasterBand(1)->GetRasterDataType();
        for( int i = 2; i <= nBands; i++ )
        {
            if( GetRasterBand(i)->GetRasterDataType() != eDTFirstBand )
            {
                bHeterogeneousDataTypes = true;
                break;
            }
        }
    }

    if( EQUAL(m_osPixelEncoding, "AUTO") )
    {
        if( bHeterogeneousDataTypes )
        {
            m_osPixelEncoding = "NPY";
        }
        else
        {
            m_osPixelEncoding = "PNG";
            for( int i = 1; i <= nBands; i++ )
            {
                if( GetRasterBand(i)->GetRasterDataType() != GDT_Byte )
                {
                    m_osPixelEncoding = "GEO_TIFF";
                }
            }
        }
    }

    if( EQUAL(m_osPixelEncoding, "PNG") ||
        EQUAL(m_osPixelEncoding, "JPEG") ||
        EQUAL(m_osPixelEncoding, "AUTO_JPEG_PNG") )
    {
        if( nBands != 1 && nBands != 3 )
        {
            m_bQueryMultipleBands = false;
        }
        for( int i = 1; i <= nBands; i++ )
        {
            if( GetRasterBand(i)->GetRasterDataType() != GDT_Byte )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                 "This dataset has non-Byte bands, which is incompatible "
                 "with PIXEL_ENCODING=%s", m_osPixelEncoding.c_str());
                return false;
            }
        }
    }

    if(nBands > SERVER_SIMUTANEOUS_BAND_LIMIT )
    {
        m_bQueryMultipleBands = false;
    }

    if( m_bQueryMultipleBands && m_osPixelEncoding != "NPY" &&
        bHeterogeneousDataTypes )
    {
        CPLDebug("EEDAI",
                 "%s PIXEL_ENCODING does not support heterogeneous data types. "
                 "Falling back to querying band per band",
                 m_osPixelEncoding.c_str());
        m_bQueryMultipleBands = false;
    }

    return true;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char* GDALEEDAIDataset::_GetProjectionRef()
{
    return m_osWKT.c_str();
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GDALEEDAIDataset::GetGeoTransform( double* adfGeoTransform )
{
    memcpy( adfGeoTransform, m_adfGeoTransform, 6 * sizeof(double) );
    return CE_None;
}

/************************************************************************/
/*                               Open()                                 */
/************************************************************************/

bool GDALEEDAIDataset::Open(GDALOpenInfo* poOpenInfo)
{
    m_osBaseURL = CPLGetConfigOption("EEDA_URL",
                            "https://earthengine-highvolume.googleapis.com/v1alpha/");

    m_osAsset =
            CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "ASSET", "");
    CPLString osBandList(
            CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "BANDS", "") );
    if( m_osAsset.empty() )
    {
        char** papszTokens =
                CSLTokenizeString2(poOpenInfo->pszFilename, ":", 0);
        if( CSLCount(papszTokens) < 2 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "No asset specified in connection string or "
                     "ASSET open option");
            CSLDestroy(papszTokens);
            return false;
        }
        if( CSLCount(papszTokens) == 3 )
        {
            osBandList = papszTokens[2];
        }

        m_osAsset = papszTokens[1];
        CSLDestroy(papszTokens);
    }
    m_osAssetName = ConvertPathToName(m_osAsset);

    m_osPixelEncoding =
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                             "PIXEL_ENCODING", "AUTO");
    m_nBlockSize = atoi(CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                        "BLOCK_SIZE", CPLSPrintf("%d", DEFAULT_BLOCK_SIZE)));
    if( m_nBlockSize < 128 &&
        !CPLTestBool(CPLGetConfigOption("EEDA_FORCE_BLOCK_SIZE", "FALSE")) )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Invalid BLOCK_SIZE");
        return false;
    }

    std::set<CPLString> oSetUserBandNames;
    {
        char** papszTokens = CSLTokenizeString2(osBandList, ",", 0);
        for( int i = 0; papszTokens && papszTokens[i]; i++ )
            oSetUserBandNames.insert(papszTokens[i]);
        CSLDestroy(papszTokens);
    }

    // Issue request to get image metadata
    char** papszOptions = GetBaseHTTPOptions();
    if( papszOptions == nullptr )
        return false;
    CPLHTTPResult* psResult = EEDAHTTPFetch(
                (m_osBaseURL + m_osAssetName).c_str(), papszOptions);
    CSLDestroy(papszOptions);
    if( psResult == nullptr )
        return false;
    if( psResult->pszErrBuf != nullptr )
    {
        if( psResult->pabyData )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s: %s",
                     psResult->pszErrBuf,
                     reinterpret_cast<const char*>(psResult->pabyData));
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s",
                     psResult->pabyData ? reinterpret_cast<const char*>(psResult->pabyData) :
                     psResult->pszErrBuf);
        }
        CPLHTTPDestroyResult(psResult);
        return false;
    }

    if( psResult->pabyData == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Empty content returned by server");
        CPLHTTPDestroyResult(psResult);
        return false;
    }

    const char* pszText = reinterpret_cast<const char*>(psResult->pabyData);
#ifdef DEBUG_VERBOSE
    CPLDebug("EEDAI", "%s", pszText);
#endif

    json_object* poObj = nullptr;
    if( !OGRJSonParse(pszText, &poObj, true) )
    {
        CPLHTTPDestroyResult(psResult);
        return false;
    }

    CPLHTTPDestroyResult(psResult);

    if( json_object_get_type(poObj) != json_type_object )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Return is not a JSON dictionary");
        json_object_put(poObj);
        return false;
    }

    json_object* poType = CPL_json_object_object_get(poObj, "type");
    const char* pszType = json_object_get_string(poType);
    if( pszType == nullptr || !EQUAL(pszType, "IMAGE") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Asset is not an image, but %s",
                  pszType ? pszType : "(null)" );
        json_object_put(poObj);
        return false;
    }

    json_object* poBands = CPL_json_object_object_get(poObj, "bands");
    if( poBands == nullptr ||  json_object_get_type(poBands) != json_type_array )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No band found");
        json_object_put(poObj);
        return false;
    }

    std::map<CPLString, CPLString> oMapCodeToWKT;
    std::vector<EEDAIBandDesc> aoBandDesc = BuildBandDescArray(poBands,
                                                               oMapCodeToWKT);
    std::map<CPLString, int> aoMapBandNames;

    if( aoBandDesc.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No band found");
        json_object_put(poObj);
        return false;
    }

    // Indices are aoBandDesc indices
    std::map< int, std::vector<int> > oMapSimilarBands;

    size_t iIdxFirstBand = 0;
    for( size_t i = 0; i < aoBandDesc.size(); i++ )
    {
        // Instantiate bands if they are compatible between them, and
        // if they are requested by the user (when user explicitly requested
        // them)
        if( (oSetUserBandNames.empty() ||
             oSetUserBandNames.find(aoBandDesc[i].osName) !=
                                        oSetUserBandNames.end() ) &&
            (nBands == 0 ||
             aoBandDesc[i].IsSimilar(aoBandDesc[iIdxFirstBand])) )
        {
            if( nBands == 0 )
            {
                iIdxFirstBand = i;
                nRasterXSize = aoBandDesc[i].nWidth;
                nRasterYSize = aoBandDesc[i].nHeight;
                memcpy(m_adfGeoTransform, aoBandDesc[i].adfGeoTransform.data(),
                       6 * sizeof(double));
                m_osWKT = aoBandDesc[i].osWKT;
                int iOvr = 0;
                while( (nRasterXSize >> iOvr) > 256 ||
                       (nRasterYSize >> iOvr) > 256 )
                {
                    iOvr ++;
                    m_apoOverviewDS.push_back(
                        new GDALEEDAIDataset(this, iOvr));
                }
            }

            GDALRasterBand* poBand =
                new GDALEEDAIRasterBand(this, aoBandDesc[i].eDT,
                                        aoBandDesc[i].bSignedByte);
            const int iBand = nBands + 1;
            SetBand( iBand, poBand );
            poBand->SetDescription( aoBandDesc[i].osName );

            // as images in USDA/NAIP/DOQQ catalog
            if( EQUAL(aoBandDesc[i].osName, "R") )
                poBand->SetColorInterpretation(GCI_RedBand);
            else if( EQUAL(aoBandDesc[i].osName, "G") )
                poBand->SetColorInterpretation(GCI_GreenBand);
            else if( EQUAL(aoBandDesc[i].osName, "B") )
                poBand->SetColorInterpretation(GCI_BlueBand);

            for(size_t iOvr = 0; iOvr < m_apoOverviewDS.size(); iOvr++ )
            {
                GDALRasterBand* poOvrBand =
                    new GDALEEDAIRasterBand(m_apoOverviewDS[iOvr],
                                            aoBandDesc[i].eDT,
                                            aoBandDesc[i].bSignedByte);
                m_apoOverviewDS[iOvr]->SetBand( iBand, poOvrBand );
                poOvrBand->SetDescription( aoBandDesc[i].osName );
            }

            aoMapBandNames[ aoBandDesc[i].osName ] = iBand;
        }
        else
        {
            if( oSetUserBandNames.find(aoBandDesc[i].osName) !=
                                                oSetUserBandNames.end() )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Band %s is not compatible of other bands",
                         aoBandDesc[i].osName.c_str());
            }
            aoMapBandNames[ aoBandDesc[i].osName ] = -1;
        }

        // Group similar bands to be able to build subdataset list
        std::map< int, std::vector<int> >::iterator oIter =
                                                    oMapSimilarBands.begin();
        for( ; oIter != oMapSimilarBands.end(); ++oIter )
        {
            if( aoBandDesc[i].IsSimilar(aoBandDesc[oIter->first]) )
            {
                oIter->second.push_back(static_cast<int>(i));
                break;
            }
        }
        if( oIter == oMapSimilarBands.end() )
        {
            oMapSimilarBands[static_cast<int>(i)].push_back(static_cast<int>(i));
        }
    }

    if( !ComputeQueryStrategy() )
    {
        json_object_put(poObj);
        return false;
    }
    for( size_t i = 0; i < m_apoOverviewDS.size(); i++ )
    {
        m_apoOverviewDS[i]->ComputeQueryStrategy();
    }

    if( nBands > 1 )
    {
        SetMetadataItem("INTERLEAVE",
                        m_bQueryMultipleBands ? "PIXEL" : "BAND",
                        "IMAGE_STRUCTURE");
    }

    // Build subdataset list
    if( oSetUserBandNames.empty() && oMapSimilarBands.size() > 1 )
    {
        CPLStringList aoSubDSList;
        std::map< int, std::vector<int> >::iterator oIter =
                                                    oMapSimilarBands.begin();
        for( ; oIter != oMapSimilarBands.end(); ++oIter )
        {
            CPLString osSubDSSuffix;
            for( size_t i = 0; i < oIter->second.size(); ++i )
            {
                if( !osSubDSSuffix.empty() )
                    osSubDSSuffix += ",";
                osSubDSSuffix += aoBandDesc[oIter->second[i]].osName;
            }
            aoSubDSList.AddNameValue(
                CPLSPrintf("SUBDATASET_%d_NAME", aoSubDSList.size() / 2 + 1),
                CPLSPrintf("EEDAI:%s:%s",
                           m_osAsset.c_str(), osSubDSSuffix.c_str()) );
            aoSubDSList.AddNameValue(
                CPLSPrintf("SUBDATASET_%d_DESC", aoSubDSList.size() / 2 + 1),
                CPLSPrintf("Band%s %s of %s",
                           oIter->second.size() > 1 ? "s" : "",
                           osSubDSSuffix.c_str(), m_osAsset.c_str()) );
        }
        SetMetadata( aoSubDSList.List(), "SUBDATASETS" );
    }

    // Attach metadata to dataset or bands
    json_object* poProperties = CPL_json_object_object_get(poObj,
                                                           "properties");
    if( poProperties &&
        json_object_get_type(poProperties) == json_type_object )
    {
        SetMetadataFromProperties(poProperties, aoMapBandNames);
    }
    json_object_put(poObj);

    SetDescription( poOpenInfo->pszFilename );

    return true;
}

/************************************************************************/
/*                       SetMetadataFromProperties()                    */
/************************************************************************/

void GDALEEDAIDataset::SetMetadataFromProperties(
                json_object* poProperties,
                const std::map<CPLString, int>& aoMapBandNames )
{
    json_object_iter it;
    it.key = nullptr;
    it.val = nullptr;
    it.entry = nullptr;
    json_object_object_foreachC(poProperties, it)
    {
        if( it.val )
        {
            CPLString osKey(it.key);
            int nBandForMD = 0;
            std::map<CPLString, int>::const_iterator oIter =
                                                aoMapBandNames.begin();
            for( ; oIter != aoMapBandNames.end(); ++oIter )
            {
                CPLString osBandName(oIter->first);
                CPLString osNeedle("_" + osBandName);
                size_t nPos = osKey.find(osNeedle);
                if( nPos != std::string::npos &&
                    nPos + osNeedle.size() == osKey.size() )
                {
                    nBandForMD = oIter->second;
                    osKey.resize(nPos);
                    break;
                }

                // Landsat bands are named Bxxx, must their metadata
                // are _BAND_xxxx ...
                if( osBandName.size() > 1 && osBandName[0] == 'B' &&
                    atoi(osBandName.c_str() + 1) > 0 )
                {
                    osNeedle = "_BAND_" + osBandName.substr(1);
                    nPos = osKey.find(osNeedle);
                    if( nPos != std::string::npos &&
                        nPos + osNeedle.size() == osKey.size() )
                    {
                        nBandForMD = oIter->second;
                        osKey.resize(nPos);
                        break;
                    }
                }
            }

            if( nBandForMD > 0 )
            {
                GetRasterBand(nBandForMD)->SetMetadataItem(
                    osKey, json_object_get_string(it.val));
            }
            else if( nBandForMD == 0 )
            {
                SetMetadataItem(osKey, json_object_get_string(it.val));
            }
        }
    }
}

/************************************************************************/
/*                          GDALEEDAIIdentify()                         */
/************************************************************************/

static int GDALEEDAIIdentify(GDALOpenInfo* poOpenInfo)
{
    return STARTS_WITH_CI(poOpenInfo->pszFilename, "EEDAI:");
}


/************************************************************************/
/*                            GDALEEDAIOpen()                           */
/************************************************************************/

static GDALDataset* GDALEEDAIOpen(GDALOpenInfo* poOpenInfo)
{
    if(! GDALEEDAIIdentify(poOpenInfo) )
        return nullptr;

    GDALEEDAIDataset* poDS = new GDALEEDAIDataset();
    if( !poDS->Open(poOpenInfo) )
    {
        delete poDS;
        return nullptr;
    }
    return poDS;
}

/************************************************************************/
/*                         GDALRegister_EEDAI()                         */
/************************************************************************/

void GDALRegister_EEDAI()

{
    if( GDALGetDriverByName( "EEDAI" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "EEDAI" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "Earth Engine Data API Image" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/eedai.html" );
    poDriver->SetMetadataItem( GDAL_DMD_CONNECTION_PREFIX, "EEDAI:" );
    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='ASSET' type='string' description='Asset name'/>"
"  <Option name='BANDS' type='string' "
                        "description='Comma separated list of band names'/>"
"  <Option name='PIXEL_ENCODING' type='string-select' "
                        "description='Format in which pixls are queried'>"
"       <Value>AUTO</Value>"
"       <Value>PNG</Value>"
"       <Value>JPEG</Value>"
"       <Value>GEO_TIFF</Value>"
"       <Value>AUTO_JPEG_PNG</Value>"
"       <Value>NPY</Value>"
"   </Option>"
"  <Option name='BLOCK_SIZE' type='integer' "
                                "description='Size of a block' default='256'/>"
"</OpenOptionList>");
    poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );

    poDriver->pfnOpen = GDALEEDAIOpen;
    poDriver->pfnIdentify = GDALEEDAIIdentify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}

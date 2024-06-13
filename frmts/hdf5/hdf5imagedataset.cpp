/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  Read subdatasets of HDF5 file.
 * Author:   Denis Nadeau <denis.nadeau@gmail.com>
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "hdf5_api.h"

#include "cpl_float.h"
#include "cpl_string.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "gh5_convenience.h"
#include "hdf5dataset.h"
#include "hdf5drivercore.h"
#include "ogr_spatialref.h"
#include "../mem/memdataset.h"

#include <algorithm>

class HDF5ImageDataset final : public HDF5Dataset
{
    typedef enum
    {
        UNKNOWN_PRODUCT = 0,
        CSK_PRODUCT
    } Hdf5ProductType;

    typedef enum
    {
        PROD_UNKNOWN = 0,
        PROD_CSK_L0,
        PROD_CSK_L1A,
        PROD_CSK_L1B,
        PROD_CSK_L1C,
        PROD_CSK_L1D
    } HDF5CSKProductEnum;

    friend class HDF5ImageRasterBand;

    OGRSpatialReference m_oSRS{};
    OGRSpatialReference m_oGCPSRS{};
    std::vector<gdal::GCP> m_aoGCPs{};

    hsize_t *dims;
    hsize_t *maxdims;
    HDF5GroupObjects *poH5Objects;
    int ndims;
    int dimensions;
    hid_t dataset_id;
    hid_t dataspace_id;
    hid_t native;
#ifdef HDF5_HAVE_FLOAT16
    bool m_bConvertFromFloat16 = false;
#endif
    Hdf5ProductType iSubdatasetType;
    HDF5CSKProductEnum iCSKProductType;
    double adfGeoTransform[6];
    bool bHasGeoTransform;
    int m_nXIndex = -1;
    int m_nYIndex = -1;
    int m_nOtherDimIndex = -1;

    int m_nBlockXSize = 0;
    int m_nBlockYSize = 0;
    int m_nBandChunkSize = 1;  //! Number of bands in a chunk

    enum WholeBandChunkOptim
    {
        WBC_DETECTION_IN_PROGRESS,
        WBC_DISABLED,
        WBC_ENABLED,
    };

    //! Flag to detect if the read pattern of HDF5ImageRasterBand::IRasterIO()
    // is whole band after whole band.
    WholeBandChunkOptim m_eWholeBandChunkOptim = WBC_DETECTION_IN_PROGRESS;
    //! Value of nBand during last HDF5ImageRasterBand::IRasterIO() call
    int m_nLastRasterIOBand = -1;
    //! Value of nXOff during last HDF5ImageRasterBand::IRasterIO() call
    int m_nLastRasterIOXOff = -1;
    //! Value of nYOff during last HDF5ImageRasterBand::IRasterIO() call
    int m_nLastRasterIOYOff = -1;
    //! Value of nXSize during last HDF5ImageRasterBand::IRasterIO() call
    int m_nLastRasterIOXSize = -1;
    //! Value of nYSize during last HDF5ImageRasterBand::IRasterIO() call
    int m_nLastRasterIOYSize = -1;
    //! Value such that m_abyBandChunk represent band data in the range
    // [m_iCurrentBandChunk * m_nBandChunkSize, (m_iCurrentBandChunk+1) * m_nBandChunkSize[
    int m_iCurrentBandChunk = -1;
    //! Cached values (in native data type) for bands in the range
    // [m_iCurrentBandChunk * m_nBandChunkSize, (m_iCurrentBandChunk+1) * m_nBandChunkSize[
    std::vector<GByte> m_abyBandChunk{};

    CPLErr CreateODIMH5Projection();

  public:
    HDF5ImageDataset();
    virtual ~HDF5ImageDataset();

    CPLErr CreateProjections();
    static GDALDataset *Open(GDALOpenInfo *);
    static int Identify(GDALOpenInfo *);

    const OGRSpatialReference *GetSpatialRef() const override;
    virtual int GetGCPCount() override;
    const OGRSpatialReference *GetGCPSpatialRef() const override;
    virtual const GDAL_GCP *GetGCPs() override;
    virtual CPLErr GetGeoTransform(double *padfTransform) override;

    CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                     int nYSize, void *pData, int nBufXSize, int nBufYSize,
                     GDALDataType eBufType, int nBandCount, int *panBandMap,
                     GSpacing nPixelSpace, GSpacing nLineSpace,
                     GSpacing nBandSpace,
                     GDALRasterIOExtraArg *psExtraArg) override;

    const char *GetMetadataItem(const char *pszName,
                                const char *pszDomain = "") override;

    Hdf5ProductType GetSubdatasetType() const
    {
        return iSubdatasetType;
    }

    HDF5CSKProductEnum GetCSKProductType() const
    {
        return iCSKProductType;
    }

    int IsComplexCSKL1A() const
    {
        return GetSubdatasetType() == CSK_PRODUCT &&
               GetCSKProductType() == PROD_CSK_L1A && ndims == 3;
    }

    int GetYIndex() const
    {
        return m_nYIndex;
    }

    int GetXIndex() const
    {
        return m_nXIndex;
    }

    /**
     * Identify if the subdataset has a known product format
     * It stores a product identifier in iSubdatasetType,
     * UNKNOWN_PRODUCT, if it isn't a recognizable format.
     */
    void IdentifyProductType();

    /**
     * Captures Geolocation information from a COSMO-SKYMED
     * file.
     * The geoid will always be WGS84
     * The projection type may be UTM or UPS, depending on the
     * latitude from the center of the image.
     * @param iProductType type of HDF5 subproduct, see HDF5CSKProduct
     */
    void CaptureCSKGeolocation(int iProductType);

    /**
     * Get Geotransform information for COSMO-SKYMED files
     * In case of success it stores the transformation
     * in adfGeoTransform. In case of failure it doesn't
     * modify adfGeoTransform
     * @param iProductType type of HDF5 subproduct, see HDF5CSKProduct
     */
    void CaptureCSKGeoTransform(int iProductType);

    /**
     * @param iProductType type of HDF5 subproduct, see HDF5CSKProduct
     */
    void CaptureCSKGCPs(int iProductType);
};

/************************************************************************/
/* ==================================================================== */
/*                              HDF5ImageDataset                        */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           HDF5ImageDataset()                         */
/************************************************************************/
HDF5ImageDataset::HDF5ImageDataset()
    : dims(nullptr), maxdims(nullptr), poH5Objects(nullptr), ndims(0),
      dimensions(0), dataset_id(-1), dataspace_id(-1), native(-1),
      iSubdatasetType(UNKNOWN_PRODUCT), iCSKProductType(PROD_UNKNOWN),
      bHasGeoTransform(false)
{
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    m_oGCPSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~HDF5ImageDataset()                       */
/************************************************************************/
HDF5ImageDataset::~HDF5ImageDataset()
{
    HDF5_GLOBAL_LOCK();

    FlushCache(true);

    if (dataset_id > 0)
        H5Dclose(dataset_id);
    if (dataspace_id > 0)
        H5Sclose(dataspace_id);
    if (native > 0)
        H5Tclose(native);

    CPLFree(dims);
    CPLFree(maxdims);
}

/************************************************************************/
/* ==================================================================== */
/*                            Hdf5imagerasterband                       */
/* ==================================================================== */
/************************************************************************/
class HDF5ImageRasterBand final : public GDALPamRasterBand
{
    friend class HDF5ImageDataset;

    bool bNoDataSet = false;
    double dfNoDataValue = -9999.0;
    bool m_bHasOffset = false;
    double m_dfOffset = 0.0;
    bool m_bHasScale = false;
    double m_dfScale = 1.0;
    int m_nIRasterIORecCounter = 0;

  public:
    HDF5ImageRasterBand(HDF5ImageDataset *, int, GDALDataType);
    virtual ~HDF5ImageRasterBand();

    virtual CPLErr IReadBlock(int, int, void *) override;
    virtual double GetNoDataValue(int *) override;
    virtual double GetOffset(int *) override;
    virtual double GetScale(int *) override;
    // virtual CPLErr IWriteBlock( int, int, void * );

    CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                     int nYSize, void *pData, int nBufXSize, int nBufYSize,
                     GDALDataType eBufType, GSpacing nPixelSpace,
                     GSpacing nLineSpace,
                     GDALRasterIOExtraArg *psExtraArg) override;
};

/************************************************************************/
/*                        ~HDF5ImageRasterBand()                        */
/************************************************************************/

HDF5ImageRasterBand::~HDF5ImageRasterBand()
{
}

/************************************************************************/
/*                           HDF5ImageRasterBand()                      */
/************************************************************************/
HDF5ImageRasterBand::HDF5ImageRasterBand(HDF5ImageDataset *poDSIn, int nBandIn,
                                         GDALDataType eType)
{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType = eType;
    nBlockXSize = poDSIn->m_nBlockXSize;
    nBlockYSize = poDSIn->m_nBlockYSize;

    // netCDF convention for nodata
    bNoDataSet =
        GH5_FetchAttribute(poDSIn->dataset_id, "_FillValue", dfNoDataValue);
    if (!bNoDataSet)
        dfNoDataValue = -9999.0;

    // netCDF conventions for scale and offset
    m_bHasOffset =
        GH5_FetchAttribute(poDSIn->dataset_id, "add_offset", m_dfOffset);
    if (!m_bHasOffset)
        m_dfOffset = 0.0;
    m_bHasScale =
        GH5_FetchAttribute(poDSIn->dataset_id, "scale_factor", m_dfScale);
    if (!m_bHasScale)
        m_dfScale = 1.0;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/
double HDF5ImageRasterBand::GetNoDataValue(int *pbSuccess)

{
    if (bNoDataSet)
    {
        if (pbSuccess)
            *pbSuccess = bNoDataSet;

        return dfNoDataValue;
    }

    return GDALPamRasterBand::GetNoDataValue(pbSuccess);
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

double HDF5ImageRasterBand::GetOffset(int *pbSuccess)

{
    if (m_bHasOffset)
    {
        if (pbSuccess)
            *pbSuccess = m_bHasOffset;

        return m_dfOffset;
    }

    return GDALPamRasterBand::GetOffset(pbSuccess);
}

/************************************************************************/
/*                             GetScale()                               */
/************************************************************************/

double HDF5ImageRasterBand::GetScale(int *pbSuccess)

{
    if (m_bHasScale)
    {
        if (pbSuccess)
            *pbSuccess = m_bHasScale;

        return m_dfScale;
    }

    return GDALPamRasterBand::GetScale(pbSuccess);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/
CPLErr HDF5ImageRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                       void *pImage)
{
    HDF5ImageDataset *poGDS = static_cast<HDF5ImageDataset *>(poDS);

    memset(pImage, 0,
           static_cast<size_t>(nBlockXSize) * nBlockYSize *
               GDALGetDataTypeSizeBytes(eDataType));

    if (poGDS->eAccess == GA_Update)
    {
        return CE_None;
    }

    const int nXOff = nBlockXOff * nBlockXSize;
    const int nYOff = nBlockYOff * nBlockYSize;
    const int nXSize = std::min(nBlockXSize, nRasterXSize - nXOff);
    const int nYSize = std::min(nBlockYSize, nRasterYSize - nYOff);
    if (poGDS->m_eWholeBandChunkOptim == HDF5ImageDataset::WBC_ENABLED)
    {
        const bool bIsBandInterleavedData =
            poGDS->ndims == 3 && poGDS->m_nOtherDimIndex == 0 &&
            poGDS->GetYIndex() == 1 && poGDS->GetXIndex() == 2;
        if (poGDS->nBands == 1 || bIsBandInterleavedData)
        {
            GDALRasterIOExtraArg sExtraArg;
            INIT_RASTERIO_EXTRA_ARG(sExtraArg);
            const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
            return IRasterIO(GF_Read, nXOff, nYOff, nXSize, nYSize, pImage,
                             nXSize, nYSize, eDataType, nDTSize,
                             static_cast<GSpacing>(nDTSize) * nBlockXSize,
                             &sExtraArg);
        }
    }

    HDF5_GLOBAL_LOCK();

    hsize_t count[3] = {0, 0, 0};
    H5OFFSET_TYPE offset[3] = {0, 0, 0};
    hsize_t col_dims[3] = {0, 0, 0};
    hsize_t rank = std::min(poGDS->ndims, 2);

    if (poGDS->ndims == 3)
    {
        rank = 3;
        offset[poGDS->m_nOtherDimIndex] = nBand - 1;
        count[poGDS->m_nOtherDimIndex] = 1;
        col_dims[poGDS->m_nOtherDimIndex] = 1;
    }

    const int nYIndex = poGDS->GetYIndex();
    // Blocksize may not be a multiple of imagesize.
    if (nYIndex >= 0)
    {
        offset[nYIndex] = nYOff;
        count[nYIndex] = nYSize;
    }
    offset[poGDS->GetXIndex()] = nXOff;
    count[poGDS->GetXIndex()] = nXSize;

    // Select block from file space.
    herr_t status = H5Sselect_hyperslab(poGDS->dataspace_id, H5S_SELECT_SET,
                                        offset, nullptr, count, nullptr);
    if (status < 0)
        return CE_Failure;

    // Create memory space to receive the data.
    if (nYIndex >= 0)
        col_dims[nYIndex] = nBlockYSize;
    col_dims[poGDS->GetXIndex()] = nBlockXSize;

    const hid_t memspace =
        H5Screate_simple(static_cast<int>(rank), col_dims, nullptr);
    H5OFFSET_TYPE mem_offset[3] = {0, 0, 0};
    status = H5Sselect_hyperslab(memspace, H5S_SELECT_SET, mem_offset, nullptr,
                                 count, nullptr);
    if (status < 0)
    {
        H5Sclose(memspace);
        return CE_Failure;
    }

    status = H5Dread(poGDS->dataset_id, poGDS->native, memspace,
                     poGDS->dataspace_id, H5P_DEFAULT, pImage);

    H5Sclose(memspace);

    if (status < 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "H5Dread() failed for block.");
        return CE_Failure;
    }

#ifdef HDF5_HAVE_FLOAT16
    if (eDataType == GDT_Float32 && poGDS->m_bConvertFromFloat16)
    {
        for (size_t i = static_cast<size_t>(nBlockXSize) * nBlockYSize; i > 0;
             /* do nothing */)
        {
            --i;
            uint16_t nVal16;
            memcpy(&nVal16, static_cast<uint16_t *>(pImage) + i,
                   sizeof(nVal16));
            const uint32_t nVal32 = CPLHalfToFloat(nVal16);
            float fVal;
            memcpy(&fVal, &nVal32, sizeof(fVal));
            *(static_cast<float *>(pImage) + i) = fVal;
        }
    }
    else if (eDataType == GDT_CFloat32 && poGDS->m_bConvertFromFloat16)
    {
        for (size_t i = static_cast<size_t>(nBlockXSize) * nBlockYSize; i > 0;
             /* do nothing */)
        {
            --i;
            for (int j = 1; j >= 0; --j)
            {
                uint16_t nVal16;
                memcpy(&nVal16, static_cast<uint16_t *>(pImage) + 2 * i + j,
                       sizeof(nVal16));
                const uint32_t nVal32 = CPLHalfToFloat(nVal16);
                float fVal;
                memcpy(&fVal, &nVal32, sizeof(fVal));
                *(static_cast<float *>(pImage) + 2 * i + j) = fVal;
            }
        }
    }
#endif

    return CE_None;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr HDF5ImageRasterBand::IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                                      int nXSize, int nYSize, void *pData,
                                      int nBufXSize, int nBufYSize,
                                      GDALDataType eBufType,
                                      GSpacing nPixelSpace, GSpacing nLineSpace,
                                      GDALRasterIOExtraArg *psExtraArg)

{
    HDF5ImageDataset *poGDS = static_cast<HDF5ImageDataset *>(poDS);

#ifdef HDF5_HAVE_FLOAT16
    if (poGDS->m_bConvertFromFloat16)
    {
        return GDALPamRasterBand::IRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, nPixelSpace, nLineSpace, psExtraArg);
    }
#endif

    const bool bIsBandInterleavedData =
        poGDS->ndims == 3 && poGDS->m_nOtherDimIndex == 0 &&
        poGDS->GetYIndex() == 1 && poGDS->GetXIndex() == 2;

    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);

    // Try to detect if we read whole bands by chunks of whole lines
    // If so, then read and cache whole band (or group of m_nBandChunkSize bands)
    // to save HDF5 decompression.
    if (m_nIRasterIORecCounter == 0)
    {
        bool bInvalidateWholeBandChunkOptim = false;
        if (!(nXSize == nBufXSize && nYSize == nBufYSize))
        {
            bInvalidateWholeBandChunkOptim = true;
        }
        // Is the first request on band 1, line 0 and one or several full lines?
        else if (poGDS->m_eWholeBandChunkOptim !=
                     HDF5ImageDataset::WBC_ENABLED &&
                 nBand == 1 && nXOff == 0 && nYOff == 0 &&
                 nXSize == nRasterXSize)
        {
            poGDS->m_eWholeBandChunkOptim =
                HDF5ImageDataset::WBC_DETECTION_IN_PROGRESS;
            poGDS->m_nLastRasterIOBand = 1;
            poGDS->m_nLastRasterIOXOff = nXOff;
            poGDS->m_nLastRasterIOYOff = nYOff;
            poGDS->m_nLastRasterIOXSize = nXSize;
            poGDS->m_nLastRasterIOYSize = nYSize;
        }
        else if (poGDS->m_eWholeBandChunkOptim ==
                 HDF5ImageDataset::WBC_DETECTION_IN_PROGRESS)
        {
            if (poGDS->m_nLastRasterIOBand == 1 && nBand == 1)
            {
                // Is this request a continuation of the previous one?
                if (nXOff == 0 && poGDS->m_nLastRasterIOXOff == 0 &&
                    nYOff == poGDS->m_nLastRasterIOYOff +
                                 poGDS->m_nLastRasterIOYSize &&
                    poGDS->m_nLastRasterIOXSize == nRasterXSize &&
                    nXSize == nRasterXSize)
                {
                    poGDS->m_nLastRasterIOXOff = nXOff;
                    poGDS->m_nLastRasterIOYOff = nYOff;
                    poGDS->m_nLastRasterIOXSize = nXSize;
                    poGDS->m_nLastRasterIOYSize = nYSize;
                }
                else
                {
                    bInvalidateWholeBandChunkOptim = true;
                }
            }
            else if (poGDS->m_nLastRasterIOBand == 1 && nBand == 2)
            {
                // Are we switching to band 2 while having fully read band 1?
                if (nXOff == 0 && nYOff == 0 && nXSize == nRasterXSize &&
                    poGDS->m_nLastRasterIOXOff == 0 &&
                    poGDS->m_nLastRasterIOXSize == nRasterXSize &&
                    poGDS->m_nLastRasterIOYOff + poGDS->m_nLastRasterIOYSize ==
                        nRasterYSize)
                {
                    if ((poGDS->m_nBandChunkSize > 1 ||
                         nBufYSize < nRasterYSize) &&
                        static_cast<int64_t>(poGDS->m_nBandChunkSize) *
                                nRasterXSize * nRasterYSize * nDTSize <
                            CPLGetUsablePhysicalRAM() / 10)
                    {
                        poGDS->m_eWholeBandChunkOptim =
                            HDF5ImageDataset::WBC_ENABLED;
                    }
                    else
                    {
                        bInvalidateWholeBandChunkOptim = true;
                    }
                }
                else
                {
                    bInvalidateWholeBandChunkOptim = true;
                }
            }
            else
            {
                bInvalidateWholeBandChunkOptim = true;
            }
        }
        if (bInvalidateWholeBandChunkOptim)
        {
            poGDS->m_eWholeBandChunkOptim = HDF5ImageDataset::WBC_DISABLED;
            poGDS->m_nLastRasterIOBand = -1;
            poGDS->m_nLastRasterIOXOff = -1;
            poGDS->m_nLastRasterIOYOff = -1;
            poGDS->m_nLastRasterIOXSize = -1;
            poGDS->m_nLastRasterIOYSize = -1;
        }
    }

    if (poGDS->m_eWholeBandChunkOptim == HDF5ImageDataset::WBC_ENABLED &&
        nXSize == nBufXSize && nYSize == nBufYSize)
    {
        if (poGDS->nBands == 1 || bIsBandInterleavedData)
        {
            if (poGDS->m_iCurrentBandChunk < 0)
                CPLDebug("HDF5", "Using whole band chunk caching");
            const int iBandChunk = (nBand - 1) / poGDS->m_nBandChunkSize;
            if (iBandChunk != poGDS->m_iCurrentBandChunk)
            {
                poGDS->m_abyBandChunk.resize(
                    static_cast<size_t>(poGDS->m_nBandChunkSize) *
                    nRasterXSize * nRasterYSize * nDTSize);

                HDF5_GLOBAL_LOCK();

                hsize_t count[3] = {
                    std::min(static_cast<hsize_t>(poGDS->nBands),
                             static_cast<hsize_t>(iBandChunk + 1) *
                                 poGDS->m_nBandChunkSize) -
                        static_cast<hsize_t>(iBandChunk) *
                            poGDS->m_nBandChunkSize,
                    static_cast<hsize_t>(nRasterYSize),
                    static_cast<hsize_t>(nRasterXSize)};
                H5OFFSET_TYPE offset[3] = {
                    static_cast<H5OFFSET_TYPE>(iBandChunk) *
                        poGDS->m_nBandChunkSize,
                    static_cast<H5OFFSET_TYPE>(0),
                    static_cast<H5OFFSET_TYPE>(0)};
                herr_t status =
                    H5Sselect_hyperslab(poGDS->dataspace_id, H5S_SELECT_SET,
                                        offset, nullptr, count, nullptr);
                if (status < 0)
                    return CE_Failure;

                const hid_t memspace =
                    H5Screate_simple(poGDS->ndims, count, nullptr);
                H5OFFSET_TYPE mem_offset[3] = {0, 0, 0};
                status =
                    H5Sselect_hyperslab(memspace, H5S_SELECT_SET, mem_offset,
                                        nullptr, count, nullptr);
                if (status < 0)
                {
                    H5Sclose(memspace);
                    return CE_Failure;
                }

                status = H5Dread(poGDS->dataset_id, poGDS->native, memspace,
                                 poGDS->dataspace_id, H5P_DEFAULT,
                                 poGDS->m_abyBandChunk.data());

                H5Sclose(memspace);

                if (status < 0)
                {
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "HDF5ImageRasterBand::IRasterIO(): H5Dread() failed");
                    return CE_Failure;
                }

                poGDS->m_iCurrentBandChunk = iBandChunk;
            }

            for (int iY = 0; iY < nYSize; ++iY)
            {
                GDALCopyWords(poGDS->m_abyBandChunk.data() +
                                  static_cast<size_t>((nBand - 1) %
                                                      poGDS->m_nBandChunkSize) *
                                      nRasterYSize * nRasterXSize * nDTSize +
                                  static_cast<size_t>(nYOff + iY) *
                                      nRasterXSize * nDTSize +
                                  nXOff * nDTSize,
                              eDataType, nDTSize,
                              static_cast<GByte *>(pData) +
                                  static_cast<size_t>(iY) * nLineSpace,
                              eBufType, static_cast<int>(nPixelSpace), nXSize);
            }
            return CE_None;
        }
    }

    const bool bIsExpectedLayout =
        (bIsBandInterleavedData ||
         (poGDS->ndims == 2 && poGDS->GetYIndex() == 0 &&
          poGDS->GetXIndex() == 1));
    if (eRWFlag == GF_Read && bIsExpectedLayout && nXSize == nBufXSize &&
        nYSize == nBufYSize && eBufType == eDataType &&
        nPixelSpace == nDTSize && nLineSpace == nXSize * nPixelSpace)
    {
        HDF5_GLOBAL_LOCK();

        hsize_t count[3] = {1, static_cast<hsize_t>(nYSize),
                            static_cast<hsize_t>(nXSize)};
        H5OFFSET_TYPE offset[3] = {static_cast<H5OFFSET_TYPE>(nBand - 1),
                                   static_cast<H5OFFSET_TYPE>(nYOff),
                                   static_cast<H5OFFSET_TYPE>(nXOff)};
        if (poGDS->ndims == 2)
        {
            count[0] = count[1];
            count[1] = count[2];

            offset[0] = offset[1];
            offset[1] = offset[2];
        }
        herr_t status = H5Sselect_hyperslab(poGDS->dataspace_id, H5S_SELECT_SET,
                                            offset, nullptr, count, nullptr);
        if (status < 0)
            return CE_Failure;

        const hid_t memspace = H5Screate_simple(poGDS->ndims, count, nullptr);
        H5OFFSET_TYPE mem_offset[3] = {0, 0, 0};
        status = H5Sselect_hyperslab(memspace, H5S_SELECT_SET, mem_offset,
                                     nullptr, count, nullptr);
        if (status < 0)
        {
            H5Sclose(memspace);
            return CE_Failure;
        }

        status = H5Dread(poGDS->dataset_id, poGDS->native, memspace,
                         poGDS->dataspace_id, H5P_DEFAULT, pData);

        H5Sclose(memspace);

        if (status < 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "HDF5ImageRasterBand::IRasterIO(): H5Dread() failed");
            return CE_Failure;
        }

        return CE_None;
    }

    // If the request is still small enough, try to read from libhdf5 with
    // the natural interleaving into a temporary MEMDataset, and then read
    // from it with the requested interleaving and data type.
    if (eRWFlag == GF_Read && bIsExpectedLayout && nXSize == nBufXSize &&
        nYSize == nBufYSize &&
        static_cast<GIntBig>(nXSize) * nYSize < CPLGetUsablePhysicalRAM() / 10)
    {
        auto poMemDS = std::unique_ptr<GDALDataset>(
            MEMDataset::Create("", nXSize, nYSize, 1, eDataType, nullptr));
        if (poMemDS)
        {
            void *pMemData = poMemDS->GetInternalHandle("MEMORY1");
            CPLAssert(pMemData);
            // Read from HDF5 into the temporary MEMDataset using the
            // natural interleaving of the HDF5 dataset
            ++m_nIRasterIORecCounter;
            CPLErr eErr =
                IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize, pMemData,
                          nXSize, nYSize, eDataType, nDTSize,
                          static_cast<GSpacing>(nXSize) * nDTSize, psExtraArg);
            --m_nIRasterIORecCounter;
            if (eErr != CE_None)
            {
                return CE_Failure;
            }
            // Copy to the final buffer using requested data type and spacings.
            return poMemDS->GetRasterBand(1)->RasterIO(
                GF_Read, 0, 0, nXSize, nYSize, pData, nXSize, nYSize, eBufType,
                nPixelSpace, nLineSpace, nullptr);
        }
    }

    return GDALPamRasterBand::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                        pData, nBufXSize, nBufYSize, eBufType,
                                        nPixelSpace, nLineSpace, psExtraArg);
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr HDF5ImageDataset::IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                                   int nXSize, int nYSize, void *pData,
                                   int nBufXSize, int nBufYSize,
                                   GDALDataType eBufType, int nBandCount,
                                   int *panBandMap, GSpacing nPixelSpace,
                                   GSpacing nLineSpace, GSpacing nBandSpace,
                                   GDALRasterIOExtraArg *psExtraArg)

{
#ifdef HDF5_HAVE_FLOAT16
    if (m_bConvertFromFloat16)
    {
        return HDF5Dataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                      pData, nBufXSize, nBufYSize, eBufType,
                                      nBandCount, panBandMap, nPixelSpace,
                                      nLineSpace, nBandSpace, psExtraArg);
    }
#endif

    const auto IsConsecutiveBands = [](const int *panVals, int nCount)
    {
        for (int i = 1; i < nCount; ++i)
        {
            if (panVals[i] != panVals[i - 1] + 1)
                return false;
        }
        return true;
    };

    const auto eDT = GetRasterBand(1)->GetRasterDataType();
    const int nDTSize = GDALGetDataTypeSizeBytes(eDT);

    // Band-interleaved data and request
    const bool bIsBandInterleavedData = ndims == 3 && m_nOtherDimIndex == 0 &&
                                        GetYIndex() == 1 && GetXIndex() == 2;
    if (eRWFlag == GF_Read && bIsBandInterleavedData && nXSize == nBufXSize &&
        nYSize == nBufYSize && IsConsecutiveBands(panBandMap, nBandCount) &&
        eBufType == eDT && nPixelSpace == nDTSize &&
        nLineSpace == nXSize * nPixelSpace && nBandSpace == nYSize * nLineSpace)
    {
        HDF5_GLOBAL_LOCK();

        hsize_t count[3] = {static_cast<hsize_t>(nBandCount),
                            static_cast<hsize_t>(nYSize),
                            static_cast<hsize_t>(nXSize)};
        H5OFFSET_TYPE offset[3] = {
            static_cast<H5OFFSET_TYPE>(panBandMap[0] - 1),
            static_cast<H5OFFSET_TYPE>(nYOff),
            static_cast<H5OFFSET_TYPE>(nXOff)};
        herr_t status = H5Sselect_hyperslab(dataspace_id, H5S_SELECT_SET,
                                            offset, nullptr, count, nullptr);
        if (status < 0)
            return CE_Failure;

        const hid_t memspace = H5Screate_simple(ndims, count, nullptr);
        H5OFFSET_TYPE mem_offset[3] = {0, 0, 0};
        status = H5Sselect_hyperslab(memspace, H5S_SELECT_SET, mem_offset,
                                     nullptr, count, nullptr);
        if (status < 0)
        {
            H5Sclose(memspace);
            return CE_Failure;
        }

        status = H5Dread(dataset_id, native, memspace, dataspace_id,
                         H5P_DEFAULT, pData);

        H5Sclose(memspace);

        if (status < 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "HDF5ImageDataset::IRasterIO(): H5Dread() failed");
            return CE_Failure;
        }

        return CE_None;
    }

    // Pixel-interleaved data and request

    const bool bIsPixelInterleaveData = ndims == 3 && m_nOtherDimIndex == 2 &&
                                        GetYIndex() == 0 && GetXIndex() == 1;
    if (eRWFlag == GF_Read && bIsPixelInterleaveData && nXSize == nBufXSize &&
        nYSize == nBufYSize && IsConsecutiveBands(panBandMap, nBandCount) &&
        eBufType == eDT && nBandSpace == nDTSize &&
        nPixelSpace == nBandCount * nBandSpace &&
        nLineSpace == nXSize * nPixelSpace)
    {
        HDF5_GLOBAL_LOCK();

        hsize_t count[3] = {static_cast<hsize_t>(nYSize),
                            static_cast<hsize_t>(nXSize),
                            static_cast<hsize_t>(nBandCount)};
        H5OFFSET_TYPE offset[3] = {
            static_cast<H5OFFSET_TYPE>(nYOff),
            static_cast<H5OFFSET_TYPE>(nXOff),
            static_cast<H5OFFSET_TYPE>(panBandMap[0] - 1)};
        herr_t status = H5Sselect_hyperslab(dataspace_id, H5S_SELECT_SET,
                                            offset, nullptr, count, nullptr);
        if (status < 0)
            return CE_Failure;

        const hid_t memspace = H5Screate_simple(ndims, count, nullptr);
        H5OFFSET_TYPE mem_offset[3] = {0, 0, 0};
        status = H5Sselect_hyperslab(memspace, H5S_SELECT_SET, mem_offset,
                                     nullptr, count, nullptr);
        if (status < 0)
        {
            H5Sclose(memspace);
            return CE_Failure;
        }

        status = H5Dread(dataset_id, native, memspace, dataspace_id,
                         H5P_DEFAULT, pData);

        H5Sclose(memspace);

        if (status < 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "HDF5ImageDataset::IRasterIO(): H5Dread() failed");
            return CE_Failure;
        }

        return CE_None;
    }

    // If the request is still small enough, try to read from libhdf5 with
    // the natural interleaving into a temporary MEMDataset, and then read
    // from it with the requested interleaving and data type.
    if (eRWFlag == GF_Read &&
        (bIsBandInterleavedData || bIsPixelInterleaveData) &&
        nXSize == nBufXSize && nYSize == nBufYSize &&
        IsConsecutiveBands(panBandMap, nBandCount) &&
        static_cast<GIntBig>(nXSize) * nYSize <
            CPLGetUsablePhysicalRAM() / 10 / nBandCount)
    {
        const char *const apszOptions[] = {
            bIsPixelInterleaveData ? "INTERLEAVE=PIXEL" : nullptr, nullptr};
        auto poMemDS = std::unique_ptr<GDALDataset>(
            MEMDataset::Create("", nXSize, nYSize, nBandCount, eDT,
                               const_cast<char **>(apszOptions)));
        if (poMemDS)
        {
            void *pMemData = poMemDS->GetInternalHandle("MEMORY1");
            CPLAssert(pMemData);
            // Read from HDF5 into the temporary MEMDataset using the
            // natural interleaving of the HDF5 dataset
            if (IRasterIO(
                    eRWFlag, nXOff, nYOff, nXSize, nYSize, pMemData, nXSize,
                    nYSize, eDT, nBandCount, panBandMap,
                    bIsBandInterleavedData ? nDTSize : nDTSize * nBandCount,
                    bIsBandInterleavedData
                        ? static_cast<GSpacing>(nXSize) * nDTSize
                        : static_cast<GSpacing>(nXSize) * nDTSize * nBandCount,
                    bIsBandInterleavedData
                        ? static_cast<GSpacing>(nYSize) * nXSize * nDTSize
                        : nDTSize,
                    psExtraArg) != CE_None)
            {
                return CE_Failure;
            }
            // Copy to the final buffer using requested data type and spacings.
            return poMemDS->RasterIO(GF_Read, 0, 0, nXSize, nYSize, pData,
                                     nXSize, nYSize, eBufType, nBandCount,
                                     nullptr, nPixelSpace, nLineSpace,
                                     nBandSpace, nullptr);
        }
    }

    return HDF5Dataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize, pData,
                                  nBufXSize, nBufYSize, eBufType, nBandCount,
                                  panBandMap, nPixelSpace, nLineSpace,
                                  nBandSpace, psExtraArg);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/
GDALDataset *HDF5ImageDataset::Open(GDALOpenInfo *poOpenInfo)
{
    if (!STARTS_WITH_CI(poOpenInfo->pszFilename, "HDF5:"))
        return nullptr;

    HDF5_GLOBAL_LOCK();

    // Confirm the requested access is supported.
    if (poOpenInfo->eAccess == GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The HDF5ImageDataset driver does not support update access "
                 "to existing datasets.");
        return nullptr;
    }

    HDF5ImageDataset *poDS = new HDF5ImageDataset();

    // Create a corresponding GDALDataset.
    char **papszName =
        CSLTokenizeString2(poOpenInfo->pszFilename, ":",
                           CSLT_HONOURSTRINGS | CSLT_PRESERVEESCAPES);

    if (!(CSLCount(papszName) == 3 || CSLCount(papszName) == 4))
    {
        CSLDestroy(papszName);
        delete poDS;
        return nullptr;
    }

    poDS->SetDescription(poOpenInfo->pszFilename);

    // Check for drive name in windows HDF5:"D:\...
    CPLString osSubdatasetName;

    CPLString osFilename(papszName[1]);

    if ((strlen(papszName[1]) == 1 && papszName[3] != nullptr) ||
        (STARTS_WITH(papszName[1], "/vsicurl/http") && papszName[3] != nullptr))
    {
        osFilename += ":";
        osFilename += papszName[2];
        osSubdatasetName = papszName[3];
    }
    else
    {
        osSubdatasetName = papszName[2];
    }

    poDS->SetSubdatasetName(osSubdatasetName);

    CSLDestroy(papszName);
    papszName = nullptr;

    poDS->SetPhysicalFilename(osFilename);

    // Try opening the dataset.
    poDS->m_hHDF5 = GDAL_HDF5Open(osFilename);
    if (poDS->m_hHDF5 < 0)
    {
        delete poDS;
        return nullptr;
    }

    poDS->hGroupID = H5Gopen(poDS->m_hHDF5, "/");
    if (poDS->hGroupID < 0)
    {
        delete poDS;
        return nullptr;
    }

    // This is an HDF5 file.
    poDS->ReadGlobalAttributes(FALSE);

    // Create HDF5 Data Hierarchy in a link list.
    poDS->poH5Objects = poDS->HDF5FindDatasetObjectsbyPath(poDS->poH5RootGroup,
                                                           osSubdatasetName);

    if (poDS->poH5Objects == nullptr)
    {
        delete poDS;
        return nullptr;
    }

    // Retrieve HDF5 data information.
    poDS->dataset_id = H5Dopen(poDS->m_hHDF5, poDS->poH5Objects->pszPath);
    poDS->dataspace_id = H5Dget_space(poDS->dataset_id);
    poDS->ndims = H5Sget_simple_extent_ndims(poDS->dataspace_id);
    if (poDS->ndims <= 0)
    {
        delete poDS;
        return nullptr;
    }
    poDS->dims =
        static_cast<hsize_t *>(CPLCalloc(poDS->ndims, sizeof(hsize_t)));
    poDS->maxdims =
        static_cast<hsize_t *>(CPLCalloc(poDS->ndims, sizeof(hsize_t)));
    poDS->dimensions = H5Sget_simple_extent_dims(poDS->dataspace_id, poDS->dims,
                                                 poDS->maxdims);
    auto datatype = H5Dget_type(poDS->dataset_id);
    poDS->native = H5Tget_native_type(datatype, H5T_DIR_ASCEND);
    H5Tclose(datatype);

    const auto eGDALDataType = poDS->GetDataType(poDS->native);
    if (eGDALDataType == GDT_Unknown)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unhandled HDF5 data type");
        delete poDS;
        return nullptr;
    }

#ifdef HDF5_HAVE_FLOAT16
    if (H5Tequal(H5T_NATIVE_FLOAT16, poDS->native) ||
        IsNativeCFloat16(poDS->native))
    {
        poDS->m_bConvertFromFloat16 = true;
    }
#endif

    // CSK code in IdentifyProductType() and CreateProjections()
    // uses dataset metadata.
    poDS->SetMetadata(poDS->m_aosMetadata.List());

    // Check if the hdf5 is a well known product type
    poDS->IdentifyProductType();

    poDS->m_nYIndex = poDS->IsComplexCSKL1A() ? 0 : poDS->ndims - 2;
    poDS->m_nXIndex = poDS->IsComplexCSKL1A() ? 1 : poDS->ndims - 1;

    if (poDS->IsComplexCSKL1A())
    {
        poDS->m_nOtherDimIndex = 2;
    }
    else if (poDS->ndims == 3)
    {
        poDS->m_nOtherDimIndex = 0;
    }

    if (HDF5EOSParser::HasHDFEOS(poDS->hGroupID))
    {
        HDF5EOSParser oHDFEOSParser;
        if (oHDFEOSParser.Parse(poDS->hGroupID))
        {
            CPLDebug("HDF5", "Successfully parsed HDFEOS metadata");
            HDF5EOSParser::GridDataFieldMetadata oGridDataFieldMetadata;
            HDF5EOSParser::SwathDataFieldMetadata oSwathDataFieldMetadata;
            if (oHDFEOSParser.GetDataModel() ==
                    HDF5EOSParser::DataModel::GRID &&
                oHDFEOSParser.GetGridDataFieldMetadata(
                    osSubdatasetName.c_str(), oGridDataFieldMetadata) &&
                static_cast<int>(oGridDataFieldMetadata.aoDimensions.size()) ==
                    poDS->ndims)
            {
                int iDim = 0;
                for (const auto &oDim : oGridDataFieldMetadata.aoDimensions)
                {
                    if (oDim.osName == "XDim")
                        poDS->m_nXIndex = iDim;
                    else if (oDim.osName == "YDim")
                        poDS->m_nYIndex = iDim;
                    else
                        poDS->m_nOtherDimIndex = iDim;
                    ++iDim;
                }

                if (oGridDataFieldMetadata.poGridMetadata->GetGeoTransform(
                        poDS->adfGeoTransform))
                    poDS->bHasGeoTransform = true;

                auto poSRS = oGridDataFieldMetadata.poGridMetadata->GetSRS();
                if (poSRS)
                    poDS->m_oSRS = *(poSRS.get());
            }
            else if (oHDFEOSParser.GetDataModel() ==
                         HDF5EOSParser::DataModel::SWATH &&
                     oHDFEOSParser.GetSwathDataFieldMetadata(
                         osSubdatasetName.c_str(), oSwathDataFieldMetadata) &&
                     static_cast<int>(
                         oSwathDataFieldMetadata.aoDimensions.size()) ==
                         poDS->ndims &&
                     oSwathDataFieldMetadata.iXDim >= 0 &&
                     oSwathDataFieldMetadata.iYDim >= 0)
            {
                poDS->m_nXIndex = oSwathDataFieldMetadata.iXDim;
                poDS->m_nYIndex = oSwathDataFieldMetadata.iYDim;
                poDS->m_nOtherDimIndex = oSwathDataFieldMetadata.iOtherDim;
                if (!oSwathDataFieldMetadata.osLongitudeSubdataset.empty())
                {
                    // Arbitrary
                    poDS->SetMetadataItem("SRS", SRS_WKT_WGS84_LAT_LONG,
                                          "GEOLOCATION");
                    poDS->SetMetadataItem(
                        "X_DATASET",
                        ("HDF5:\"" + osFilename +
                         "\":" + oSwathDataFieldMetadata.osLongitudeSubdataset)
                            .c_str(),
                        "GEOLOCATION");
                    poDS->SetMetadataItem("X_BAND", "1", "GEOLOCATION");
                    poDS->SetMetadataItem(
                        "Y_DATASET",
                        ("HDF5:\"" + osFilename +
                         "\":" + oSwathDataFieldMetadata.osLatitudeSubdataset)
                            .c_str(),
                        "GEOLOCATION");
                    poDS->SetMetadataItem("Y_BAND", "1", "GEOLOCATION");
                    poDS->SetMetadataItem(
                        "PIXEL_OFFSET",
                        CPLSPrintf("%d", oSwathDataFieldMetadata.nPixelOffset),
                        "GEOLOCATION");
                    poDS->SetMetadataItem(
                        "PIXEL_STEP",
                        CPLSPrintf("%d", oSwathDataFieldMetadata.nPixelStep),
                        "GEOLOCATION");
                    poDS->SetMetadataItem(
                        "LINE_OFFSET",
                        CPLSPrintf("%d", oSwathDataFieldMetadata.nLineOffset),
                        "GEOLOCATION");
                    poDS->SetMetadataItem(
                        "LINE_STEP",
                        CPLSPrintf("%d", oSwathDataFieldMetadata.nLineStep),
                        "GEOLOCATION");
                    // Not totally sure about that
                    poDS->SetMetadataItem("GEOREFERENCING_CONVENTION",
                                          "PIXEL_CENTER", "GEOLOCATION");
                }
            }
        }
    }

    poDS->nRasterYSize =
        poDS->GetYIndex() < 0
            ? 1
            : static_cast<int>(poDS->dims[poDS->GetYIndex()]);  // nRows
    poDS->nRasterXSize =
        static_cast<int>(poDS->dims[poDS->GetXIndex()]);  // nCols
    int nBands = 1;
    if (poDS->m_nOtherDimIndex >= 0)
    {
        nBands = static_cast<int>(poDS->dims[poDS->m_nOtherDimIndex]);
    }

    CPLStringList aosMetadata;
    std::map<std::string, CPLStringList> oMapBandSpecificMetadata;
    if (poDS->poH5Objects->nType == H5G_DATASET)
    {
        HDF5Dataset::CreateMetadata(poDS->m_hHDF5, poDS->poH5Objects,
                                    H5G_DATASET, false, aosMetadata);
        if (nBands > 1 && poDS->nRasterXSize != nBands &&
            poDS->nRasterYSize != nBands)
        {
            // Heuristics to detect non-scalar attributes, that are intended
            // to be attached to a specific band.
            const CPLStringList aosMetadataDup(aosMetadata);
            for (const auto &[pszKey, pszValue] :
                 cpl::IterateNameValue(aosMetadataDup))
            {
                const hid_t hAttrID = H5Aopen_name(poDS->dataset_id, pszKey);
                const hid_t hAttrSpace = H5Aget_space(hAttrID);
                if (H5Sget_simple_extent_ndims(hAttrSpace) == 1 &&
                    H5Sget_simple_extent_npoints(hAttrSpace) == nBands)
                {
                    CPLStringList aosTokens(
                        CSLTokenizeString2(pszValue, " ", 0));
                    if (aosTokens.size() == nBands)
                    {
                        std::string osAttrName(pszKey);
                        if (osAttrName.size() > strlen("_coefficients") &&
                            osAttrName.substr(osAttrName.size() -
                                              strlen("_coefficients")) ==
                                "_coefficients")
                        {
                            osAttrName.pop_back();
                        }
                        else if (osAttrName.size() > strlen("_wavelengths") &&
                                 osAttrName.substr(osAttrName.size() -
                                                   strlen("_wavelengths")) ==
                                     "_wavelengths")
                        {
                            osAttrName.pop_back();
                        }
                        else if (osAttrName.size() > strlen("_list") &&
                                 osAttrName.substr(osAttrName.size() -
                                                   strlen("_list")) == "_list")
                        {
                            osAttrName.resize(osAttrName.size() -
                                              strlen("_list"));
                        }
                        oMapBandSpecificMetadata[osAttrName] =
                            std::move(aosTokens);
                        aosMetadata.SetNameValue(pszKey, nullptr);
                    }
                }
                H5Sclose(hAttrSpace);
                H5Aclose(hAttrID);
            }
        }
    }

    poDS->m_nBlockXSize = poDS->GetRasterXSize();
    poDS->m_nBlockYSize = 1;
    poDS->m_nBandChunkSize = 1;

    // Check for chunksize and set it as the blocksize (optimizes read).
    const hid_t listid = H5Dget_create_plist(poDS->dataset_id);
    if (listid > 0)
    {
        if (H5Pget_layout(listid) == H5D_CHUNKED)
        {
            hsize_t panChunkDims[3] = {0, 0, 0};
            const int nDimSize = H5Pget_chunk(listid, 3, panChunkDims);
            CPL_IGNORE_RET_VAL(nDimSize);
            CPLAssert(nDimSize == poDS->ndims);
            poDS->m_nBlockXSize =
                static_cast<int>(panChunkDims[poDS->GetXIndex()]);
            if (poDS->GetYIndex() >= 0)
                poDS->m_nBlockYSize =
                    static_cast<int>(panChunkDims[poDS->GetYIndex()]);
            if (nBands > 1)
            {
                poDS->m_nBandChunkSize =
                    static_cast<int>(panChunkDims[poDS->m_nOtherDimIndex]);

                poDS->SetMetadataItem("BAND_CHUNK_SIZE",
                                      CPLSPrintf("%d", poDS->m_nBandChunkSize),
                                      "IMAGE_STRUCTURE");
            }
        }

        const int nFilters = H5Pget_nfilters(listid);
        for (int i = 0; i < nFilters; ++i)
        {
            unsigned int flags = 0;
            size_t cd_nelmts = 0;
            char szName[64 + 1] = {0};
            const auto eFilter = H5Pget_filter(listid, i, &flags, &cd_nelmts,
                                               nullptr, 64, szName);
            if (eFilter == H5Z_FILTER_DEFLATE)
            {
                poDS->SetMetadataItem("COMPRESSION", "DEFLATE",
                                      "IMAGE_STRUCTURE");
            }
            else if (eFilter == H5Z_FILTER_SZIP)
            {
                poDS->SetMetadataItem("COMPRESSION", "SZIP", "IMAGE_STRUCTURE");
            }
        }

        H5Pclose(listid);
    }

    for (int i = 0; i < nBands; i++)
    {
        HDF5ImageRasterBand *const poBand =
            new HDF5ImageRasterBand(poDS, i + 1, eGDALDataType);

        poDS->SetBand(i + 1, poBand);

        if (poDS->poH5Objects->nType == H5G_DATASET)
        {
            poBand->SetMetadata(aosMetadata.List());
            for (const auto &oIter : oMapBandSpecificMetadata)
            {
                poBand->SetMetadataItem(oIter.first.c_str(), oIter.second[i]);
            }
        }
    }

    if (!poDS->GetMetadata("GEOLOCATION"))
        poDS->CreateProjections();

    // Setup/check for pam .aux.xml.
    poDS->TryLoadXML();

    // Setup overviews.
    poDS->oOvManager.Initialize(poDS, ":::VIRTUAL:::");

    return poDS;
}

/************************************************************************/
/*                   HDF5ImageDatasetDriverUnload()                     */
/************************************************************************/

static void HDF5ImageDatasetDriverUnload(GDALDriver *)
{
    HDF5UnloadFileDriver();
}

/************************************************************************/
/*                        GDALRegister_HDF5Image()                      */
/************************************************************************/
void GDALRegister_HDF5Image()

{
    if (!GDAL_CHECK_VERSION("HDF5Image driver"))
        return;

    if (GDALGetDriverByName(HDF5_IMAGE_DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    HDF5ImageDriverSetCommonMetadata(poDriver);

    poDriver->pfnOpen = HDF5ImageDataset::Open;
    poDriver->pfnUnloadDriver = HDF5ImageDatasetDriverUnload;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}

/************************************************************************/
/*                       CreateODIMH5Projection()                       */
/************************************************************************/

// Reference:
//   http://www.knmi.nl/opera/opera3/OPERA_2008_03_WP2.1b_ODIM_H5_v2.1.pdf
//
// 4.3.2 where for geographically referenced image Groups
// We don't use the where_xscale and where_yscale parameters, but recompute them
// from the lower-left and upper-right coordinates. There's some difference.
// As all those parameters are linked together, I'm not sure which one should be
// considered as the reference.

CPLErr HDF5ImageDataset::CreateODIMH5Projection()
{
    const char *const pszProj4String = GetMetadataItem("where_projdef");
    const char *const pszLL_lon = GetMetadataItem("where_LL_lon");
    const char *const pszLL_lat = GetMetadataItem("where_LL_lat");
    const char *const pszUR_lon = GetMetadataItem("where_UR_lon");
    const char *const pszUR_lat = GetMetadataItem("where_UR_lat");
    if (pszProj4String == nullptr || pszLL_lon == nullptr ||
        pszLL_lat == nullptr || pszUR_lon == nullptr || pszUR_lat == nullptr)
        return CE_Failure;

    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    if (m_oSRS.importFromProj4(pszProj4String) != OGRERR_NONE)
        return CE_Failure;

    OGRSpatialReference oSRSWGS84;
    oSRSWGS84.SetWellKnownGeogCS("WGS84");
    oSRSWGS84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    OGRCoordinateTransformation *poCT =
        OGRCreateCoordinateTransformation(&oSRSWGS84, &m_oSRS);
    if (poCT == nullptr)
        return CE_Failure;

    // Reproject corners from long,lat WGS84 to the target SRS.
    double dfLLX = CPLAtof(pszLL_lon);
    double dfLLY = CPLAtof(pszLL_lat);
    double dfURX = CPLAtof(pszUR_lon);
    double dfURY = CPLAtof(pszUR_lat);
    if (!poCT->Transform(1, &dfLLX, &dfLLY) ||
        !poCT->Transform(1, &dfURX, &dfURY))
    {
        delete poCT;
        return CE_Failure;
    }
    delete poCT;

    // Compute the geotransform now.
    const double dfPixelX = (dfURX - dfLLX) / nRasterXSize;
    const double dfPixelY = (dfURY - dfLLY) / nRasterYSize;

    bHasGeoTransform = true;
    adfGeoTransform[0] = dfLLX;
    adfGeoTransform[1] = dfPixelX;
    adfGeoTransform[2] = 0;
    adfGeoTransform[3] = dfURY;
    adfGeoTransform[4] = 0;
    adfGeoTransform[5] = -dfPixelY;

    return CE_None;
}

/************************************************************************/
/*                         CreateProjections()                          */
/************************************************************************/
CPLErr HDF5ImageDataset::CreateProjections()
{
    switch (iSubdatasetType)
    {
        case CSK_PRODUCT:
        {
            int productType = PROD_UNKNOWN;

            if (GetMetadataItem("Product_Type") != nullptr)
            {
                // Get the format's level.
                const char *osMissionLevel =
                    HDF5Dataset::GetMetadataItem("Product_Type");

                if (STARTS_WITH_CI(osMissionLevel, "RAW"))
                    productType = PROD_CSK_L0;

                if (STARTS_WITH_CI(osMissionLevel, "SSC"))
                    productType = PROD_CSK_L1A;

                if (STARTS_WITH_CI(osMissionLevel, "DGM"))
                    productType = PROD_CSK_L1B;

                if (STARTS_WITH_CI(osMissionLevel, "GEC"))
                    productType = PROD_CSK_L1C;

                if (STARTS_WITH_CI(osMissionLevel, "GTC"))
                    productType = PROD_CSK_L1D;
            }

            CaptureCSKGeoTransform(productType);
            CaptureCSKGeolocation(productType);
            CaptureCSKGCPs(productType);

            break;
        }
        case UNKNOWN_PRODUCT:
        {
            constexpr int NBGCPLAT = 100;
            constexpr int NBGCPLON = 30;

            const int nDeltaLat = nRasterYSize / NBGCPLAT;
            const int nDeltaLon = nRasterXSize / NBGCPLON;

            if (nDeltaLat == 0 || nDeltaLon == 0)
                return CE_None;

            // Create HDF5 Data Hierarchy in a link list.
            poH5Objects = HDF5FindDatasetObjects(poH5RootGroup, "Latitude");
            if (!poH5Objects)
            {
                if (GetMetadataItem("where_projdef") != nullptr)
                    return CreateODIMH5Projection();
                return CE_None;
            }

            // The Latitude and Longitude arrays must have a rank of 2 to
            // retrieve GCPs.
            if (poH5Objects->nRank != 2 ||
                poH5Objects->paDims[0] != static_cast<size_t>(nRasterYSize) ||
                poH5Objects->paDims[1] != static_cast<size_t>(nRasterXSize))
            {
                return CE_None;
            }

            // Retrieve HDF5 data information.
            const hid_t LatitudeDatasetID =
                H5Dopen(m_hHDF5, poH5Objects->pszPath);
            // LatitudeDataspaceID = H5Dget_space(dataset_id);

            poH5Objects = HDF5FindDatasetObjects(poH5RootGroup, "Longitude");
            // GCPs.
            if (poH5Objects == nullptr || poH5Objects->nRank != 2 ||
                poH5Objects->paDims[0] != static_cast<size_t>(nRasterYSize) ||
                poH5Objects->paDims[1] != static_cast<size_t>(nRasterXSize))
            {
                if (LatitudeDatasetID > 0)
                    H5Dclose(LatitudeDatasetID);
                return CE_None;
            }

            const hid_t LongitudeDatasetID =
                H5Dopen(m_hHDF5, poH5Objects->pszPath);
            // LongitudeDataspaceID = H5Dget_space(dataset_id);

            if (LatitudeDatasetID > 0 && LongitudeDatasetID > 0)
            {
                float *const Latitude =
                    static_cast<float *>(VSI_MALLOC3_VERBOSE(
                        nRasterYSize, nRasterXSize, sizeof(float)));
                float *const Longitude =
                    static_cast<float *>(VSI_MALLOC3_VERBOSE(
                        nRasterYSize, nRasterXSize, sizeof(float)));
                if (!Latitude || !Longitude)
                {
                    CPLFree(Latitude);
                    CPLFree(Longitude);
                    H5Dclose(LatitudeDatasetID);
                    H5Dclose(LongitudeDatasetID);
                    return CE_Failure;
                }
                memset(Latitude, 0,
                       static_cast<size_t>(nRasterXSize) * nRasterYSize *
                           sizeof(float));
                memset(Longitude, 0,
                       static_cast<size_t>(nRasterXSize) * nRasterYSize *
                           sizeof(float));

                // netCDF convention for nodata
                double dfLatNoData = 0;
                bool bHasLatNoData = GH5_FetchAttribute(
                    LatitudeDatasetID, "_FillValue", dfLatNoData);

                double dfLongNoData = 0;
                bool bHasLongNoData = GH5_FetchAttribute(
                    LongitudeDatasetID, "_FillValue", dfLongNoData);

                H5Dread(LatitudeDatasetID, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL,
                        H5P_DEFAULT, Latitude);

                H5Dread(LongitudeDatasetID, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL,
                        H5P_DEFAULT, Longitude);

                m_oSRS.Clear();
                m_oGCPSRS.SetWellKnownGeogCS("WGS84");

                const int nYLimit =
                    (static_cast<int>(nRasterYSize) / nDeltaLat) * nDeltaLat;
                const int nXLimit =
                    (static_cast<int>(nRasterXSize) / nDeltaLon) * nDeltaLon;

                // The original code in
                // https://trac.osgeo.org/gdal/changeset/8066 always add +180 to
                // the longitudes, but without justification I suspect this
                // might be due to handling products crossing the antimeridian.
                // Trying to do it just when needed through a heuristics.
                bool bHasLonNearMinus180 = false;
                bool bHasLonNearPlus180 = false;
                bool bHasLonNearZero = false;
                for (int j = 0; j < nYLimit; j += nDeltaLat)
                {
                    for (int i = 0; i < nXLimit; i += nDeltaLon)
                    {
                        const int iGCP = j * nRasterXSize + i;
                        if ((bHasLatNoData && static_cast<float>(dfLatNoData) ==
                                                  Latitude[iGCP]) ||
                            (bHasLongNoData &&
                             static_cast<float>(dfLongNoData) ==
                                 Longitude[iGCP]))
                            continue;
                        if (Longitude[iGCP] > 170 && Longitude[iGCP] <= 180)
                            bHasLonNearPlus180 = true;
                        if (Longitude[iGCP] < -170 && Longitude[iGCP] >= -180)
                            bHasLonNearMinus180 = true;
                        if (fabs(Longitude[iGCP]) < 90)
                            bHasLonNearZero = true;
                    }
                }

                // Fill the GCPs list.
                const char *pszShiftGCP =
                    CPLGetConfigOption("HDF5_SHIFT_GCPX_BY_180", nullptr);
                const bool bAdd180 =
                    (bHasLonNearPlus180 && bHasLonNearMinus180 &&
                     !bHasLonNearZero && pszShiftGCP == nullptr) ||
                    (pszShiftGCP != nullptr && CPLTestBool(pszShiftGCP));

                for (int j = 0; j < nYLimit; j += nDeltaLat)
                {
                    for (int i = 0; i < nXLimit; i += nDeltaLon)
                    {
                        const int iGCP = j * nRasterXSize + i;
                        if ((bHasLatNoData && static_cast<float>(dfLatNoData) ==
                                                  Latitude[iGCP]) ||
                            (bHasLongNoData &&
                             static_cast<float>(dfLongNoData) ==
                                 Longitude[iGCP]))
                            continue;
                        double dfGCPX = static_cast<double>(Longitude[iGCP]);
                        if (bAdd180)
                            dfGCPX += 180.0;
                        const double dfGCPY =
                            static_cast<double>(Latitude[iGCP]);

                        m_aoGCPs.emplace_back("", "", i + 0.5, j + 0.5, dfGCPX,
                                              dfGCPY);
                    }
                }

                CPLFree(Latitude);
                CPLFree(Longitude);
            }

            if (LatitudeDatasetID > 0)
                H5Dclose(LatitudeDatasetID);
            if (LongitudeDatasetID > 0)
                H5Dclose(LongitudeDatasetID);

            break;
        }
    }

    return CE_None;
}

/************************************************************************/
/*                         GetMetadataItem()                            */
/************************************************************************/

const char *HDF5ImageDataset::GetMetadataItem(const char *pszName,
                                              const char *pszDomain)
{
    if (pszDomain && EQUAL(pszDomain, "__DEBUG__") &&
        EQUAL(pszName, "WholeBandChunkOptim"))
    {
        switch (m_eWholeBandChunkOptim)
        {
            case WBC_DETECTION_IN_PROGRESS:
                return "DETECTION_IN_PROGRESS";
            case WBC_DISABLED:
                return "DISABLED";
            case WBC_ENABLED:
                return "ENABLED";
        }
    }
    return GDALPamDataset::GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                         GetSpatialRef()                              */
/************************************************************************/

const OGRSpatialReference *HDF5ImageDataset::GetSpatialRef() const
{
    if (!m_oSRS.IsEmpty())
        return &m_oSRS;
    return GDALPamDataset::GetSpatialRef();
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int HDF5ImageDataset::GetGCPCount()

{
    if (!m_aoGCPs.empty())
        return static_cast<int>(m_aoGCPs.size());

    return GDALPamDataset::GetGCPCount();
}

/************************************************************************/
/*                        GetGCPSpatialRef()                            */
/************************************************************************/

const OGRSpatialReference *HDF5ImageDataset::GetGCPSpatialRef() const

{
    if (!m_aoGCPs.empty() && !m_oGCPSRS.IsEmpty())
        return &m_oGCPSRS;

    return GDALPamDataset::GetGCPSpatialRef();
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *HDF5ImageDataset::GetGCPs()
{
    if (!m_aoGCPs.empty())
        return gdal::GCP::c_ptr(m_aoGCPs);

    return GDALPamDataset::GetGCPs();
}

/************************************************************************/
/*                         GetGeoTransform()                            */
/************************************************************************/

CPLErr HDF5ImageDataset::GetGeoTransform(double *padfTransform)
{
    if (bHasGeoTransform)
    {
        memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6);
        return CE_None;
    }

    return GDALPamDataset::GetGeoTransform(padfTransform);
}

/************************************************************************/
/*                       IdentifyProductType()                          */
/************************************************************************/

/**
 * Identify if the subdataset has a known product format
 * It stores a product identifier in iSubdatasetType,
 * UNKNOWN_PRODUCT, if it isn't a recognizable format.
 */
void HDF5ImageDataset::IdentifyProductType()
{
    iSubdatasetType = UNKNOWN_PRODUCT;

    // COSMO-SKYMED

    // Get the Mission Id as a char *, because the field may not exist.
    const char *const pszMissionId = HDF5Dataset::GetMetadataItem("Mission_ID");

    // If there is a Mission_ID field.
    if (pszMissionId != nullptr && strstr(GetDescription(), "QLK") == nullptr)
    {
        // Check if the mission type is CSK, KMPS or CSG.
        // KMPS: Komsat-5 is Korean mission with a SAR instrument.
        // CSG: Cosmo Skymed 2nd Generation
        if (EQUAL(pszMissionId, "CSK") || EQUAL(pszMissionId, "KMPS") ||
            EQUAL(pszMissionId, "CSG"))
        {
            iSubdatasetType = CSK_PRODUCT;

            if (GetMetadataItem("Product_Type") != nullptr)
            {
                // Get the format's level.
                const char *osMissionLevel =
                    HDF5Dataset::GetMetadataItem("Product_Type");

                if (STARTS_WITH_CI(osMissionLevel, "RAW"))
                    iCSKProductType = PROD_CSK_L0;

                if (STARTS_WITH_CI(osMissionLevel, "SCS"))
                    iCSKProductType = PROD_CSK_L1A;

                if (STARTS_WITH_CI(osMissionLevel, "DGM"))
                    iCSKProductType = PROD_CSK_L1B;

                if (STARTS_WITH_CI(osMissionLevel, "GEC"))
                    iCSKProductType = PROD_CSK_L1C;

                if (STARTS_WITH_CI(osMissionLevel, "GTC"))
                    iCSKProductType = PROD_CSK_L1D;
            }
        }
    }
}

/************************************************************************/
/*                       CaptureCSKGeolocation()                        */
/************************************************************************/

/**
 * Captures Geolocation information from a COSMO-SKYMED
 * file.
 * The geoid will always be WGS84
 * The projection type may be UTM or UPS, depending on the
 * latitude from the center of the image.
 * @param iProductType type of CSK subproduct, see HDF5CSKProduct
 */
void HDF5ImageDataset::CaptureCSKGeolocation(int iProductType)
{
    // Set the ellipsoid to WGS84.
    m_oSRS.SetWellKnownGeogCS("WGS84");

    if (iProductType == PROD_CSK_L1C || iProductType == PROD_CSK_L1D)
    {
        double *dfProjFalseEastNorth = nullptr;
        double *dfProjScaleFactor = nullptr;
        double *dfCenterCoord = nullptr;

        // Check if all the metadata attributes are present.
        if (HDF5ReadDoubleAttr("Map Projection False East-North",
                               &dfProjFalseEastNorth) == CE_Failure ||
            HDF5ReadDoubleAttr("Map Projection Scale Factor",
                               &dfProjScaleFactor) == CE_Failure ||
            HDF5ReadDoubleAttr("Map Projection Centre", &dfCenterCoord) ==
                CE_Failure ||
            GetMetadataItem("Projection_ID") == nullptr)
        {
            m_oSRS.Clear();
            m_oGCPSRS.Clear();
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "The CSK hdf5 file geolocation information is "
                     "malformed");
        }
        else
        {
            // Fetch projection Type.
            CPLString osProjectionID = GetMetadataItem("Projection_ID");

            // If the projection is UTM.
            if (EQUAL(osProjectionID, "UTM"))
            {
                // @TODO: use SetUTM
                m_oSRS.SetProjCS(SRS_PT_TRANSVERSE_MERCATOR);
                m_oSRS.SetTM(dfCenterCoord[0], dfCenterCoord[1],
                             dfProjScaleFactor[0], dfProjFalseEastNorth[0],
                             dfProjFalseEastNorth[1]);
            }
            else
            {
                // TODO: No UPS projected files to test!
                // If the projection is UPS.
                if (EQUAL(osProjectionID, "UPS"))
                {
                    m_oSRS.SetProjCS(SRS_PT_POLAR_STEREOGRAPHIC);
                    m_oSRS.SetPS(dfCenterCoord[0], dfCenterCoord[1],
                                 dfProjScaleFactor[0], dfProjFalseEastNorth[0],
                                 dfProjFalseEastNorth[1]);
                }
            }

            CPLFree(dfCenterCoord);
            CPLFree(dfProjScaleFactor);
            CPLFree(dfProjFalseEastNorth);
        }
    }
    else
    {
        m_oGCPSRS = m_oSRS;
    }
}

/************************************************************************/
/*                       CaptureCSKGeoTransform()                       */
/************************************************************************/

/**
 * Get Geotransform information for COSMO-SKYMED files
 * In case of success it stores the transformation
 * in adfGeoTransform. In case of failure it doesn't
 * modify adfGeoTransform
 * @param iProductType type of CSK subproduct, see HDF5CSKProduct
 */
void HDF5ImageDataset::CaptureCSKGeoTransform(int iProductType)
{
    const char *pszSubdatasetName = GetSubdatasetName();

    bHasGeoTransform = false;
    // If the product level is not L1C or L1D then
    // it doesn't have a valid projection.
    if (iProductType == PROD_CSK_L1C || iProductType == PROD_CSK_L1D)
    {
        // If there is a subdataset.
        if (pszSubdatasetName != nullptr)
        {
            CPLString osULPath = pszSubdatasetName;
            osULPath += "/Top Left East-North";

            CPLString osLineSpacingPath = pszSubdatasetName;
            osLineSpacingPath += "/Line Spacing";

            CPLString osColumnSpacingPath = pszSubdatasetName;
            osColumnSpacingPath += "/Column Spacing";

            double *pdOutUL = nullptr;
            double *pdLineSpacing = nullptr;
            double *pdColumnSpacing = nullptr;

            // If it could find the attributes on the metadata.
            if (HDF5ReadDoubleAttr(osULPath.c_str(), &pdOutUL) == CE_Failure ||
                HDF5ReadDoubleAttr(osLineSpacingPath.c_str(), &pdLineSpacing) ==
                    CE_Failure ||
                HDF5ReadDoubleAttr(osColumnSpacingPath.c_str(),
                                   &pdColumnSpacing) == CE_Failure)
            {
                bHasGeoTransform = false;
            }
            else
            {
                // geotransform[1] : width of pixel
                // geotransform[4] : rotational coefficient, zero for north up
                // images.
                // geotransform[2] : rotational coefficient, zero for north up
                // images.
                // geotransform[5] : height of pixel (but negative)

                adfGeoTransform[0] = pdOutUL[0];
                adfGeoTransform[1] = pdLineSpacing[0];
                adfGeoTransform[2] = 0;
                adfGeoTransform[3] = pdOutUL[1];
                adfGeoTransform[4] = 0;
                adfGeoTransform[5] = -pdColumnSpacing[0];

                CPLFree(pdOutUL);
                CPLFree(pdLineSpacing);
                CPLFree(pdColumnSpacing);

                bHasGeoTransform = true;
            }
        }
    }
}

/************************************************************************/
/*                          CaptureCSKGCPs()                            */
/************************************************************************/

/**
 * Retrieves and stores the GCPs from a COSMO-SKYMED dataset
 * It only retrieves the GCPs for L0, L1A and L1B products
 * for L1C and L1D products, geotransform is provided.
 * The GCPs provided will be the Image's corners.
 * @param iProductType type of CSK product @see HDF5CSKProductEnum
 */
void HDF5ImageDataset::CaptureCSKGCPs(int iProductType)
{
    // Only retrieve GCPs for L0,L1A and L1B products.
    if (iProductType == PROD_CSK_L0 || iProductType == PROD_CSK_L1A ||
        iProductType == PROD_CSK_L1B)
    {
        CPLString osCornerName[4];
        double pdCornerPixel[4] = {0.0, 0.0, 0.0, 0.0};
        double pdCornerLine[4] = {0.0, 0.0, 0.0, 0.0};

        const char *const pszSubdatasetName = GetSubdatasetName();

        // Load the subdataset name first.
        for (int i = 0; i < 4; i++)
            osCornerName[i] = pszSubdatasetName;

        // Load the attribute name, and raster coordinates for
        // all the corners.
        osCornerName[0] += "/Top Left Geodetic Coordinates";
        pdCornerPixel[0] = 0;
        pdCornerLine[0] = 0;

        osCornerName[1] += "/Top Right Geodetic Coordinates";
        pdCornerPixel[1] = GetRasterXSize();
        pdCornerLine[1] = 0;

        osCornerName[2] += "/Bottom Left Geodetic Coordinates";
        pdCornerPixel[2] = 0;
        pdCornerLine[2] = GetRasterYSize();

        osCornerName[3] += "/Bottom Right Geodetic Coordinates";
        pdCornerPixel[3] = GetRasterXSize();
        pdCornerLine[3] = GetRasterYSize();

        // For all the image's corners.
        for (int i = 0; i < 4; i++)
        {
            double *pdCornerCoordinates = nullptr;

            // Retrieve the attributes.
            if (HDF5ReadDoubleAttr(osCornerName[i].c_str(),
                                   &pdCornerCoordinates) == CE_Failure)
            {
                CPLError(CE_Failure, CPLE_OpenFailed,
                         "Error retrieving CSK GCPs");
                m_aoGCPs.clear();
                break;
            }

            m_aoGCPs.emplace_back(osCornerName[i].c_str(), "", pdCornerPixel[i],
                                  pdCornerLine[i],
                                  /* X = */ pdCornerCoordinates[1],
                                  /* Y = */ pdCornerCoordinates[0],
                                  /* Z = */ pdCornerCoordinates[2]);

            // Free the returned coordinates.
            CPLFree(pdCornerCoordinates);
        }
    }
}

/******************************************************************************
 *
 * Project:  GeoTIFF Driver
 * Purpose:  GDAL GeoTIFF support.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, 2002, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2015, Even Rouault <even dot rouault at spatialys dot com>
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

#include "gtiffjpegoverviewds.h"

#include "gtiffdataset.h"

#include "tifvsi.h"

/************************************************************************/
/* ==================================================================== */
/*                     GTiffJPEGOverviewBand                            */
/* ==================================================================== */
/************************************************************************/

class GTiffJPEGOverviewBand final : public GDALRasterBand
{
  public:
    GTiffJPEGOverviewBand(GTiffJPEGOverviewDS *poDS, int nBand);

    virtual ~GTiffJPEGOverviewBand()
    {
    }

    virtual CPLErr IReadBlock(int, int, void *) override;

    GDALColorInterp GetColorInterpretation() override
    {
        return cpl::down_cast<GTiffJPEGOverviewDS *>(poDS)
            ->m_poParentDS->GetRasterBand(nBand)
            ->GetColorInterpretation();
    }
};

/************************************************************************/
/*                        GTiffJPEGOverviewDS()                         */
/************************************************************************/

GTiffJPEGOverviewDS::GTiffJPEGOverviewDS(GTiffDataset *poParentDSIn,
                                         int nOverviewLevelIn,
                                         const void *pJPEGTable,
                                         int nJPEGTableSizeIn)
    : m_poParentDS(poParentDSIn), m_nOverviewLevel(nOverviewLevelIn),
      m_nJPEGTableSize(nJPEGTableSizeIn)
{
    ShareLockWithParentDataset(poParentDSIn);

    m_osTmpFilenameJPEGTable.Printf("/vsimem/jpegtable_%p", this);

    const GByte abyAdobeAPP14RGB[] = {0xFF, 0xEE, 0x00, 0x0E, 0x41, 0x64,
                                      0x6F, 0x62, 0x65, 0x00, 0x64, 0x00,
                                      0x00, 0x00, 0x00, 0x00};
    const bool bAddAdobe =
        m_poParentDS->m_nPlanarConfig == PLANARCONFIG_CONTIG &&
        m_poParentDS->m_nPhotometric != PHOTOMETRIC_YCBCR &&
        m_poParentDS->nBands == 3;
    m_pabyJPEGTable = static_cast<GByte *>(CPLMalloc(
        m_nJPEGTableSize + (bAddAdobe ? sizeof(abyAdobeAPP14RGB) : 0)));
    memcpy(m_pabyJPEGTable, pJPEGTable, m_nJPEGTableSize);
    if (bAddAdobe)
    {
        memcpy(m_pabyJPEGTable + m_nJPEGTableSize, abyAdobeAPP14RGB,
               sizeof(abyAdobeAPP14RGB));
        m_nJPEGTableSize += sizeof(abyAdobeAPP14RGB);
    }
    CPL_IGNORE_RET_VAL(VSIFCloseL(VSIFileFromMemBuffer(
        m_osTmpFilenameJPEGTable, m_pabyJPEGTable, m_nJPEGTableSize, TRUE)));

    const int nScaleFactor = 1 << m_nOverviewLevel;
    nRasterXSize =
        (m_poParentDS->nRasterXSize + nScaleFactor - 1) / nScaleFactor;
    nRasterYSize =
        (m_poParentDS->nRasterYSize + nScaleFactor - 1) / nScaleFactor;

    for (int i = 1; i <= m_poParentDS->nBands; ++i)
        SetBand(i, new GTiffJPEGOverviewBand(this, i));

    SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
    if (m_poParentDS->m_nPhotometric == PHOTOMETRIC_YCBCR)
        SetMetadataItem("COMPRESSION", "YCbCr JPEG", "IMAGE_STRUCTURE");
    else
        SetMetadataItem("COMPRESSION", "JPEG", "IMAGE_STRUCTURE");
}

/************************************************************************/
/*                       ~GTiffJPEGOverviewDS()                         */
/************************************************************************/

GTiffJPEGOverviewDS::~GTiffJPEGOverviewDS()
{
    m_poJPEGDS.reset();
    VSIUnlink(m_osTmpFilenameJPEGTable);
    if (!m_osTmpFilename.empty())
        VSIUnlink(m_osTmpFilename);
}

/************************************************************************/
/*                            IRasterIO()                               */
/************************************************************************/

CPLErr GTiffJPEGOverviewDS::IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                                      int nXSize, int nYSize, void *pData,
                                      int nBufXSize, int nBufYSize,
                                      GDALDataType eBufType, int nBandCount,
                                      int *panBandMap, GSpacing nPixelSpace,
                                      GSpacing nLineSpace, GSpacing nBandSpace,
                                      GDALRasterIOExtraArg *psExtraArg)

{
    // For non-single strip JPEG-IN-TIFF, the block based strategy will
    // be the most efficient one, to avoid decompressing the JPEG content
    // for each requested band.
    if (nBandCount > 1 &&
        m_poParentDS->m_nPlanarConfig == PLANARCONFIG_CONTIG &&
        (m_poParentDS->m_nBlockXSize < m_poParentDS->nRasterXSize ||
         m_poParentDS->m_nBlockYSize > 1))
    {
        return BlockBasedRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize, pData,
                                  nBufXSize, nBufYSize, eBufType, nBandCount,
                                  panBandMap, nPixelSpace, nLineSpace,
                                  nBandSpace, psExtraArg);
    }

    return GDALDataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize, pData,
                                  nBufXSize, nBufYSize, eBufType, nBandCount,
                                  panBandMap, nPixelSpace, nLineSpace,
                                  nBandSpace, psExtraArg);
}

/************************************************************************/
/*                        GTiffJPEGOverviewBand()                       */
/************************************************************************/

GTiffJPEGOverviewBand::GTiffJPEGOverviewBand(GTiffJPEGOverviewDS *poDSIn,
                                             int nBandIn)
{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType =
        poDSIn->m_poParentDS->GetRasterBand(nBandIn)->GetRasterDataType();
    poDSIn->m_poParentDS->GetRasterBand(nBandIn)->GetBlockSize(&nBlockXSize,
                                                               &nBlockYSize);
    const int nScaleFactor = 1 << poDSIn->m_nOverviewLevel;
    nBlockXSize = (nBlockXSize + nScaleFactor - 1) / nScaleFactor;
    nBlockYSize = (nBlockYSize + nScaleFactor - 1) / nScaleFactor;
}

/************************************************************************/
/*                          IReadBlock()                                */
/************************************************************************/

CPLErr GTiffJPEGOverviewBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                         void *pImage)
{
    GTiffJPEGOverviewDS *m_poGDS = cpl::down_cast<GTiffJPEGOverviewDS *>(poDS);

    // Compute the source block ID.
    int nBlockId = 0;
    int nParentBlockXSize, nParentBlockYSize;
    m_poGDS->m_poParentDS->GetRasterBand(1)->GetBlockSize(&nParentBlockXSize,
                                                          &nParentBlockYSize);
    const bool bIsSingleStripAsSplit =
        (nParentBlockYSize == 1 &&
         m_poGDS->m_poParentDS->m_nBlockYSize != nParentBlockYSize);
    if (!bIsSingleStripAsSplit)
    {
        nBlockId =
            nBlockYOff * m_poGDS->m_poParentDS->m_nBlocksPerRow + nBlockXOff;
    }
    if (m_poGDS->m_poParentDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE)
    {
        nBlockId += (nBand - 1) * m_poGDS->m_poParentDS->m_nBlocksPerBand;
    }

    // Make sure it is available.
    const int nDataTypeSize = GDALGetDataTypeSizeBytes(eDataType);
    vsi_l_offset nOffset = 0;
    vsi_l_offset nByteCount = 0;
    bool bErrOccurred = false;
    if (!m_poGDS->m_poParentDS->IsBlockAvailable(nBlockId, &nOffset,
                                                 &nByteCount, &bErrOccurred))
    {
        memset(pImage, 0,
               static_cast<GPtrDiff_t>(nBlockXSize) * nBlockYSize *
                   nDataTypeSize);
        if (bErrOccurred)
            return CE_Failure;
        return CE_None;
    }

    const int nScaleFactor = 1 << m_poGDS->m_nOverviewLevel;
    if (m_poGDS->m_poJPEGDS == nullptr || nBlockId != m_poGDS->m_nBlockId)
    {
        if (nByteCount < 2)
            return CE_Failure;
        nOffset += 2;  // Skip leading 0xFF 0xF8.
        nByteCount -= 2;

        CPLString osFileToOpen;
        m_poGDS->m_osTmpFilename.Printf("/vsimem/sparse_%p", m_poGDS);
        VSILFILE *fp = VSIFOpenL(m_poGDS->m_osTmpFilename, "wb+");

        // If the size of the JPEG strip/tile is small enough, we will
        // read it from the TIFF file and forge a in-memory JPEG file with
        // the JPEG table followed by the JPEG data.
        const bool bInMemoryJPEGFile = nByteCount < 256 * 256;
        if (bInMemoryJPEGFile)
        {
            osFileToOpen = m_poGDS->m_osTmpFilename;

            bool bError = false;
            if (VSIFSeekL(fp, m_poGDS->m_nJPEGTableSize + nByteCount - 1,
                          SEEK_SET) != 0)
                bError = true;
            char ch = 0;
            if (!bError && VSIFWriteL(&ch, 1, 1, fp) != 1)
                bError = true;
            GByte *pabyBuffer =
                VSIGetMemFileBuffer(m_poGDS->m_osTmpFilename, nullptr, FALSE);
            memcpy(pabyBuffer, m_poGDS->m_pabyJPEGTable,
                   m_poGDS->m_nJPEGTableSize);
            TIFF *hTIFF = m_poGDS->m_poParentDS->m_hTIFF;
            VSILFILE *fpTIF = VSI_TIFFGetVSILFile(TIFFClientdata(hTIFF));
            if (!bError && VSIFSeekL(fpTIF, nOffset, SEEK_SET) != 0)
                bError = true;
            if (VSIFReadL(pabyBuffer + m_poGDS->m_nJPEGTableSize,
                          static_cast<size_t>(nByteCount), 1, fpTIF) != 1)
                bError = true;
            if (bError)
            {
                CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
                return CE_Failure;
            }
        }
        else
        {
            // If the JPEG strip/tile is too big (e.g. a single-strip
            // JPEG-in-TIFF), we will use /vsisparse mechanism to make a
            // fake JPEG file.

            osFileToOpen =
                CPLSPrintf("/vsisparse/%s", m_poGDS->m_osTmpFilename.c_str());

            if (VSIFPrintfL(fp,
                            "<VSISparseFile><SubfileRegion>"
                            "<Filename relative='0'>%s</Filename>"
                            "<DestinationOffset>0</DestinationOffset>"
                            "<SourceOffset>0</SourceOffset>"
                            "<RegionLength>%d</RegionLength>"
                            "</SubfileRegion>"
                            "<SubfileRegion>"
                            "<Filename relative='0'>%s</Filename>"
                            "<DestinationOffset>%d</DestinationOffset>"
                            "<SourceOffset>" CPL_FRMT_GUIB "</SourceOffset>"
                            "<RegionLength>" CPL_FRMT_GUIB "</RegionLength>"
                            "</SubfileRegion></VSISparseFile>",
                            m_poGDS->m_osTmpFilenameJPEGTable.c_str(),
                            static_cast<int>(m_poGDS->m_nJPEGTableSize),
                            m_poGDS->m_poParentDS->GetDescription(),
                            static_cast<int>(m_poGDS->m_nJPEGTableSize),
                            nOffset, nByteCount) < 0)
            {
                CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
                return CE_Failure;
            }
        }
        CPL_IGNORE_RET_VAL(VSIFCloseL(fp));

        const char *const apszDrivers[] = {"JPEG", nullptr};

        CPLConfigOptionSetter oJPEGtoRGBSetter(
            "GDAL_JPEG_TO_RGB",
            m_poGDS->m_poParentDS->m_nPlanarConfig == PLANARCONFIG_CONTIG &&
                    m_poGDS->nBands == 4
                ? "NO"
                : "YES",
            false);

        m_poGDS->m_poJPEGDS.reset(
            GDALDataset::Open(osFileToOpen, GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                              apszDrivers, nullptr, nullptr));

        if (m_poGDS->m_poJPEGDS != nullptr)
        {
            // Force all implicit overviews to be available, even for
            // small tiles.
            CPLConfigOptionSetter oInternalOverviewsSetter(
                "JPEG_FORCE_INTERNAL_OVERVIEWS", "YES", false);
            GDALGetOverviewCount(
                GDALGetRasterBand(m_poGDS->m_poJPEGDS.get(), 1));

            m_poGDS->m_nBlockId = nBlockId;
        }
    }

    CPLErr eErr = CE_Failure;
    if (m_poGDS->m_poJPEGDS)
    {
        GDALDataset *l_poDS = m_poGDS->m_poJPEGDS.get();

        int nReqXOff = 0;
        int nReqYOff = 0;
        int nReqXSize = 0;
        int nReqYSize = 0;
        if (bIsSingleStripAsSplit)
        {
            nReqYOff = nBlockYOff * nScaleFactor;
            nReqXSize = l_poDS->GetRasterXSize();
            nReqYSize = nScaleFactor;
        }
        else
        {
            if (nBlockXSize == m_poGDS->GetRasterXSize())
            {
                nReqXSize = l_poDS->GetRasterXSize();
            }
            else
            {
                nReqXSize = nBlockXSize * nScaleFactor;
            }
            nReqYSize = nBlockYSize * nScaleFactor;
        }
        int nBufXSize = nBlockXSize;
        int nBufYSize = nBlockYSize;
        if (nBlockXOff == m_poGDS->m_poParentDS->m_nBlocksPerRow - 1)
        {
            nReqXSize = m_poGDS->m_poParentDS->nRasterXSize -
                        nBlockXOff * m_poGDS->m_poParentDS->m_nBlockXSize;
        }
        if (nReqXOff + nReqXSize > l_poDS->GetRasterXSize())
        {
            nReqXSize = l_poDS->GetRasterXSize() - nReqXOff;
        }
        if (!bIsSingleStripAsSplit &&
            nBlockYOff == m_poGDS->m_poParentDS->m_nBlocksPerColumn - 1)
        {
            nReqYSize = m_poGDS->m_poParentDS->nRasterYSize -
                        nBlockYOff * m_poGDS->m_poParentDS->m_nBlockYSize;
        }
        if (nReqYOff + nReqYSize > l_poDS->GetRasterYSize())
        {
            nReqYSize = l_poDS->GetRasterYSize() - nReqYOff;
        }
        if (nBlockXOff * nBlockXSize > m_poGDS->GetRasterXSize() - nBufXSize)
        {
            memset(pImage, 0,
                   static_cast<GPtrDiff_t>(nBlockXSize) * nBlockYSize *
                       nDataTypeSize);
            nBufXSize = m_poGDS->GetRasterXSize() - nBlockXOff * nBlockXSize;
        }
        if (nBlockYOff * nBlockYSize > m_poGDS->GetRasterYSize() - nBufYSize)
        {
            memset(pImage, 0,
                   static_cast<GPtrDiff_t>(nBlockXSize) * nBlockYSize *
                       nDataTypeSize);
            nBufYSize = m_poGDS->GetRasterYSize() - nBlockYOff * nBlockYSize;
        }

        const int nSrcBand =
            m_poGDS->m_poParentDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE
                ? 1
                : nBand;
        if (nSrcBand <= l_poDS->GetRasterCount())
        {
            eErr = l_poDS->GetRasterBand(nSrcBand)->RasterIO(
                GF_Read, nReqXOff, nReqYOff, nReqXSize, nReqYSize, pImage,
                nBufXSize, nBufYSize, eDataType, 0,
                static_cast<GPtrDiff_t>(nBlockXSize) * nDataTypeSize, nullptr);
        }
    }

    return eErr;
}

/* TerraSAR-X COSAR Format Driver
 * (C)2007 Philippe P. Vachon <philippe@cowpig.ca>
 * ---------------------------------------------------------------------------
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
 */

#include "cpl_conv.h"
#include "cpl_port.h"
#include "cpl_float.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal_frmts.h"
#include "gdal_priv.h"

#include <algorithm>
#include <string.h>

/* Various offsets, in bytes */
// Commented out the unused defines.
// #define BIB_OFFSET   0  /* Bytes in burst, valid only for ScanSAR */
// #define RSRI_OFFSET  4  /* Range Sample Relative Index */
constexpr int RS_OFFSET = 8; /* Range Samples, the length of a range line */
// #define AS_OFFSET    12 /* Azimuth Samples, the length of an azimuth column
// */ #define BI_OFFSET    16 /* Burst Index, the index number of the burst */
constexpr int RTNB_OFFSET =
    20; /* Rangeline total number of bytes, incl. annot. */
// #define TNL_OFFSET   24 /* Total Number of Lines */
constexpr int MAGIC1_OFFSET = 28;         /* Magic number 1: 0x43534152 */
constexpr int VERSION_NUMBER_OFFSET = 32; /* 1 for COSAR, 2 for COSSC */

// #define FILLER_MAGIC 0x7F7F7F7F  /* Filler value, we'll use this for a test
// */

class COSARDataset final : public GDALDataset
{
    friend class COSARRasterBand;
    VSILFILE *m_fp = nullptr;
    uint32_t m_nVersion = 0;

  public:
    COSARDataset() = default;
    ~COSARDataset();

    static GDALDataset *Open(GDALOpenInfo *);
};

class COSARRasterBand final : public GDALRasterBand
{
    uint32_t nRTNB;

  public:
    COSARRasterBand(COSARDataset *, uint32_t nRTNB);
    CPLErr IReadBlock(int, int, void *) override;
};

/*****************************************************************************
 * COSARRasterBand Implementation
 *****************************************************************************/

COSARRasterBand::COSARRasterBand(COSARDataset *pDS, uint32_t nRTNBIn)
    : nRTNB(nRTNBIn)
{
    COSARDataset *pCDS = cpl::down_cast<COSARDataset *>(pDS);
    nBlockXSize = pDS->GetRasterXSize();
    nBlockYSize = 1;
    eDataType = pCDS->m_nVersion == 1 ? GDT_CInt16 : GDT_CFloat32;
}

CPLErr COSARRasterBand::IReadBlock(int /*nBlockXOff*/, int nBlockYOff,
                                   void *pImage)
{

    COSARDataset *pCDS = cpl::down_cast<COSARDataset *>(poDS);
    constexpr uint32_t ITEM_SIZE = 2 * sizeof(int16_t);

    /* Find the line we want to be at */
    /* To explain some magic numbers:
     *   4 bytes for an entire sample (2 I, 2 Q)
     *   nBlockYOff + 4 = Y offset + 4 annotation lines at beginning
     *    of file
     */

    VSIFSeekL(pCDS->m_fp,
              static_cast<vsi_l_offset>(nRTNB) * (nBlockYOff + ITEM_SIZE),
              SEEK_SET);

    /* Read RSFV and RSLV (TX-GS-DD-3307) */
    uint32_t nRSFV = 0;  // Range Sample First Valid (starting at 1)
    uint32_t nRSLV = 0;  // Range Sample Last Valid (starting at 1)
    VSIFReadL(&nRSFV, 1, sizeof(nRSFV), pCDS->m_fp);
    VSIFReadL(&nRSLV, 1, sizeof(nRSLV), pCDS->m_fp);

    nRSFV = CPL_MSBWORD32(nRSFV);
    nRSLV = CPL_MSBWORD32(nRSLV);

    if (nRSLV < nRSFV || nRSFV == 0 || nRSLV == 0 ||
        nRSFV - 1 >= static_cast<uint32_t>(nBlockXSize) ||
        nRSLV - 1 >= static_cast<uint32_t>(nBlockXSize) ||
        nRSFV >= this->nRTNB || nRSLV > this->nRTNB)
    {
        /* throw an error */
        CPLError(CE_Failure, CPLE_AppDefined,
                 "RSLV/RSFV values are not sane... oh dear.\n");
        return CE_Failure;
    }

    /* zero out the range line */
    memset(pImage, 0,
           static_cast<size_t>(nBlockXSize) *
               GDALGetDataTypeSizeBytes(eDataType));

    /* properly account for validity mask */
    if (nRSFV > 1)
    {
        VSIFSeekL(pCDS->m_fp,
                  static_cast<vsi_l_offset>(nRTNB) * (nBlockYOff + ITEM_SIZE) +
                      (nRSFV + 1) * ITEM_SIZE,
                  SEEK_SET);
    }

    /* Read the valid samples: */
    VSIFReadL(((char *)pImage) + (static_cast<size_t>(nRSFV - 1) * ITEM_SIZE),
              1, static_cast<size_t>(nRSLV - nRSFV + 1) * ITEM_SIZE,
              pCDS->m_fp);

#ifdef CPL_LSB
    GDALSwapWords(pImage, sizeof(int16_t), nBlockXSize * 2, sizeof(int16_t));
#endif

    if (pCDS->m_nVersion == 2)
    {
        // Convert from half-float to float32
        // Iterate starting the end to avoid overwriting first values
        for (int i = nBlockXSize * 2 - 1; i >= 0; --i)
        {
            static_cast<GUInt32 *>(pImage)[i] =
                CPLHalfToFloat(static_cast<GUInt16 *>(pImage)[i]);
        }
    }

    return CE_None;
}

/*****************************************************************************
 * COSARDataset Implementation
 *****************************************************************************/

COSARDataset::~COSARDataset()
{
    if (m_fp != nullptr)
    {
        VSIFCloseL(m_fp);
    }
}

GDALDataset *COSARDataset::Open(GDALOpenInfo *pOpenInfo)
{

    /* Check if we're actually a COSAR data set. */
    if (pOpenInfo->nHeaderBytes < VERSION_NUMBER_OFFSET + 4 ||
        pOpenInfo->fpL == nullptr)
        return nullptr;

    if (!STARTS_WITH_CI((char *)pOpenInfo->pabyHeader + MAGIC1_OFFSET, "CSAR"))
        return nullptr;

    uint32_t nVersionMSB;
    memcpy(&nVersionMSB, pOpenInfo->pabyHeader + VERSION_NUMBER_OFFSET,
           sizeof(uint32_t));
    const uint32_t nVersion = CPL_MSBWORD32(nVersionMSB);
    if (nVersion != 1 && nVersion != 2)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Confirm the requested access is supported.                      */
    /* -------------------------------------------------------------------- */
    if (pOpenInfo->eAccess == GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The COSAR driver does not support update access to existing"
                 " datasets.\n");
        return nullptr;
    }

    /* this is a cosar dataset */
    COSARDataset *pDS = new COSARDataset();
    pDS->m_nVersion = nVersion;

    /* steal fp */
    std::swap(pDS->m_fp, pOpenInfo->fpL);

    VSIFSeekL(pDS->m_fp, RS_OFFSET, SEEK_SET);
    int32_t nXSize;
    VSIFReadL(&nXSize, 1, sizeof(nXSize), pDS->m_fp);
    pDS->nRasterXSize = CPL_MSBWORD32(nXSize);

    int32_t nYSize;
    VSIFReadL(&nYSize, 1, sizeof(nYSize), pDS->m_fp);
    pDS->nRasterYSize = CPL_MSBWORD32(nYSize);

    if (!GDALCheckDatasetDimensions(pDS->nRasterXSize, pDS->nRasterYSize))
    {
        delete pDS;
        return nullptr;
    }

    VSIFSeekL(pDS->m_fp, RTNB_OFFSET, SEEK_SET);
    uint32_t nRTNB;
    VSIFReadL(&nRTNB, 1, sizeof(nRTNB), pDS->m_fp);
    nRTNB = CPL_MSBWORD32(nRTNB);

    /* Add raster band */
    pDS->SetBand(1, new COSARRasterBand(pDS, nRTNB));
    return pDS;
}

/* register the driver with GDAL */
void GDALRegister_COSAR()

{
    if (GDALGetDriverByName("cosar") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    poDriver->SetDescription("COSAR");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "COSAR Annotated Binary Matrix (TerraSAR-X)");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/cosar.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->pfnOpen = COSARDataset::Open;
    GetGDALDriverManager()->RegisterDriver(poDriver);
}

/**********************************************************************
 *
 * Project:  GDAL
 * Purpose:  Dataset that modifies the orientation of an underlying dataset
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 **********************************************************************
 * Copyright (c) 2022, Even Rouault, <even dot rouault at spatialys dot com>
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "gdalorienteddataset.h"

#include "gdal_utils.h"

#include <algorithm>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                      GDALOrientedRasterBand                          */
/************************************************************************/

class GDALOrientedRasterBand : public GDALRasterBand
{
    GDALRasterBand *m_poSrcBand;
    std::unique_ptr<GDALDataset> m_poCacheDS{};

    GDALOrientedRasterBand(const GDALOrientedRasterBand &) = delete;
    GDALOrientedRasterBand &operator=(const GDALOrientedRasterBand &) = delete;

  protected:
    CPLErr IReadBlock(int nBlockXOff, int nBlockYOff, void *pImage) override;

  public:
    GDALOrientedRasterBand(GDALOrientedDataset *poDSIn, int nBandIn);

    GDALColorInterp GetColorInterpretation() override
    {
        return m_poSrcBand->GetColorInterpretation();
    }
};

/************************************************************************/
/*                      GDALOrientedRasterBand()                        */
/************************************************************************/

GDALOrientedRasterBand::GDALOrientedRasterBand(GDALOrientedDataset *poDSIn,
                                               int nBandIn)
    : m_poSrcBand(poDSIn->m_poSrcDS->GetRasterBand(nBandIn))
{
    poDS = poDSIn;
    eDataType = m_poSrcBand->GetRasterDataType();
    if (poDSIn->m_eOrigin == GDALOrientedDataset::Origin::TOP_LEFT)
    {
        m_poSrcBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
    }
    else
    {
        nBlockXSize = poDS->GetRasterXSize();
        nBlockYSize = 1;
    }
}

/************************************************************************/
/*                  FlipLineHorizontally()                              */
/************************************************************************/

static void FlipLineHorizontally(void *pLine, int nDTSize, int nBlockXSize)
{
    switch (nDTSize)
    {
        case 1:
        {
            GByte *pabyLine = static_cast<GByte *>(pLine);
            for (int iX = 0; iX < nBlockXSize / 2; ++iX)
            {
                std::swap(pabyLine[iX], pabyLine[nBlockXSize - 1 - iX]);
            }
            break;
        }

        default:
        {
            GByte *pabyLine = static_cast<GByte *>(pLine);
            std::vector<GByte> abyTemp(nDTSize);
            for (int iX = 0; iX < nBlockXSize / 2; ++iX)
            {
                memcpy(&abyTemp[0], pabyLine + iX * nDTSize, nDTSize);
                memcpy(pabyLine + iX * nDTSize,
                       pabyLine + (nBlockXSize - 1 - iX) * nDTSize, nDTSize);
                memcpy(pabyLine + (nBlockXSize - 1 - iX) * nDTSize, &abyTemp[0],
                       nDTSize);
            }
            break;
        }
    }
}

/************************************************************************/
/*                            IReadBlock()                              */
/************************************************************************/

CPLErr GDALOrientedRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                          void *pImage)
{
    auto l_poDS = cpl::down_cast<GDALOrientedDataset *>(poDS);

    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
    if (m_poCacheDS == nullptr &&
        l_poDS->m_eOrigin != GDALOrientedDataset::Origin::TOP_LEFT &&
        l_poDS->m_eOrigin != GDALOrientedDataset::Origin::TOP_RIGHT)
    {
        auto poGTiffDrv = GetGDALDriverManager()->GetDriverByName("GTiff");
        CPLStringList aosOptions;
        aosOptions.AddString("-f");
        aosOptions.AddString(poGTiffDrv ? "GTiff" : "MEM");
        aosOptions.AddString("-b");
        aosOptions.AddString(CPLSPrintf("%d", nBand));
        if (poGTiffDrv)
        {
            aosOptions.AddString("-co");
            aosOptions.AddString("TILED=YES");
        }
        std::string osTmpName;
        if (poGTiffDrv)
        {
            if (static_cast<GIntBig>(nRasterXSize) * nRasterYSize * nDTSize >
                10 * 1024 * 1024)
            {
                osTmpName = CPLGenerateTempFilename(nullptr);
            }
            else
            {
                osTmpName =
                    CPLSPrintf("/vsimem/_gdalorienteddataset/%p.tif", this);
            }
        }
        GDALTranslateOptions *psOptions =
            GDALTranslateOptionsNew(aosOptions.List(), nullptr);
        if (psOptions == nullptr)
            return CE_Failure;
        GDALDatasetH hOutDS = GDALTranslate(
            osTmpName.c_str(), GDALDataset::ToHandle(l_poDS->m_poSrcDS),
            psOptions, nullptr);
        GDALTranslateOptionsFree(psOptions);
        if (hOutDS == nullptr)
            return CE_Failure;
        m_poCacheDS.reset(GDALDataset::FromHandle(hOutDS));
        m_poCacheDS->MarkSuppressOnClose();
    }

    CPLErr eErr = CE_None;
    switch (l_poDS->m_eOrigin)
    {
        case GDALOrientedDataset::Origin::TOP_LEFT:
        {
            eErr = m_poSrcBand->ReadBlock(nBlockXOff, nBlockYOff, pImage);
            break;
        }

        case GDALOrientedDataset::Origin::TOP_RIGHT:
        {
            CPLAssert(nBlockXSize == nRasterXSize);
            CPLAssert(nBlockYSize == 1);
            if (m_poSrcBand->RasterIO(GF_Read, 0, nBlockYOff, nRasterXSize, 1,
                                      pImage, nRasterXSize, 1, eDataType, 0, 0,
                                      nullptr) != CE_None)
            {
                return CE_Failure;
            }
            FlipLineHorizontally(pImage, nDTSize, nBlockXSize);
            break;
        }

        case GDALOrientedDataset::Origin::BOT_RIGHT:
        case GDALOrientedDataset::Origin::BOT_LEFT:
        {
            CPLAssert(nBlockXSize == nRasterXSize);
            CPLAssert(nBlockYSize == 1);
            if (m_poCacheDS->GetRasterBand(1)->RasterIO(
                    GF_Read, 0, nRasterYSize - 1 - nBlockYOff, nRasterXSize, 1,
                    pImage, nRasterXSize, 1, eDataType, 0, 0,
                    nullptr) != CE_None)
            {
                return CE_Failure;
            }
            if (l_poDS->m_eOrigin == GDALOrientedDataset::Origin::BOT_RIGHT)
                FlipLineHorizontally(pImage, nDTSize, nBlockXSize);
            break;
        }

        case GDALOrientedDataset::Origin::LEFT_TOP:
        case GDALOrientedDataset::Origin::RIGHT_TOP:
        {
            CPLAssert(nBlockXSize == nRasterXSize);
            CPLAssert(nBlockYSize == 1);
            if (m_poCacheDS->GetRasterBand(1)->RasterIO(
                    GF_Read, nBlockYOff, 0, 1, nRasterXSize, pImage, 1,
                    nRasterXSize, eDataType, 0, 0, nullptr) != CE_None)
            {
                return CE_Failure;
            }
            if (l_poDS->m_eOrigin == GDALOrientedDataset::Origin::RIGHT_TOP)
                FlipLineHorizontally(pImage, nDTSize, nBlockXSize);
            break;
        }

        case GDALOrientedDataset::Origin::RIGHT_BOT:
        case GDALOrientedDataset::Origin::LEFT_BOT:
        {
            CPLAssert(nBlockXSize == nRasterXSize);
            CPLAssert(nBlockYSize == 1);
            if (m_poCacheDS->GetRasterBand(1)->RasterIO(
                    GF_Read, nRasterYSize - 1 - nBlockYOff, 0, 1, nRasterXSize,
                    pImage, 1, nRasterXSize, eDataType, 0, 0,
                    nullptr) != CE_None)
            {
                return CE_Failure;
            }
            if (l_poDS->m_eOrigin == GDALOrientedDataset::Origin::RIGHT_BOT)
                FlipLineHorizontally(pImage, nDTSize, nBlockXSize);
            break;
        }
    }

    return eErr;
}

/************************************************************************/
/*                         GDALOrientedDataset()                        */
/************************************************************************/

GDALOrientedDataset::GDALOrientedDataset(GDALDataset *poSrcDataset,
                                         Origin eOrigin)
    : m_poSrcDS(poSrcDataset), m_eOrigin(eOrigin)
{
    switch (eOrigin)
    {
        case GDALOrientedDataset::Origin::TOP_LEFT:
        case GDALOrientedDataset::Origin::TOP_RIGHT:
        case GDALOrientedDataset::Origin::BOT_RIGHT:
        case GDALOrientedDataset::Origin::BOT_LEFT:
        {
            nRasterXSize = poSrcDataset->GetRasterXSize();
            nRasterYSize = poSrcDataset->GetRasterYSize();
            break;
        }

        case GDALOrientedDataset::Origin::LEFT_TOP:
        case GDALOrientedDataset::Origin::RIGHT_TOP:
        case GDALOrientedDataset::Origin::RIGHT_BOT:
        case GDALOrientedDataset::Origin::LEFT_BOT:
        {
            // Permute (x, y)
            nRasterXSize = poSrcDataset->GetRasterYSize();
            nRasterYSize = poSrcDataset->GetRasterXSize();
            break;
        }
    }

    const int nSrcBands = poSrcDataset->GetRasterCount();
    for (int i = 1; i <= nSrcBands; ++i)
    {
        SetBand(i, new GDALOrientedRasterBand(this, i));
    }
}

/************************************************************************/
/*                         GDALOrientedDataset()                        */
/************************************************************************/

GDALOrientedDataset::GDALOrientedDataset(
    std::unique_ptr<GDALDataset> &&poSrcDataset, Origin eOrigin)
    : GDALOrientedDataset(poSrcDataset.get(), eOrigin)
{
    // cppcheck-suppress useInitializationList
    m_poSrcDSHolder = std::move(poSrcDataset);
}

/************************************************************************/
/*                           GetMetadata()                              */
/************************************************************************/

char **GDALOrientedDataset::GetMetadata(const char *pszDomain)
{
    if (pszDomain == nullptr || pszDomain[0] == '\0')
    {
        if (m_aosSrcMD.empty())
        {
            m_aosSrcMD.Assign(CSLDuplicate(m_poSrcDS->GetMetadata(pszDomain)));
            const char *pszOrientation =
                m_aosSrcMD.FetchNameValue("EXIF_Orientation");
            if (pszOrientation)
            {
                m_aosSrcMD.SetNameValue("original_EXIF_Orientation",
                                        pszOrientation);
                m_aosSrcMD.SetNameValue("EXIF_Orientation", nullptr);
            }
        }
        return m_aosSrcMD.List();
    }
    if (EQUAL(pszDomain, "EXIF"))
    {
        if (m_aosSrcMD_EXIF.empty())
        {
            m_aosSrcMD_EXIF.Assign(
                CSLDuplicate(m_poSrcDS->GetMetadata(pszDomain)));
            const char *pszOrientation =
                m_aosSrcMD_EXIF.FetchNameValue("EXIF_Orientation");
            if (pszOrientation)
            {
                m_aosSrcMD_EXIF.SetNameValue("original_EXIF_Orientation",
                                             pszOrientation);
                m_aosSrcMD_EXIF.SetNameValue("EXIF_Orientation", nullptr);
            }
        }
        return m_aosSrcMD_EXIF.List();
    }
    return m_poSrcDS->GetMetadata(pszDomain);
}

/************************************************************************/
/*                       GetMetadataItem()                              */
/************************************************************************/

const char *GDALOrientedDataset::GetMetadataItem(const char *pszName,
                                                 const char *pszDomain)
{
    return CSLFetchNameValue(GetMetadata(pszDomain), pszName);
}

//! @endcond

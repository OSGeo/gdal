/******************************************************************************
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Write/set operations on GTiffRasterBand
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

#include "gtiffrasterband.h"
#include "gtiffdataset.h"

#include <algorithm>
#include <limits>

#include "cpl_vsi_virtual.h"
#include "gdal_priv_templates.hpp"
#include "gtiff.h"
#include "tifvsi.h"

/************************************************************************/
/*                           SetDefaultRAT()                            */
/************************************************************************/

CPLErr GTiffRasterBand::SetDefaultRAT(const GDALRasterAttributeTable *poRAT)
{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();
    return GDALPamRasterBand::SetDefaultRAT(poRAT);
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr GTiffRasterBand::IWriteBlock(int nBlockXOff, int nBlockYOff,
                                    void *pImage)

{
    m_poGDS->Crystalize();

    if (m_poGDS->m_bDebugDontWriteBlocks)
        return CE_None;

    if (m_poGDS->m_bWriteError)
    {
        // Report as an error if a previously loaded block couldn't be written
        // correctly.
        return CE_Failure;
    }

    const int nBlockId = ComputeBlockId(nBlockXOff, nBlockYOff);

    /* -------------------------------------------------------------------- */
    /*      Handle case of "separate" images                                */
    /* -------------------------------------------------------------------- */
    if (m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE ||
        m_poGDS->nBands == 1)
    {
        const CPLErr eErr =
            m_poGDS->WriteEncodedTileOrStrip(nBlockId, pImage, true);

        return eErr;
    }

    /* -------------------------------------------------------------------- */
    /*      Handle case of pixel interleaved (PLANARCONFIG_CONTIG) images.  */
    /* -------------------------------------------------------------------- */
    // Why 10 ? Somewhat arbitrary
    constexpr int MAX_BANDS_FOR_DIRTY_CHECK = 10;
    GDALRasterBlock *apoBlocks[MAX_BANDS_FOR_DIRTY_CHECK] = {};
    const int nBands = m_poGDS->nBands;
    bool bAllBlocksDirty = false;

    /* -------------------------------------------------------------------- */
    /*     If all blocks are cached and dirty then we do not need to reload */
    /*     the tile/strip from disk                                         */
    /* -------------------------------------------------------------------- */
    if (nBands <= MAX_BANDS_FOR_DIRTY_CHECK)
    {
        bAllBlocksDirty = true;
        for (int iBand = 0; iBand < nBands; ++iBand)
        {
            if (iBand + 1 != nBand)
            {
                apoBlocks[iBand] =
                    cpl::down_cast<GTiffRasterBand *>(
                        m_poGDS->GetRasterBand(iBand + 1))
                        ->TryGetLockedBlockRef(nBlockXOff, nBlockYOff);

                if (apoBlocks[iBand] == nullptr)
                {
                    bAllBlocksDirty = false;
                }
                else if (!apoBlocks[iBand]->GetDirty())
                {
                    apoBlocks[iBand]->DropLock();
                    apoBlocks[iBand] = nullptr;
                    bAllBlocksDirty = false;
                }
            }
            else
                apoBlocks[iBand] = nullptr;
        }
#if DEBUG_VERBOSE
        if (bAllBlocksDirty)
            CPLDebug("GTIFF", "Saved reloading block %d", nBlockId);
        else
            CPLDebug("GTIFF", "Must reload block %d", nBlockId);
#endif
    }

    {
        const CPLErr eErr = m_poGDS->LoadBlockBuf(nBlockId, !bAllBlocksDirty);
        if (eErr != CE_None)
        {
            if (nBands <= MAX_BANDS_FOR_DIRTY_CHECK)
            {
                for (int iBand = 0; iBand < nBands; ++iBand)
                {
                    if (apoBlocks[iBand] != nullptr)
                        apoBlocks[iBand]->DropLock();
                }
            }
            return eErr;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      On write of pixel interleaved data, we might as well flush      */
    /*      out any other bands that are dirty in our cache.  This is       */
    /*      especially helpful when writing compressed blocks.              */
    /* -------------------------------------------------------------------- */
    const int nWordBytes = m_poGDS->m_nBitsPerSample / 8;

    for (int iBand = 0; iBand < nBands; ++iBand)
    {
        const GByte *pabyThisImage = nullptr;
        GDALRasterBlock *poBlock = nullptr;

        if (iBand + 1 == nBand)
        {
            pabyThisImage = static_cast<GByte *>(pImage);
        }
        else
        {
            if (nBands <= MAX_BANDS_FOR_DIRTY_CHECK)
                poBlock = apoBlocks[iBand];
            else
                poBlock = cpl::down_cast<GTiffRasterBand *>(
                              m_poGDS->GetRasterBand(iBand + 1))
                              ->TryGetLockedBlockRef(nBlockXOff, nBlockYOff);

            if (poBlock == nullptr)
                continue;

            if (!poBlock->GetDirty())
            {
                poBlock->DropLock();
                continue;
            }

            pabyThisImage = static_cast<GByte *>(poBlock->GetDataRef());
        }

        GByte *pabyOut = m_poGDS->m_pabyBlockBuf + iBand * nWordBytes;

        GDALCopyWords64(pabyThisImage, eDataType, nWordBytes, pabyOut,
                        eDataType, nWordBytes * nBands,
                        static_cast<GPtrDiff_t>(nBlockXSize) * nBlockYSize);

        if (poBlock != nullptr)
        {
            poBlock->MarkClean();
            poBlock->DropLock();
        }
    }

    if (bAllBlocksDirty)
    {
        // We can synchronously write the block now.
        const CPLErr eErr = m_poGDS->WriteEncodedTileOrStrip(
            nBlockId, m_poGDS->m_pabyBlockBuf, true);
        m_poGDS->m_bLoadedBlockDirty = false;
        return eErr;
    }

    m_poGDS->m_bLoadedBlockDirty = true;

    return CE_None;
}

/************************************************************************/
/*                           SetDescription()                           */
/************************************************************************/

void GTiffRasterBand::SetDescription(const char *pszDescription)

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    if (pszDescription == nullptr)
        pszDescription = "";

    if (m_osDescription != pszDescription)
        m_poGDS->m_bMetadataChanged = true;

    m_osDescription = pszDescription;
}

/************************************************************************/
/*                             SetOffset()                              */
/************************************************************************/

CPLErr GTiffRasterBand::SetOffset(double dfNewValue)

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    if (!m_bHaveOffsetScale || dfNewValue != m_dfOffset)
        m_poGDS->m_bMetadataChanged = true;

    m_bHaveOffsetScale = true;
    m_dfOffset = dfNewValue;
    return CE_None;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

CPLErr GTiffRasterBand::SetScale(double dfNewValue)

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    if (!m_bHaveOffsetScale || dfNewValue != m_dfScale)
        m_poGDS->m_bMetadataChanged = true;

    m_bHaveOffsetScale = true;
    m_dfScale = dfNewValue;
    return CE_None;
}

/************************************************************************/
/*                           SetUnitType()                              */
/************************************************************************/

CPLErr GTiffRasterBand::SetUnitType(const char *pszNewValue)

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    CPLString osNewValue(pszNewValue ? pszNewValue : "");
    if (osNewValue.compare(m_osUnitType) != 0)
        m_poGDS->m_bMetadataChanged = true;

    m_osUnitType = std::move(osNewValue);
    return CE_None;
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr GTiffRasterBand::SetMetadata(char **papszMD, const char *pszDomain)

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    if (m_poGDS->m_bStreamingOut && m_poGDS->m_bCrystalized)
    {
        ReportError(CE_Failure, CPLE_NotSupported,
                    "Cannot modify metadata at that point in a streamed "
                    "output file");
        return CE_Failure;
    }

    CPLErr eErr = CE_None;
    if (eAccess == GA_Update)
    {
        if (pszDomain == nullptr || !EQUAL(pszDomain, "_temporary_"))
        {
            if (papszMD != nullptr || GetMetadata(pszDomain) != nullptr)
            {
                m_poGDS->m_bMetadataChanged = true;
                // Cancel any existing metadata from PAM file.
                if (GDALPamRasterBand::GetMetadata(pszDomain) != nullptr)
                    GDALPamRasterBand::SetMetadata(nullptr, pszDomain);
            }
        }
    }
    else
    {
        CPLDebug(
            "GTIFF",
            "GTiffRasterBand::SetMetadata() goes to PAM instead of TIFF tags");
        eErr = GDALPamRasterBand::SetMetadata(papszMD, pszDomain);
    }

    if (eErr == CE_None)
    {
        eErr = m_oGTiffMDMD.SetMetadata(papszMD, pszDomain);
    }
    return eErr;
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr GTiffRasterBand::SetMetadataItem(const char *pszName,
                                        const char *pszValue,
                                        const char *pszDomain)

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    if (m_poGDS->m_bStreamingOut && m_poGDS->m_bCrystalized)
    {
        ReportError(CE_Failure, CPLE_NotSupported,
                    "Cannot modify metadata at that point in a streamed "
                    "output file");
        return CE_Failure;
    }

    CPLErr eErr = CE_None;
    if (eAccess == GA_Update)
    {
        if (pszDomain == nullptr || !EQUAL(pszDomain, "_temporary_"))
        {
            m_poGDS->m_bMetadataChanged = true;
            // Cancel any existing metadata from PAM file.
            if (GDALPamRasterBand::GetMetadataItem(pszName, pszDomain) !=
                nullptr)
                GDALPamRasterBand::SetMetadataItem(pszName, nullptr, pszDomain);
        }
    }
    else
    {
        CPLDebug("GTIFF", "GTiffRasterBand::SetMetadataItem() goes to PAM "
                          "instead of TIFF tags");
        eErr = GDALPamRasterBand::SetMetadataItem(pszName, pszValue, pszDomain);
    }

    if (eErr == CE_None)
    {
        eErr = m_oGTiffMDMD.SetMetadataItem(pszName, pszValue, pszDomain);
    }
    return eErr;
}

/************************************************************************/
/*                       SetColorInterpretation()                       */
/************************************************************************/

CPLErr GTiffRasterBand::SetColorInterpretation(GDALColorInterp eInterp)

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    if (eInterp == m_eBandInterp)
        return CE_None;

    m_eBandInterp = eInterp;

    if (eAccess != GA_Update)
    {
        CPLDebug("GTIFF",
                 "ColorInterpretation %s for band %d goes to PAM "
                 "instead of TIFF tag",
                 GDALGetColorInterpretationName(eInterp), nBand);
        return GDALPamRasterBand::SetColorInterpretation(eInterp);
    }

    m_poGDS->m_bNeedsRewrite = true;
    m_poGDS->m_bMetadataChanged = true;

    // Try to autoset TIFFTAG_PHOTOMETRIC = PHOTOMETRIC_RGB if possible.
    if (m_poGDS->nBands >= 3 && m_poGDS->m_nCompression != COMPRESSION_JPEG &&
        m_poGDS->m_nPhotometric != PHOTOMETRIC_RGB &&
        CSLFetchNameValue(m_poGDS->m_papszCreationOptions, "PHOTOMETRIC") ==
            nullptr &&
        ((nBand == 1 && eInterp == GCI_RedBand) ||
         (nBand == 2 && eInterp == GCI_GreenBand) ||
         (nBand == 3 && eInterp == GCI_BlueBand)))
    {
        if (m_poGDS->GetRasterBand(1)->GetColorInterpretation() ==
                GCI_RedBand &&
            m_poGDS->GetRasterBand(2)->GetColorInterpretation() ==
                GCI_GreenBand &&
            m_poGDS->GetRasterBand(3)->GetColorInterpretation() == GCI_BlueBand)
        {
            m_poGDS->m_nPhotometric = PHOTOMETRIC_RGB;
            TIFFSetField(m_poGDS->m_hTIFF, TIFFTAG_PHOTOMETRIC,
                         m_poGDS->m_nPhotometric);

            // We need to update the number of extra samples.
            uint16_t *v = nullptr;
            uint16_t count = 0;
            const uint16_t nNewExtraSamplesCount =
                static_cast<uint16_t>(m_poGDS->nBands - 3);
            if (m_poGDS->nBands >= 4 &&
                TIFFGetField(m_poGDS->m_hTIFF, TIFFTAG_EXTRASAMPLES, &count,
                             &v) &&
                count > nNewExtraSamplesCount)
            {
                uint16_t *const pasNewExtraSamples = static_cast<uint16_t *>(
                    CPLMalloc(nNewExtraSamplesCount * sizeof(uint16_t)));
                memcpy(pasNewExtraSamples, v + count - nNewExtraSamplesCount,
                       nNewExtraSamplesCount * sizeof(uint16_t));

                TIFFSetField(m_poGDS->m_hTIFF, TIFFTAG_EXTRASAMPLES,
                             nNewExtraSamplesCount, pasNewExtraSamples);

                CPLFree(pasNewExtraSamples);
            }
        }
        return CE_None;
    }

    // On the contrary, cancel the above if needed
    if (m_poGDS->m_nCompression != COMPRESSION_JPEG &&
        m_poGDS->m_nPhotometric == PHOTOMETRIC_RGB &&
        CSLFetchNameValue(m_poGDS->m_papszCreationOptions, "PHOTOMETRIC") ==
            nullptr &&
        ((nBand == 1 && eInterp != GCI_RedBand) ||
         (nBand == 2 && eInterp != GCI_GreenBand) ||
         (nBand == 3 && eInterp != GCI_BlueBand)))
    {
        m_poGDS->m_nPhotometric = PHOTOMETRIC_MINISBLACK;
        TIFFSetField(m_poGDS->m_hTIFF, TIFFTAG_PHOTOMETRIC,
                     m_poGDS->m_nPhotometric);

        // We need to update the number of extra samples.
        uint16_t *v = nullptr;
        uint16_t count = 0;
        const uint16_t nNewExtraSamplesCount =
            static_cast<uint16_t>(m_poGDS->nBands - 1);
        if (m_poGDS->nBands >= 2)
        {
            TIFFGetField(m_poGDS->m_hTIFF, TIFFTAG_EXTRASAMPLES, &count, &v);
            if (nNewExtraSamplesCount > count)
            {
                uint16_t *const pasNewExtraSamples = static_cast<uint16_t *>(
                    CPLMalloc(nNewExtraSamplesCount * sizeof(uint16_t)));
                for (int i = 0;
                     i < static_cast<int>(nNewExtraSamplesCount - count); ++i)
                    pasNewExtraSamples[i] = EXTRASAMPLE_UNSPECIFIED;
                if (count > 0)
                {
                    memcpy(pasNewExtraSamples + nNewExtraSamplesCount - count,
                           v, count * sizeof(uint16_t));
                }

                TIFFSetField(m_poGDS->m_hTIFF, TIFFTAG_EXTRASAMPLES,
                             nNewExtraSamplesCount, pasNewExtraSamples);

                CPLFree(pasNewExtraSamples);
            }
        }
    }

    // Mark alpha band / undefined in extrasamples.
    if (eInterp == GCI_AlphaBand || eInterp == GCI_Undefined)
    {
        uint16_t *v = nullptr;
        uint16_t count = 0;
        if (TIFFGetField(m_poGDS->m_hTIFF, TIFFTAG_EXTRASAMPLES, &count, &v))
        {
            const int nBaseSamples = m_poGDS->m_nSamplesPerPixel - count;

            if (eInterp == GCI_AlphaBand)
            {
                for (int i = 1; i <= m_poGDS->nBands; ++i)
                {
                    if (i != nBand &&
                        m_poGDS->GetRasterBand(i)->GetColorInterpretation() ==
                            GCI_AlphaBand)
                    {
                        if (i == nBaseSamples + 1 &&
                            CSLFetchNameValue(m_poGDS->m_papszCreationOptions,
                                              "ALPHA") != nullptr)
                        {
                            ReportError(
                                CE_Warning, CPLE_AppDefined,
                                "Band %d was already identified as alpha band, "
                                "and band %d is now marked as alpha too. "
                                "Presumably ALPHA creation option is not "
                                "needed",
                                i, nBand);
                        }
                        else
                        {
                            ReportError(
                                CE_Warning, CPLE_AppDefined,
                                "Band %d was already identified as alpha band, "
                                "and band %d is now marked as alpha too",
                                i, nBand);
                        }
                    }
                }
            }

            if (nBand > nBaseSamples && nBand - nBaseSamples - 1 < count)
            {
                // We need to allocate a new array as (current) libtiff
                // versions will not like that we reuse the array we got from
                // TIFFGetField().

                uint16_t *pasNewExtraSamples = static_cast<uint16_t *>(
                    CPLMalloc(count * sizeof(uint16_t)));
                memcpy(pasNewExtraSamples, v, count * sizeof(uint16_t));
                if (eInterp == GCI_AlphaBand)
                {
                    pasNewExtraSamples[nBand - nBaseSamples - 1] =
                        GTiffGetAlphaValue(
                            CPLGetConfigOption("GTIFF_ALPHA", nullptr),
                            DEFAULT_ALPHA_TYPE);
                }
                else
                {
                    pasNewExtraSamples[nBand - nBaseSamples - 1] =
                        EXTRASAMPLE_UNSPECIFIED;
                }

                TIFFSetField(m_poGDS->m_hTIFF, TIFFTAG_EXTRASAMPLES, count,
                             pasNewExtraSamples);

                CPLFree(pasNewExtraSamples);

                return CE_None;
            }
        }
    }

    if (m_poGDS->m_nPhotometric != PHOTOMETRIC_MINISBLACK &&
        CSLFetchNameValue(m_poGDS->m_papszCreationOptions, "PHOTOMETRIC") ==
            nullptr)
    {
        m_poGDS->m_nPhotometric = PHOTOMETRIC_MINISBLACK;
        TIFFSetField(m_poGDS->m_hTIFF, TIFFTAG_PHOTOMETRIC,
                     m_poGDS->m_nPhotometric);
    }

    return CE_None;
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr GTiffRasterBand::SetColorTable(GDALColorTable *poCT)

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    /* -------------------------------------------------------------------- */
    /*      Check if this is even a candidate for applying a PCT.           */
    /* -------------------------------------------------------------------- */
    if (eAccess == GA_Update)
    {
        if (nBand != 1)
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "SetColorTable() can only be called on band 1.");
            return CE_Failure;
        }

        if (m_poGDS->m_nSamplesPerPixel != 1 &&
            m_poGDS->m_nSamplesPerPixel != 2)
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "SetColorTable() not supported for multi-sample TIFF "
                        "files.");
            return CE_Failure;
        }

        if (eDataType != GDT_Byte && eDataType != GDT_UInt16)
        {
            ReportError(
                CE_Failure, CPLE_NotSupported,
                "SetColorTable() only supported for Byte or UInt16 bands "
                "in TIFF format.");
            return CE_Failure;
        }

        // Clear any existing PAM color table
        if (GDALPamRasterBand::GetColorTable() != nullptr)
        {
            GDALPamRasterBand::SetColorTable(nullptr);
            GDALPamRasterBand::SetColorInterpretation(GCI_Undefined);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Is this really a request to clear the color table?              */
    /* -------------------------------------------------------------------- */
    if (poCT == nullptr || poCT->GetColorEntryCount() == 0)
    {
        if (eAccess == GA_Update)
        {
            TIFFSetField(m_poGDS->m_hTIFF, TIFFTAG_PHOTOMETRIC,
                         PHOTOMETRIC_MINISBLACK);

            TIFFUnsetField(m_poGDS->m_hTIFF, TIFFTAG_COLORMAP);
        }

        if (m_poGDS->m_poColorTable)
        {
            delete m_poGDS->m_poColorTable;
            m_poGDS->m_poColorTable = nullptr;
        }

        return CE_None;
    }

    /* -------------------------------------------------------------------- */
    /*      Write out the colortable, and update the configuration.         */
    /* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;
    if (eAccess == GA_Update)
    {
        int nColors = 65536;

        if (eDataType == GDT_Byte)
            nColors = 256;

        unsigned short *panTRed = static_cast<unsigned short *>(
            CPLMalloc(sizeof(unsigned short) * nColors));
        unsigned short *panTGreen = static_cast<unsigned short *>(
            CPLMalloc(sizeof(unsigned short) * nColors));
        unsigned short *panTBlue = static_cast<unsigned short *>(
            CPLMalloc(sizeof(unsigned short) * nColors));

        for (int iColor = 0; iColor < nColors; ++iColor)
        {
            if (iColor < poCT->GetColorEntryCount())
            {
                GDALColorEntry sRGB;
                poCT->GetColorEntryAsRGB(iColor, &sRGB);

                panTRed[iColor] = static_cast<unsigned short>(257 * sRGB.c1);
                panTGreen[iColor] = static_cast<unsigned short>(257 * sRGB.c2);
                panTBlue[iColor] = static_cast<unsigned short>(257 * sRGB.c3);
            }
            else
            {
                panTRed[iColor] = 0;
                panTGreen[iColor] = 0;
                panTBlue[iColor] = 0;
            }
        }

        TIFFSetField(m_poGDS->m_hTIFF, TIFFTAG_PHOTOMETRIC,
                     PHOTOMETRIC_PALETTE);
        TIFFSetField(m_poGDS->m_hTIFF, TIFFTAG_COLORMAP, panTRed, panTGreen,
                     panTBlue);

        CPLFree(panTRed);
        CPLFree(panTGreen);
        CPLFree(panTBlue);

        // libtiff 3.X needs setting this in all cases (creation or update)
        // whereas libtiff 4.X would just need it if there
        // was no color table before.
        m_poGDS->m_bNeedsRewrite = true;
    }
    else
    {
        eErr = GDALPamRasterBand::SetColorTable(poCT);
    }

    if (m_poGDS->m_poColorTable)
        delete m_poGDS->m_poColorTable;

    m_poGDS->m_poColorTable = poCT->Clone();
    m_eBandInterp = GCI_PaletteIndex;

    return eErr;
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr GTiffRasterBand::SetNoDataValue(double dfNoData)

{
    const auto SetNoDataMembers = [this, dfNoData]()
    {
        m_bNoDataSet = true;
        m_dfNoDataValue = dfNoData;

        m_poGDS->m_bNoDataSet = true;
        m_poGDS->m_dfNoDataValue = dfNoData;

        if (eDataType == GDT_Int64 && GDALIsValueExactAs<int64_t>(dfNoData))
        {
            m_bNoDataSetAsInt64 = true;
            m_nNoDataValueInt64 = static_cast<int64_t>(dfNoData);

            m_poGDS->m_bNoDataSetAsInt64 = true;
            m_poGDS->m_nNoDataValueInt64 = static_cast<int64_t>(dfNoData);
        }
        else if (eDataType == GDT_UInt64 &&
                 GDALIsValueExactAs<uint64_t>(dfNoData))
        {
            m_bNoDataSetAsUInt64 = true;
            m_nNoDataValueUInt64 = static_cast<uint64_t>(dfNoData);

            m_poGDS->m_bNoDataSetAsUInt64 = true;
            m_poGDS->m_nNoDataValueUInt64 = static_cast<uint64_t>(dfNoData);
        }
    };

    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    if (m_poGDS->m_bNoDataSet &&
        (m_poGDS->m_dfNoDataValue == dfNoData ||
         (std::isnan(m_poGDS->m_dfNoDataValue) && std::isnan(dfNoData))))
    {
        ResetNoDataValues(false);

        SetNoDataMembers();

        return CE_None;
    }

    if (m_poGDS->nBands > 1 && m_poGDS->m_eProfile == GTiffProfile::GDALGEOTIFF)
    {
        int bOtherBandHasNoData = FALSE;
        const int nOtherBand = nBand > 1 ? 1 : 2;
        double dfOtherNoData = m_poGDS->GetRasterBand(nOtherBand)
                                   ->GetNoDataValue(&bOtherBandHasNoData);
        if (bOtherBandHasNoData && dfOtherNoData != dfNoData)
        {
            ReportError(
                CE_Warning, CPLE_AppDefined,
                "Setting nodata to %.18g on band %d, but band %d has nodata "
                "at %.18g. The TIFFTAG_GDAL_NODATA only support one value "
                "per dataset. This value of %.18g will be used for all bands "
                "on re-opening",
                dfNoData, nBand, nOtherBand, dfOtherNoData, dfNoData);
        }
    }

    if (m_poGDS->m_bStreamingOut && m_poGDS->m_bCrystalized)
    {
        ReportError(
            CE_Failure, CPLE_NotSupported,
            "Cannot modify nodata at that point in a streamed output file");
        return CE_Failure;
    }

    CPLErr eErr = CE_None;
    if (eAccess == GA_Update)
    {
        m_poGDS->m_bNoDataChanged = true;
        int bSuccess = FALSE;
        CPL_IGNORE_RET_VAL(GDALPamRasterBand::GetNoDataValue(&bSuccess));
        if (bSuccess)
        {
            // Cancel any existing nodata from PAM file.
            eErr = GDALPamRasterBand::DeleteNoDataValue();
        }
    }
    else
    {
        CPLDebug("GTIFF", "SetNoDataValue() goes to PAM instead of TIFF tags");
        eErr = GDALPamRasterBand::SetNoDataValue(dfNoData);
    }

    if (eErr == CE_None)
    {
        ResetNoDataValues(true);

        SetNoDataMembers();
    }

    return eErr;
}

/************************************************************************/
/*                       SetNoDataValueAsInt64()                        */
/************************************************************************/

CPLErr GTiffRasterBand::SetNoDataValueAsInt64(int64_t nNoData)

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    if (m_poGDS->m_bNoDataSetAsInt64 && m_poGDS->m_nNoDataValueInt64 == nNoData)
    {
        ResetNoDataValues(false);

        m_bNoDataSetAsInt64 = true;
        m_nNoDataValueInt64 = nNoData;

        return CE_None;
    }

    if (m_poGDS->nBands > 1 && m_poGDS->m_eProfile == GTiffProfile::GDALGEOTIFF)
    {
        int bOtherBandHasNoData = FALSE;
        const int nOtherBand = nBand > 1 ? 1 : 2;
        const auto nOtherNoData =
            m_poGDS->GetRasterBand(nOtherBand)
                ->GetNoDataValueAsInt64(&bOtherBandHasNoData);
        if (bOtherBandHasNoData && nOtherNoData != nNoData)
        {
            ReportError(CE_Warning, CPLE_AppDefined,
                        "Setting nodata to " CPL_FRMT_GIB
                        " on band %d, but band %d has nodata "
                        "at " CPL_FRMT_GIB
                        ". The TIFFTAG_GDAL_NODATA only support one value "
                        "per dataset. This value of " CPL_FRMT_GIB
                        " will be used for all bands "
                        "on re-opening",
                        static_cast<GIntBig>(nNoData), nBand, nOtherBand,
                        static_cast<GIntBig>(nOtherNoData),
                        static_cast<GIntBig>(nNoData));
        }
    }

    if (m_poGDS->m_bStreamingOut && m_poGDS->m_bCrystalized)
    {
        ReportError(
            CE_Failure, CPLE_NotSupported,
            "Cannot modify nodata at that point in a streamed output file");
        return CE_Failure;
    }

    CPLErr eErr = CE_None;
    if (eAccess == GA_Update)
    {
        m_poGDS->m_bNoDataChanged = true;
        int bSuccess = FALSE;
        CPL_IGNORE_RET_VAL(GDALPamRasterBand::GetNoDataValueAsInt64(&bSuccess));
        if (bSuccess)
        {
            // Cancel any existing nodata from PAM file.
            eErr = GDALPamRasterBand::DeleteNoDataValue();
        }
    }
    else
    {
        CPLDebug("GTIFF", "SetNoDataValue() goes to PAM instead of TIFF tags");
        eErr = GDALPamRasterBand::SetNoDataValueAsInt64(nNoData);
    }

    if (eErr == CE_None)
    {
        ResetNoDataValues(true);

        m_poGDS->m_bNoDataSetAsInt64 = true;
        m_poGDS->m_nNoDataValueInt64 = nNoData;
    }

    return eErr;
}

/************************************************************************/
/*                      SetNoDataValueAsUInt64()                        */
/************************************************************************/

CPLErr GTiffRasterBand::SetNoDataValueAsUInt64(uint64_t nNoData)

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    if (m_poGDS->m_bNoDataSetAsUInt64 &&
        m_poGDS->m_nNoDataValueUInt64 == nNoData)
    {
        ResetNoDataValues(false);

        m_bNoDataSetAsUInt64 = true;
        m_nNoDataValueUInt64 = nNoData;

        return CE_None;
    }

    if (m_poGDS->nBands > 1 && m_poGDS->m_eProfile == GTiffProfile::GDALGEOTIFF)
    {
        int bOtherBandHasNoData = FALSE;
        const int nOtherBand = nBand > 1 ? 1 : 2;
        const auto nOtherNoData =
            m_poGDS->GetRasterBand(nOtherBand)
                ->GetNoDataValueAsUInt64(&bOtherBandHasNoData);
        if (bOtherBandHasNoData && nOtherNoData != nNoData)
        {
            ReportError(CE_Warning, CPLE_AppDefined,
                        "Setting nodata to " CPL_FRMT_GUIB
                        " on band %d, but band %d has nodata "
                        "at " CPL_FRMT_GUIB
                        ". The TIFFTAG_GDAL_NODATA only support one value "
                        "per dataset. This value of " CPL_FRMT_GUIB
                        " will be used for all bands "
                        "on re-opening",
                        static_cast<GUIntBig>(nNoData), nBand, nOtherBand,
                        static_cast<GUIntBig>(nOtherNoData),
                        static_cast<GUIntBig>(nNoData));
        }
    }

    if (m_poGDS->m_bStreamingOut && m_poGDS->m_bCrystalized)
    {
        ReportError(
            CE_Failure, CPLE_NotSupported,
            "Cannot modify nodata at that point in a streamed output file");
        return CE_Failure;
    }

    CPLErr eErr = CE_None;
    if (eAccess == GA_Update)
    {
        m_poGDS->m_bNoDataChanged = true;
        int bSuccess = FALSE;
        CPL_IGNORE_RET_VAL(
            GDALPamRasterBand::GetNoDataValueAsUInt64(&bSuccess));
        if (bSuccess)
        {
            // Cancel any existing nodata from PAM file.
            eErr = GDALPamRasterBand::DeleteNoDataValue();
        }
    }
    else
    {
        CPLDebug("GTIFF", "SetNoDataValue() goes to PAM instead of TIFF tags");
        eErr = GDALPamRasterBand::SetNoDataValueAsUInt64(nNoData);
    }

    if (eErr == CE_None)
    {
        ResetNoDataValues(true);

        m_poGDS->m_bNoDataSetAsUInt64 = true;
        m_poGDS->m_nNoDataValueUInt64 = nNoData;

        m_bNoDataSetAsUInt64 = true;
        m_nNoDataValueUInt64 = nNoData;
    }

    return eErr;
}

/************************************************************************/
/*                        ResetNoDataValues()                           */
/************************************************************************/

void GTiffRasterBand::ResetNoDataValues(bool bResetDatasetToo)
{
    if (bResetDatasetToo)
    {
        m_poGDS->m_bNoDataSet = false;
        m_poGDS->m_dfNoDataValue = DEFAULT_NODATA_VALUE;
    }

    m_bNoDataSet = false;
    m_dfNoDataValue = DEFAULT_NODATA_VALUE;

    if (bResetDatasetToo)
    {
        m_poGDS->m_bNoDataSetAsInt64 = false;
        m_poGDS->m_nNoDataValueInt64 = GDAL_PAM_DEFAULT_NODATA_VALUE_INT64;
    }

    m_bNoDataSetAsInt64 = false;
    m_nNoDataValueInt64 = GDAL_PAM_DEFAULT_NODATA_VALUE_INT64;

    if (bResetDatasetToo)
    {
        m_poGDS->m_bNoDataSetAsUInt64 = false;
        m_poGDS->m_nNoDataValueUInt64 = GDAL_PAM_DEFAULT_NODATA_VALUE_UINT64;
    }

    m_bNoDataSetAsUInt64 = false;
    m_nNoDataValueUInt64 = GDAL_PAM_DEFAULT_NODATA_VALUE_UINT64;
}

/************************************************************************/
/*                        DeleteNoDataValue()                           */
/************************************************************************/

CPLErr GTiffRasterBand::DeleteNoDataValue()

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    if (m_poGDS->m_bStreamingOut && m_poGDS->m_bCrystalized)
    {
        ReportError(
            CE_Failure, CPLE_NotSupported,
            "Cannot modify nodata at that point in a streamed output file");
        return CE_Failure;
    }

    if (eAccess == GA_Update)
    {
        if (m_poGDS->m_bNoDataSet)
            m_poGDS->m_bNoDataChanged = true;
    }
    else
    {
        CPLDebug("GTIFF",
                 "DeleteNoDataValue() goes to PAM instead of TIFF tags");
    }

    CPLErr eErr = GDALPamRasterBand::DeleteNoDataValue();
    if (eErr == CE_None)
    {
        ResetNoDataValues(true);
    }

    return eErr;
}

/************************************************************************/
/*                             NullBlock()                              */
/*                                                                      */
/*      Set the block data to the null value if it is set, or zero      */
/*      if there is no null data value.                                 */
/************************************************************************/

void GTiffRasterBand::NullBlock(void *pData)

{
    const GPtrDiff_t nWords =
        static_cast<GPtrDiff_t>(nBlockXSize) * nBlockYSize;
    const int nChunkSize = std::max(1, GDALGetDataTypeSizeBytes(eDataType));

    int l_bNoDataSet = FALSE;
    if (eDataType == GDT_Int64)
    {
        const auto nVal = GetNoDataValueAsInt64(&l_bNoDataSet);
        if (!l_bNoDataSet)
        {
            memset(pData, 0, nWords * nChunkSize);
        }
        else
        {
            GDALCopyWords64(&nVal, GDT_Int64, 0, pData, eDataType, nChunkSize,
                            nWords);
        }
    }
    else if (eDataType == GDT_UInt64)
    {
        const auto nVal = GetNoDataValueAsUInt64(&l_bNoDataSet);
        if (!l_bNoDataSet)
        {
            memset(pData, 0, nWords * nChunkSize);
        }
        else
        {
            GDALCopyWords64(&nVal, GDT_UInt64, 0, pData, eDataType, nChunkSize,
                            nWords);
        }
    }
    else
    {
        double dfNoData = GetNoDataValue(&l_bNoDataSet);
        if (!l_bNoDataSet)
        {
#ifdef ESRI_BUILD
            if (m_poGDS->m_nBitsPerSample >= 2)
                memset(pData, 0, nWords * nChunkSize);
            else
                memset(pData, 1, nWords * nChunkSize);
#else
            memset(pData, 0, nWords * nChunkSize);
#endif
        }
        else
        {
            // Will convert nodata value to the right type and copy efficiently.
            GDALCopyWords64(&dfNoData, GDT_Float64, 0, pData, eDataType,
                            nChunkSize, nWords);
        }
    }
}

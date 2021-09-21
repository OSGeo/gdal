/******************************************************************************
 *
 * Project:  TGA read-only driver
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2020, Even Rouault <even.rouault at spatialys.com>
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

#include "gdal_pam.h"

#include <algorithm>
#include <vector>

extern "C" void CPL_DLL GDALRegister_TGA();

enum ImageType
{
    UNCOMPRESSED_COLORMAP = 1,
    UNCOMPRESSED_TRUE_COLOR = 2,
    UNCOMPRESSED_GRAYSCALE = 3,
    RLE_COLORMAP = 9,
    RLE_TRUE_COLOR = 10,
    RLE_GRAYSCALE = 11,
};

struct ImageHeader
{
    GByte     nIDLength;
    bool      bHasColorMap;
    ImageType eImageType;
    GUInt16   nColorMapFirstIdx;
    GUInt16   nColorMapLength;
    GByte     nColorMapEntrySize;
    GUInt16   nXOrigin;
    GUInt16   nYOrigin;
    GByte     nPixelDepth;
    GByte     nImageDescriptor;
};

/************************************************************************/
/*                         GDALTGADataset                               */
/************************************************************************/

class GDALTGADataset final: public GDALPamDataset
{
        friend class GDALTGARasterBand;

        ImageHeader m_sImageHeader;
        VSILFILE   *m_fpImage;
        unsigned    m_nImageDataOffset = 0;
        std::vector<vsi_l_offset> m_anScanlineOffsets{};
        int         m_nLastLineKnownOffset = 0;
        bool        m_bFourthChannelIsAlpha = false;

    public:
        GDALTGADataset(const ImageHeader& sHeader, VSILFILE* fpImage);
        ~GDALTGADataset() override;

        static int Identify(GDALOpenInfo* poOpenInfo);
        static GDALDataset* Open(GDALOpenInfo* poOpenInfo);
};

/************************************************************************/
/*                        GDALTGARasterBand                             */
/************************************************************************/

class GDALTGARasterBand final: public GDALPamRasterBand
{
        std::unique_ptr<GDALColorTable> m_poColorTable{};
        bool                            m_bHasNoDataValue = false;
        double                          m_dfNoDataValue = 0;

    public:
        GDALTGARasterBand(GDALTGADataset* poDSIn, int nBandIn,
                          GDALDataType eDataTypeIn);

        CPLErr IReadBlock(int nBlockXOff, int nBlockYOff, void* pImage) override;

        GDALColorTable* GetColorTable() override { return m_poColorTable.get(); }

        GDALColorInterp GetColorInterpretation() override
        {
            if( m_poColorTable )
                return GCI_PaletteIndex;
            GDALTGADataset* poGDS = reinterpret_cast<GDALTGADataset*>(poDS);
            if( poGDS->GetRasterCount() == 1 )
                return GCI_GrayIndex;
            if( nBand == 4 )
                return poGDS->m_bFourthChannelIsAlpha ? GCI_AlphaBand : GCI_Undefined;
            return static_cast<GDALColorInterp>(GCI_RedBand + nBand - 1);
        }

        double GetNoDataValue(int* pbHasNoData) override
        {
            if( pbHasNoData )
                *pbHasNoData = m_bHasNoDataValue;
            return m_dfNoDataValue;
        }
};

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

int GDALTGADataset::Identify(GDALOpenInfo* poOpenInfo)
{
    if( poOpenInfo->fpL == nullptr || poOpenInfo->nHeaderBytes < 18 )
        return FALSE;
    const GByte nColorType = poOpenInfo->pabyHeader[1];
    if( nColorType > 1 )
        return FALSE;
    const GByte nImageType = poOpenInfo->pabyHeader[2];
    if( nImageType != UNCOMPRESSED_COLORMAP &&
        nImageType != UNCOMPRESSED_TRUE_COLOR &&
        nImageType != UNCOMPRESSED_GRAYSCALE &&
        nImageType != RLE_COLORMAP &&
        nImageType != RLE_TRUE_COLOR &&
        nImageType != RLE_GRAYSCALE )
        return FALSE;
    if( nImageType == UNCOMPRESSED_COLORMAP || nImageType == RLE_COLORMAP )
    {
        if( nColorType != 1 )
            return FALSE;
    }
    else
    {
        if( nColorType != 0 )
            return FALSE;
    }

    // Mostly useful for fuzzing purposes to be able to fuzz TGA on small files
    // without relying on the tga extension
    if( poOpenInfo->nHeaderBytes > 26 &&
        memcmp(poOpenInfo->pabyHeader + poOpenInfo->nHeaderBytes - 26,
               "TRUEVISION-XFILE.\x00", 18) == 0 )
    {
        return TRUE;
    }

    if( !EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "tga") )
        return FALSE;
    return TRUE;
}

/************************************************************************/
/*                           GDALTGADataset()                           */
/************************************************************************/

GDALTGADataset::GDALTGADataset(const ImageHeader& sHeader, VSILFILE* fpImage):
            m_sImageHeader(sHeader), m_fpImage(fpImage)
{
    m_nImageDataOffset = 18 + m_sImageHeader.nIDLength;
    if( m_sImageHeader.bHasColorMap )
    {
        m_nImageDataOffset += m_sImageHeader.nColorMapLength *
            ((m_sImageHeader.nColorMapEntrySize + 7) / 8);
    }
}

/************************************************************************/
/*                          ~GDALTGADataset()                           */
/************************************************************************/

GDALTGADataset::~GDALTGADataset()
{
    if( m_fpImage )
        VSIFCloseL(m_fpImage);
}

/************************************************************************/
/*                         GDALTGARasterBand()                          */
/************************************************************************/

GDALTGARasterBand::GDALTGARasterBand(GDALTGADataset* poDSIn, int nBandIn,
                                     GDALDataType eDataTypeIn)
{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType = eDataTypeIn;
    nBlockXSize = poDSIn->GetRasterXSize();
    nBlockYSize = 1;
    if( poDSIn->m_sImageHeader.bHasColorMap )
    {
        VSIFSeekL(poDSIn->m_fpImage, 18 + poDSIn->m_sImageHeader.nIDLength, SEEK_SET);
        m_poColorTable.reset(new GDALColorTable());
        const int nColorTableByteCount = poDSIn->m_sImageHeader.nColorMapLength *
            ((poDSIn->m_sImageHeader.nColorMapEntrySize + 7) / 8);
        std::vector<GByte> abyData(nColorTableByteCount);
        VSIFReadL(&abyData[0], 1, abyData.size(), poDSIn->m_fpImage);
        if( poDSIn->m_sImageHeader.nColorMapEntrySize == 24 )
        {
            for( unsigned i = 0; i < poDSIn->m_sImageHeader.nColorMapLength; ++i )
            {
                GDALColorEntry sEntry;
                sEntry.c1 = abyData[3 * i + 2];
                sEntry.c2 = abyData[3 * i + 1];
                sEntry.c3 = abyData[3 * i + 0];
                sEntry.c4 = 255;
                m_poColorTable->SetColorEntry(
                    poDSIn->m_sImageHeader.nColorMapFirstIdx + i, &sEntry);
            }
        }
        else if( poDSIn->m_sImageHeader.nColorMapEntrySize == 32 )
        {
            unsigned nCountAlpha0 = 0;
            unsigned nAlphaIdx = 0;
            for( unsigned i = 0; i < poDSIn->m_sImageHeader.nColorMapLength; ++i )
            {
                GDALColorEntry sEntry;
                sEntry.c1 = abyData[4 * i + 2];
                sEntry.c2 = abyData[4 * i + 1];
                sEntry.c3 = abyData[4 * i + 0];
                sEntry.c4 = abyData[4 * i + 3];
                m_poColorTable->SetColorEntry(
                    poDSIn->m_sImageHeader.nColorMapFirstIdx + i, &sEntry);
                if( sEntry.c4 == 0 )
                {
                    nCountAlpha0 ++;
                    nAlphaIdx = poDSIn->m_sImageHeader.nColorMapFirstIdx + i;
                }
            }
            if( nCountAlpha0 == 1 )
            {
                m_dfNoDataValue = nAlphaIdx;
                m_bHasNoDataValue = true;
            }
        }
        else if( poDSIn->m_sImageHeader.nColorMapEntrySize == 15 ||
                 poDSIn->m_sImageHeader.nColorMapEntrySize == 16 )
        {
            for( unsigned i = 0; i < poDSIn->m_sImageHeader.nColorMapLength; ++i )
            {
                GUInt16 nVal = (abyData[2 * i + 1] << 8) | abyData[2 * i];
                GDALColorEntry sEntry;
                sEntry.c1 = ((nVal >> 10) & 31) << 3;
                sEntry.c2 = ((nVal >> 5) & 31) << 3;
                sEntry.c3 = ((nVal >> 0) & 31) << 3;
                sEntry.c4 = 255;
                m_poColorTable->SetColorEntry(
                    poDSIn->m_sImageHeader.nColorMapFirstIdx + i, &sEntry);
            }
        }
    }
}

/************************************************************************/
/*                            IReadBlock()                              */
/************************************************************************/

CPLErr GDALTGARasterBand::IReadBlock(int /* nBlockXOff */, int nBlockYOff, void* pImage)
{
    GDALTGADataset* poGDS = reinterpret_cast<GDALTGADataset*>(poDS);

    const int nBands = poGDS->GetRasterCount();
    const int nLine = (poGDS->m_sImageHeader.nImageDescriptor & (1 << 5)) ?
                            nBlockYOff : nRasterYSize - 1 - nBlockYOff;
    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
    if( !poGDS->m_anScanlineOffsets.empty() ) // RLE
    {
        if( poGDS->m_anScanlineOffsets[nLine] == 0 )
        {
            for( int i = poGDS->m_nLastLineKnownOffset; i < nLine; i++ )
            {
                if( IReadBlock(0,
                           (poGDS->m_sImageHeader.nImageDescriptor & (1 << 5)) ?
                                i : nRasterYSize - 1 - i,
                           nullptr) != CE_None )
                {
                    return CE_Failure;
                }
            }
        }
        VSIFSeekL(poGDS->m_fpImage, poGDS->m_anScanlineOffsets[nLine], SEEK_SET);
        int x = 0;
        std::vector<GByte> abyData;
        const int nBytesPerPixel =
            (nBands == 1) ? nDTSize : (nBands == 4) ? 4 : poGDS->m_sImageHeader.nPixelDepth / 8;
        while( x < nRasterXSize )
        {
            GByte nRepeatCount = 0;
            VSIFReadL(&nRepeatCount, 1, 1, poGDS->m_fpImage);
            const int nPixelsToFill = std::min(nRasterXSize - x,
                                               (nRepeatCount & 0x7f) + 1);
            if( nRepeatCount & 0x80 )
            {
                if( pImage == nullptr )
                {
                    VSIFSeekL(poGDS->m_fpImage, nBytesPerPixel, SEEK_CUR);
                }
                else
                {
                    if( nBands == 1 )
                    {
                        VSIFReadL(static_cast<GByte*>(pImage) + x * nDTSize,
                                  1,
                                  nDTSize,
                                  poGDS->m_fpImage);
                        if( nPixelsToFill > 1 )
                        {
                            GDALCopyWords(static_cast<GByte*>(pImage) + x * nDTSize,
                                        eDataType,
                                        0,
                                        static_cast<GByte*>(pImage) + (x+1) * nDTSize,
                                        eDataType,
                                        nDTSize,
                                        nPixelsToFill - 1);
                        }
                    }
                    else
                    {
                        abyData.resize(4);
                        VSIFReadL(&abyData[0], 1, nBytesPerPixel, poGDS->m_fpImage);
                        if( poGDS->m_sImageHeader.nPixelDepth == 16 )
                        {
                            const GUInt16 nValue = abyData[0] | (abyData[1] << 8);
                            const GByte nByteVal = ((nValue >> (5 * (3 - nBand))) & 31) << 3;
                            memset(static_cast<GByte*>(pImage) + x, nByteVal, nPixelsToFill);
                        }
                        else
                        {
                            memset(static_cast<GByte*>(pImage) + x,
                                   abyData[nBand <= 3 ? 3 - nBand: 3],
                                   nPixelsToFill);
                        }
                    }
                }
            }
            else
            {
                if( pImage == nullptr )
                {
                    VSIFSeekL(poGDS->m_fpImage, nPixelsToFill * nBytesPerPixel, SEEK_CUR);
                }
                else
                {
                    if( nBands == 1 )
                    {
                        VSIFReadL(static_cast<GByte*>(pImage) + x * nDTSize,
                                  1,
                                  nPixelsToFill * nDTSize,
                                  poGDS->m_fpImage);
                    }
                    else
                    {
                        abyData.resize( nBytesPerPixel * nPixelsToFill );
                        VSIFReadL(&abyData[0], 1, abyData.size(), poGDS->m_fpImage);
                        if( poGDS->m_sImageHeader.nPixelDepth == 16 )
                        {
                            for(int i = 0; i < nPixelsToFill; i++ )
                            {
                                const GUInt16 nValue = abyData[2 * i] | (abyData[2 * i + 1] << 8);
                                static_cast<GByte*>(pImage)[x + i] =
                                    ((nValue >> (5 * (3 - nBand))) & 31) << 3;
                            }
                        }
                        else
                        {
                            if( nBand <= 3 )
                            {
                                for(int i = 0; i < nPixelsToFill; i++ )
                                {
                                    static_cast<GByte*>(pImage)[x + i] =
                                        abyData[3 - nBand + nBytesPerPixel * i];
                                }
                            }
                            else
                            {
                                for(int i = 0; i < nPixelsToFill; i++ )
                                {
                                    static_cast<GByte*>(pImage)[x + i] =
                                        abyData[3 + nBytesPerPixel * i];
                                }
                            }
                        }
                    }
                }
            }
            x += nPixelsToFill;
        }
        if( x != nRasterXSize )
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "RLE packet does not terminate on scan line boundary");
            return CE_Failure;
        }
        if( nLine + 1 < nRasterYSize )
        {
            poGDS->m_anScanlineOffsets[nLine+1] = VSIFTellL(poGDS->m_fpImage);
        }
        if( pImage && nBands == 1 )
        {
#ifdef CPL_MSB
            if( nDTSize > 1 )
            {
                GDALSwapWords(pImage, nDTSize, nRasterXSize, nDTSize);
            }
#endif
        }
        return CE_None;
    }

    if( pImage == nullptr )
        return CE_Failure;

    if( nBands == 1 )
    {
        vsi_l_offset nOffset = poGDS->m_nImageDataOffset +
            static_cast<vsi_l_offset>(nLine) * nRasterXSize * nDTSize;
        VSIFSeekL(poGDS->m_fpImage, nOffset, SEEK_SET);
        VSIFReadL(pImage, 1, nRasterXSize * nDTSize, poGDS->m_fpImage);
#ifdef CPL_MSB
        if( nDTSize > 1 )
        {
            GDALSwapWords(pImage, nDTSize, nRasterXSize, nDTSize);
        }
#endif
    }
    else
    {
        const int nBytesPerPixel = (nBands == 4) ? 4 : poGDS->m_sImageHeader.nPixelDepth / 8;
        std::vector<GByte> abyData;
        abyData.resize( nBytesPerPixel * nRasterXSize );
        vsi_l_offset nOffset = poGDS->m_nImageDataOffset +
            static_cast<vsi_l_offset>(nLine) * nRasterXSize * nBytesPerPixel;
        VSIFSeekL(poGDS->m_fpImage, nOffset, SEEK_SET);
        VSIFReadL(&abyData[0], 1, nRasterXSize * nBytesPerPixel, poGDS->m_fpImage);
        if( poGDS->m_sImageHeader.nPixelDepth == 16 )
        {
            for(int i = 0; i < nRasterXSize; i++ )
            {
                const GUInt16 nValue = abyData[2 * i] | (abyData[2 * i + 1] << 8);
                static_cast<GByte*>(pImage)[i] = ((nValue >> (5 * (3 - nBand))) & 31) << 3;
            }
        }
        else
        {
            if( nBand <= 3 )
            {
                for(int i = 0; i < nRasterXSize; i++ )
                {
                    static_cast<GByte*>(pImage)[i] = abyData[3 - nBand + nBytesPerPixel * i];
                }
            }
            else
            {
                for(int i = 0; i < nRasterXSize; i++ )
                {
                    static_cast<GByte*>(pImage)[i] = abyData[3 + nBytesPerPixel * i];
                }
            }
        }
    }

    return CE_None;
}

/************************************************************************/
/*                              Open()                                  */
/************************************************************************/

GDALDataset* GDALTGADataset::Open(GDALOpenInfo* poOpenInfo)
{
    if( !Identify(poOpenInfo) )
        return nullptr;
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Update of existing TGA file not supported");
        return nullptr;
    }

    ImageHeader sHeader;
    sHeader.nIDLength = poOpenInfo->pabyHeader[0];
    sHeader.bHasColorMap = poOpenInfo->pabyHeader[1] == 1;
    sHeader.eImageType = static_cast<ImageType>(poOpenInfo->pabyHeader[2]);
    sHeader.nColorMapFirstIdx = CPL_LSBUINT16PTR(poOpenInfo->pabyHeader + 3);
    sHeader.nColorMapLength = CPL_LSBUINT16PTR(poOpenInfo->pabyHeader + 5);
    sHeader.nColorMapEntrySize = poOpenInfo->pabyHeader[7];
    sHeader.nXOrigin = CPL_LSBUINT16PTR(poOpenInfo->pabyHeader + 8);
    sHeader.nYOrigin = CPL_LSBUINT16PTR(poOpenInfo->pabyHeader + 10);
    const GUInt16 nWidth = CPL_LSBUINT16PTR(poOpenInfo->pabyHeader + 12);
    const GUInt16 nHeight = CPL_LSBUINT16PTR(poOpenInfo->pabyHeader + 14);
    if( nWidth == 0 || nHeight == 0 )
        return nullptr;
    sHeader.nPixelDepth = poOpenInfo->pabyHeader[16];
    sHeader.nImageDescriptor = poOpenInfo->pabyHeader[17];

    if( sHeader.bHasColorMap )
    {
        if( sHeader.nColorMapEntrySize != 15 &&
            sHeader.nColorMapEntrySize != 16 &&
            sHeader.nColorMapEntrySize != 24 &&
            sHeader.nColorMapEntrySize != 32 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Color map entry size %d not supported",
                     sHeader.nColorMapEntrySize);
            return nullptr;
        }
    }

    GDALTGADataset* poDS = new GDALTGADataset(sHeader, poOpenInfo->fpL);

    VSIFSeekL(poOpenInfo->fpL, 0, SEEK_END);
    const auto nSize = VSIFTellL(poOpenInfo->fpL);

    bool hasFourthChannel = (sHeader.nImageDescriptor & 15) == 8;
    bool fourthChannelIsAlpha = hasFourthChannel;

    // Detect presence of optional TGA file footer.
    if( nSize >= 26 )
    {
        VSIFSeekL(poOpenInfo->fpL, nSize - 26, SEEK_SET);
        GByte abyTail[26];
        VSIFReadL(abyTail, 1, 26, poOpenInfo->fpL);
        if( memcmp(abyTail + 8, "TRUEVISION-XFILE.\x00", 18) == 0 )
        {
            const unsigned nExtensionAreaOffset = CPL_LSBUINT32PTR(abyTail);
            if( nExtensionAreaOffset > 0 )
            {
                VSIFSeekL(poOpenInfo->fpL, nExtensionAreaOffset, SEEK_SET);
                std::vector<GByte> abyExtendedData(495);
                VSIFReadL(&abyExtendedData[0], 1, abyExtendedData.size(),
                          poOpenInfo->fpL);
                const GUInt16 nExtSize = CPL_LSBUINT16PTR(&abyExtendedData[0]);
                if( nExtSize >= 495 )
                {
                    if( abyExtendedData[2] != ' ' && abyExtendedData[2] != '\0' )
                    {
                        std::string osAuthorName;
                        osAuthorName.assign(
                            reinterpret_cast<const char*>(&abyExtendedData[2]), 40);
                        osAuthorName.resize(strlen(osAuthorName.c_str()));
                        while( !osAuthorName.empty() && osAuthorName.back() == ' ' )
                        {
                            osAuthorName.resize(osAuthorName.size()-1);
                        }
                        poDS->GDALDataset::SetMetadataItem("AUTHOR_NAME", osAuthorName.c_str());
                    }

                    if( abyExtendedData[43] != ' ' && abyExtendedData[43] != '\0' )
                    {
                        std::string osComments;
                        for( int i = 0; i < 4; i++ )
                        {
                            if( abyExtendedData[43 + 81 * i] == '\0' )
                            {
                                break;
                            }
                            std::string osLine;
                            osLine.assign(reinterpret_cast<const char*>(
                                &abyExtendedData[43 + 81 * i]), 80);
                            osLine.resize(strlen(osLine.c_str()));
                            while( !osLine.empty() && osLine.back() == ' ' )
                            {
                                osLine.resize(osLine.size()-1);
                            }
                            if( i > 0 )
                                osComments += '\n';
                            osComments += osLine;
                        }
                        poDS->GDALDataset::SetMetadataItem("COMMENTS", osComments.c_str());
                    }

                    // const GUInt32 nOffsetToScanlineTable = CPL_LSBUINT32PTR(&abyExtendedData[490]);
                    // Did not find yet an image using a scanline table

                    const GByte nAttributeType = abyExtendedData[494];
                    if( nAttributeType == 1 )
                    {
                        // undefined data in the Alpha field, can be ignored
                        hasFourthChannel = false;
                    }
                    else if( nAttributeType == 2 )
                    {
                        // undefined data in the Alpha field, but should be retained
                        fourthChannelIsAlpha = false;
                    }
                }
            }
        }
    }

    if( sHeader.nIDLength > 0 && 18 + sHeader.nIDLength <= poOpenInfo->nHeaderBytes )
    {
        std::string osID;
        osID.assign( reinterpret_cast<const char*>(poOpenInfo->pabyHeader + 18),
                     sHeader.nIDLength );
        poDS->GDALDataset::SetMetadataItem("IMAGE_ID", osID.c_str());
    }

    poOpenInfo->fpL = nullptr;
    poDS->nRasterXSize = nWidth;
    poDS->nRasterYSize = nHeight;
    poDS->m_bFourthChannelIsAlpha = fourthChannelIsAlpha;
    if( sHeader.eImageType == RLE_COLORMAP ||
        sHeader.eImageType == RLE_GRAYSCALE ||
        sHeader.eImageType == RLE_TRUE_COLOR )
    {
        // nHeight is a GUInt16, so well bounded...
        // coverity[tainted_data]
        poDS->m_anScanlineOffsets.resize(nHeight);
        poDS->m_anScanlineOffsets[0] = poDS->m_nImageDataOffset;
    }
    if( sHeader.eImageType == UNCOMPRESSED_COLORMAP ||
        sHeader.eImageType == RLE_COLORMAP ||
        sHeader.eImageType == UNCOMPRESSED_GRAYSCALE ||
        sHeader.eImageType == RLE_GRAYSCALE )
    {
        if( sHeader.nPixelDepth != 8 &&
            sHeader.nPixelDepth != 16 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Pixel depth %d not supported", sHeader.nPixelDepth);
            delete poDS;
            return nullptr;
        }
        poDS->SetBand(1, new GDALTGARasterBand(poDS, 1,
            sHeader.nPixelDepth == 16 ? GDT_UInt16 : GDT_Byte));
    }
    else
    {
        if( sHeader.nPixelDepth != 16 &&
            sHeader.nPixelDepth != 24 &&
            sHeader.nPixelDepth != 32 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Pixel depth %d not supported", sHeader.nPixelDepth);
            delete poDS;
            return nullptr;
        }
        int l_nBands = sHeader.nPixelDepth == 16 ? 3 : (3 + (hasFourthChannel ? 1 : 0));
        for( int iBand = 1; iBand <= l_nBands; iBand++ )
        {
            poDS->SetBand(iBand, new GDALTGARasterBand(poDS, iBand, GDT_Byte));
        }
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */

    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */

    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return poDS;
}
/************************************************************************/
/*                       GDALRegister_TGA()                             */
/************************************************************************/

void GDALRegister_TGA()

{
    if( GDALGetDriverByName("TGA") != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("TGA");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "TGA/TARGA Image File Format");
    poDriver->SetMetadataItem( GDAL_DMD_MIMETYPE, "image/x-tga" );
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/tga.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "tga");
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = GDALTGADataset::Open;
    poDriver->pfnIdentify = GDALTGADataset::Identify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}

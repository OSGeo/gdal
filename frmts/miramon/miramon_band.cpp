/******************************************************************************
 *
 * Project:  MiraMonRaster driver
 * Purpose:  Implements MMRBand class: This class manages the metadata of each
 *           band to be processed. It is useful for maintaining a list of bands
 *           and for determining the number of subdatasets that need to be
 *           generated.
 * Author:   Abel Pau
 * 
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#include <algorithm>

#include "miramon_rel.h"
#include "miramon_band.h"

#include "../miramon_common/mm_gdal_driver_structs.h"  // For SECTION_ATTRIBUTE_DATA

/************************************************************************/
/*                              MMRBand()                               */
/************************************************************************/
MMRBand::MMRBand(MMRRel &fRel, CPLString osBandSectionIn)
    : pfRel(&fRel), nWidth(0), nHeight(0), osBandSection(osBandSectionIn)

{
    // Getting band and band file name from metadata
    if (!pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, osBandSectionIn,
                                 KEY_NomFitxer, osRawBandFileName) ||
        osRawBandFileName.empty())
    {
        osBandFileName = pfRel->MMRGetFileNameFromRelName(pfRel->GetRELName());
        if (osBandFileName.empty())
        {
            nWidth = 0;
            nHeight = 0;
            CPLError(CE_Failure, CPLE_AssertionFailed,
                     "The REL file '%s' contains a documented \
                band with no explicit name. Section [%s] or [%s:%s].\n",
                     pfRel->GetRELNameChar(), SECTION_ATTRIBUTE_DATA,
                     SECTION_ATTRIBUTE_DATA, osBandSection.c_str());
            return;
        }
        osBandName = CPLGetBasenameSafe(osBandFileName);
        osRawBandFileName = osBandName;
    }
    else
    {
        osBandName = CPLGetBasenameSafe(osRawBandFileName);
        CPLString osAux =
            CPLGetPathSafe(static_cast<const char *>(pfRel->GetRELNameChar()));
        osBandFileName =
            CPLFormFilenameSafe(osAux.c_str(), osRawBandFileName.c_str(), "");
    }

    // There is a band file documented?
    if (osBandName.empty())
    {
        nWidth = 0;
        nHeight = 0;
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "The REL file '%s' contains a documented \
            band with no explicit name. Section [%s] or [%s:%s].\n",
                 pfRel->GetRELNameChar(), SECTION_ATTRIBUTE_DATA,
                 SECTION_ATTRIBUTE_DATA, osBandSection.c_str());
        return;
    }

    // Getting essential metadata documented at
    // https://www.miramon.cat/new_note/eng/notes/MiraMon_raster_file_format.pdf

    // Getting number of columns and rows
    if (UpdateColumnsNumberFromREL(osBandSection))
    {
        nWidth = 0;
        nHeight = 0;
        return;
    }

    if (UpdateRowsNumberFromREL(osBandSection))
    {
        nWidth = 0;
        nHeight = 0;
        return;
    }

    if (nWidth <= 0 || nHeight <= 0)
    {
        nWidth = 0;
        nHeight = 0;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "MMRBand::MMRBand : (nWidth <= 0 || nHeight <= 0)");
        return;
    }

    // Getting data type and compression.
    // If error, message given inside.
    if (UpdateDataTypeFromREL(osBandSection))
        return;

    // Let's see if there is RLE compression
    bIsCompressed =
        (((eMMDataType >= MMDataType::DATATYPE_AND_COMPR_BYTE_RLE) &&
          (eMMDataType <= MMDataType::DATATYPE_AND_COMPR_DOUBLE_RLE)) ||
         eMMDataType == MMDataType::DATATYPE_AND_COMPR_BIT);

    // Getting min and max values
    UpdateMinMaxValuesFromREL(osBandSection);

    // Getting min and max values for simbolization
    UpdateMinMaxVisuValuesFromREL(osBandSection);
    if (!bMinVisuSet)
    {
        if (bMinSet)
        {
            dfVisuMin = dfMin;
            bMinVisuSet = true;
        }
    }
    if (!bMaxVisuSet)
    {
        if (bMaxSet)
        {
            dfVisuMax = dfMax;
            bMaxVisuSet = true;
        }
    }

    // Getting the friendly description of the band
    UpdateFriendlyDescriptionFromREL(osBandSection);

    // Getting NoData value and definition
    UpdateNoDataValue(osBandSection);

    // Getting reference system and coordinates of the geographic bounding box
    UpdateReferenceSystemFromREL();

    // Getting the bounding box: coordinates in the terrain
    UpdateBoundingBoxFromREL(osBandSection);

    // MiraMon IMG files are efficient in going to an specified row.
    // So le'ts configurate the blocks as line blocks.
    nBlockXSize = nWidth;
    nBlockYSize = 1;
    nNRowsPerBlock = 1;

    // Can the binary file that contains all data for this band be opened?
    pfIMG = VSIFOpenL(osBandFileName, "rb");
    if (!pfIMG)
    {
        nWidth = 0;
        nHeight = 0;
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Failed to open MiraMon band file `%s' with access 'rb'.",
                 osBandFileName.c_str());
        return;
    }

    // We have a valid MMRBand.
    bIsValid = true;
}

/************************************************************************/
/*                              ~MMRBand()                              */
/************************************************************************/
MMRBand::~MMRBand()

{
    if (pfIMG != nullptr)
        CPL_IGNORE_RET_VAL(VSIFCloseL(pfIMG));
}

const CPLString MMRBand::GetRELFileName() const
{
    if (!pfRel)
        return "";
    return pfRel->GetRELName();
}

/************************************************************************/
/*                           GetRasterBlock()                           */
/************************************************************************/
CPLErr MMRBand::GetRasterBlock(int /*nXBlock*/, int nYBlock, void *pData,
                               int nDataSize)

{
    const int iBlock = nYBlock * nNRowsPerBlock;
    const int nGDALBlockSize = nDataTypeSizeBytes * nBlockXSize * nBlockYSize;

    // Calculate block offset in case we have spill file. Use predefined
    // block map otherwise.
    if (!pfIMG)
    {
        CPLError(CE_Failure, CPLE_FileIO, "File band not opened: \n%s",
                 osBandFileName.c_str());
        return CE_Failure;
    }

    if (nDataSize != -1 && (nGDALBlockSize > INT_MAX ||
                            static_cast<int>(nGDALBlockSize) > nDataSize))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid block size: %d",
                 static_cast<int>(nGDALBlockSize));
        return CE_Failure;
    }

    // Getting the row offsets to optimize access.
    if (FillRowOffsets() == false || aFileOffsets.size() == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Some error in offsets calculation");
        return CE_Failure;
    }

    // Read the block in the documented or deduced offset
    if (VSIFSeekL(pfIMG, aFileOffsets[iBlock], SEEK_SET))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Read from invalid offset for grid block.");
        return CE_Failure;
    }

    size_t nCompressedRawSize;
    if (iBlock == nHeight - 1)
        nCompressedRawSize = SIZE_MAX;  // We don't know it
    else
        nCompressedRawSize =
            static_cast<int>(aFileOffsets[iBlock + 1] - aFileOffsets[iBlock]);

    return GetBlockData(pData, nCompressedRawSize);
}

int MMRBand::UpdateGeoTransform()
{
    m_gt[0] = GetBoundingBoxMinX();
    m_gt[1] = (GetBoundingBoxMaxX() - m_gt[0]) / GetWidth();
    m_gt[2] = 0.0;  // No rotation in MiraMon rasters
    m_gt[3] = GetBoundingBoxMaxY();
    m_gt[4] = 0.0;
    m_gt[5] = (GetBoundingBoxMinY() - m_gt[3]) / GetHeight();

    return 0;
}

/************************************************************************/
/*                      Other functions                                 */
/************************************************************************/

// [ATTRIBUTE_DATA:xxxx] or [OVERVIEW:ASPECTES_TECNICS]
int MMRBand::Get_ATTRIBUTE_DATA_or_OVERVIEW_ASPECTES_TECNICS_int(
    const CPLString osSection, const char *pszKey, int *nValue,
    const char *pszErrorMessage)
{
    if (osSection.empty() || !pszKey || !nValue)
        return 1;

    CPLString osValue;
    if (!pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, osSection, pszKey,
                                 osValue) ||
        osValue.empty())
    {
        if (pfRel->GetMetadataValue(SECTION_OVERVIEW, SECTION_ASPECTES_TECNICS,
                                    pszKey, osValue) == false ||
            osValue.empty())
        {
            if (pszErrorMessage)
                CPLError(CE_Failure, CPLE_AppDefined, "%s", pszErrorMessage);
            return 1;
        }
    }

    if (1 != sscanf(osValue, "%d", nValue))
    {
        if (pszErrorMessage)
            CPLError(CE_Failure, CPLE_AppDefined, "%s", pszErrorMessage);
        return 1;
    }
    return 0;
}

int MMRBand::GetDataTypeAndBytesPerPixel(const char *pszCompType,
                                         MMDataType *nCompressionType,
                                         MMBytesPerPixel *nBytesPerPixel)
{
    if (!nCompressionType || !nBytesPerPixel || !pszCompType)
        return 1;

    if (EQUAL(pszCompType, "bit"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_BIT;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_BYTE_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "byte"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_BYTE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_BYTE_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "byte-RLE"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_BYTE_RLE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_BYTE_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "integer"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_INTEGER;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_INTEGER_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "integer-RLE"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_INTEGER_RLE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_INTEGER_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "uinteger"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_UINTEGER;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_INTEGER_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "uinteger-RLE"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_UINTEGER_RLE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_INTEGER_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "long"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_LONG;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_LONG_REAL_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "long-RLE"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_LONG_RLE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_LONG_REAL_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "real"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_REAL;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_LONG_REAL_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "real-RLE"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_REAL_RLE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_LONG_REAL_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "double"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_DOUBLE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_DOUBLE_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "double-RLE"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_DOUBLE_RLE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_DOUBLE_I_RLE;
        return 0;
    }

    return 1;
}

// Getting data type from metadata
int MMRBand::UpdateDataTypeFromREL(const CPLString osSection)
{
    eMMDataType = MMDataType::DATATYPE_AND_COMPR_UNDEFINED;
    eMMBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_UNDEFINED;

    CPLString osValue;
    if (!pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, osSection,
                                 "TipusCompressio", osValue) ||
        osValue.empty())
    {
        nWidth = 0;
        nHeight = 0;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "MiraMonRaster: no nDataType documented");
        return 1;
    }

    if (GetDataTypeAndBytesPerPixel(osValue.c_str(), &eMMDataType,
                                    &eMMBytesPerPixel) == 1)
    {
        nWidth = 0;
        nHeight = 0;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "MiraMonRaster: data type unhandled");
        return 1;
    }

    nDataTypeSizeBytes = std::max(1, static_cast<int>(eMMBytesPerPixel));
    return 0;
}

// Getting number of columns from metadata
int MMRBand::UpdateColumnsNumberFromREL(const CPLString osSection)
{
    return Get_ATTRIBUTE_DATA_or_OVERVIEW_ASPECTES_TECNICS_int(
        osSection, "columns", &nWidth,
        "MMRBand::MMRBand : No number of columns documented");
}

int MMRBand::UpdateRowsNumberFromREL(const CPLString osSection)
{
    return Get_ATTRIBUTE_DATA_or_OVERVIEW_ASPECTES_TECNICS_int(
        osSection, "rows", &nHeight,
        "MMRBand::MMRBand : No number of rows documented");
}

// Getting nodata value from metadata
void MMRBand::UpdateNoDataValue(const CPLString osSection)
{
    CPLString osValue;
    if (!pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, osSection, "NODATA",
                                 osValue) ||
        osValue.empty())
    {
        dfNoData = 0;  // No a valid value.
        bNoDataSet = false;
    }
    else
    {
        dfNoData = CPLAtof(osValue);
        bNoDataSet = true;
    }
}

void MMRBand::UpdateMinMaxValuesFromREL(const CPLString osSection)
{
    bMinSet = false;

    CPLString osValue;

    CPLString osAuxSection = SECTION_ATTRIBUTE_DATA;
    osAuxSection.append(":");
    osAuxSection.append(osSection);
    if (pfRel->GetMetadataValue(osAuxSection, "min", osValue) &&
        !osValue.empty())
    {
        if (1 == CPLsscanf(osValue, "%lf", &dfMin))
            bMinSet = true;
    }

    bMaxSet = false;
    if (pfRel->GetMetadataValue(osAuxSection, "max", osValue) &&
        !osValue.empty())
    {
        if (1 == CPLsscanf(osValue, "%lf", &dfMax))
            bMaxSet = true;
    }

    // Special case: dfMin > dfMax
    if (bMinSet && bMaxSet && dfMin > dfMax)
    {
        bMinSet = false;
        bMaxSet = false;
    }
}

void MMRBand::UpdateMinMaxVisuValuesFromREL(const CPLString osSection)
{
    bMinVisuSet = false;
    dfVisuMin = 1;

    CPLString osValue;
    if (pfRel->GetMetadataValue(SECTION_COLOR_TEXT, osSection,
                                "Color_ValorColor_0", osValue) &&
        !osValue.empty())
    {
        if (1 == CPLsscanf(osValue, "%lf", &dfVisuMin))
            bMinVisuSet = true;
    }

    bMaxVisuSet = false;
    dfVisuMax = 1;

    if (pfRel->GetMetadataValue(SECTION_COLOR_TEXT, osSection,
                                "Color_ValorColor_n_1", osValue) &&
        !osValue.empty())
    {
        if (1 == CPLsscanf(osValue, "%lf", &dfVisuMax))
            bMaxVisuSet = true;
    }
}

void MMRBand::UpdateFriendlyDescriptionFromREL(const CPLString osSection)
{
    pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, osSection, "descriptor",
                            osFriendlyDescription);
}

void MMRBand::UpdateReferenceSystemFromREL()
{
    pfRel->GetMetadataValue("SPATIAL_REFERENCE_SYSTEM:HORIZONTAL",
                            "HorizontalSystemIdentifier", osRefSystem);
}

void MMRBand::UpdateBoundingBoxFromREL(const CPLString osSection)
{
    // Bounding box of the band
    // [ATTRIBUTE_DATA:xxxx:EXTENT] or [EXTENT]
    CPLString osValue;
    if (!pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, osSection,
                                 SECTION_EXTENT, "MinX", osValue) ||
        osValue.empty())
    {
        dfBBMinX = 0;
    }
    else
    {
        if (1 != CPLsscanf(osValue, "%lf", &dfBBMinX))
            dfBBMinX = 0;
    }

    if (!pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, osSection,
                                 SECTION_EXTENT, "MaxX", osValue) ||
        osValue.empty())
    {
        dfBBMaxX = nWidth;
    }
    else
    {
        if (1 != CPLsscanf(osValue, "%lf", &dfBBMaxX))
        {
            // If the value is something that cannot be scanned,
            // we silently continue as it was undefined.
            dfBBMaxX = nWidth;
        }
    }

    if (!pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, osSection,
                                 SECTION_EXTENT, "MinY", osValue) ||
        osValue.empty())
    {
        dfBBMinY = 0;
    }
    else
    {
        if (1 != CPLsscanf(osValue, "%lf", &dfBBMinY))
            dfBBMinY = 0;
    }

    if (!pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, osSection,
                                 SECTION_EXTENT, "MaxY", osValue) ||
        osValue.empty())
    {
        dfBBMaxY = nHeight;
    }
    else
    {
        if (1 != CPLsscanf(osValue, "%lf", &dfBBMaxY))
        {
            // If the value is something that cannot be scanned,
            // we silently continue as it was undefined.
            dfBBMaxY = nHeight;
        }
    }
}

/************************************************************************/
/*          Functions that read bytes from IMG file band                */
/************************************************************************/
template <typename TYPE>
CPLErr MMRBand::UncompressRow(void *rowBuffer, size_t nCompressedRawSize)
{
    int nAcumulated = 0L, nIAcumulated = 0L;
    unsigned char cCounter;
    size_t nCompressedIndex = 0;

    TYPE RLEValue;
    TYPE *pDst;
    size_t sizeof_TYPE = sizeof(TYPE);

    std::vector<unsigned char> aCompressedRow;

    if (nCompressedRawSize != SIZE_MAX)
    {
        aCompressedRow.resize(nCompressedRawSize);
        if (VSIFReadL(aCompressedRow.data(), nCompressedRawSize, 1, pfIMG) != 1)
            return CE_Failure;
    }

    while (nAcumulated < nWidth)
    {
        if (nCompressedRawSize == SIZE_MAX)
        {
            if (VSIFReadL(&cCounter, 1, 1, pfIMG) != 1)
                return CE_Failure;
        }
        else
        {
            cCounter = aCompressedRow[nCompressedIndex];
            nCompressedIndex++;
        }

        if (cCounter == 0) /* Not compressed part */
        {
            /* The following counter read does not indicate
            "how many repeated values follow" but rather
            "how many are decompressed in standard raster format" */
            if (nCompressedRawSize == SIZE_MAX)
            {
                if (VSIFReadL(&cCounter, 1, 1, pfIMG) != 1)
                    return CE_Failure;
            }
            else
            {
                cCounter = aCompressedRow[nCompressedIndex];
                nCompressedIndex++;
            }

            nAcumulated += cCounter;

            if (nAcumulated > nWidth) /* This should not happen if the file
                                  is RLE and does not share counters across rows */
                return CE_Failure;

            for (; nIAcumulated < nAcumulated; nIAcumulated++)
            {
                if (nCompressedRawSize == SIZE_MAX)
                {
                    VSIFReadL(&RLEValue, sizeof_TYPE, 1, pfIMG);
                    memcpy((static_cast<TYPE *>(rowBuffer)) + nIAcumulated,
                           &RLEValue, sizeof_TYPE);
                }
                else
                {
                    memcpy((static_cast<TYPE *>(rowBuffer)) + nIAcumulated,
                           &aCompressedRow[nCompressedIndex], sizeof_TYPE);
                    nCompressedIndex += sizeof_TYPE;
                }
            }
        }
        else
        {
            nAcumulated += cCounter;
            if (nAcumulated > nWidth) /* This should not happen if the file
                                  is RLE and does not share counters across rows */
                return CE_Failure;

            if (nCompressedRawSize == SIZE_MAX)
            {
                if (VSIFReadL(&RLEValue, sizeof_TYPE, 1, pfIMG) != 1)
                    return CE_Failure;
            }
            else
            {
                memcpy(&RLEValue, &aCompressedRow[nCompressedIndex],
                       sizeof(TYPE));
                nCompressedIndex += sizeof(TYPE);
            }

            const int nCount = nAcumulated - nIAcumulated;
            pDst = static_cast<TYPE *>(rowBuffer) + nIAcumulated;

            std::fill(pDst, pDst + nCount, RLEValue);

            nIAcumulated = nAcumulated;
        }
    }

    return CE_None;
}

CPLErr MMRBand::GetBlockData(void *rowBuffer, size_t nCompressedRawSize)
{
    if (eMMDataType == MMDataType::DATATYPE_AND_COMPR_BIT)
    {
        const int nGDALBlockSize = static_cast<int>(ceil(nBlockXSize / 8.0));

        if (VSIFReadL(rowBuffer, nGDALBlockSize, 1, pfIMG) != 1)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "\nError while reading band");
            return CE_Failure;
        }
        return CE_None;
    }

    if (eMMDataType == MMDataType::DATATYPE_AND_COMPR_BYTE ||
        eMMDataType == MMDataType::DATATYPE_AND_COMPR_INTEGER ||
        eMMDataType == MMDataType::DATATYPE_AND_COMPR_UINTEGER ||
        eMMDataType == MMDataType::DATATYPE_AND_COMPR_LONG ||
        eMMDataType == MMDataType::DATATYPE_AND_COMPR_REAL ||
        eMMDataType == MMDataType::DATATYPE_AND_COMPR_DOUBLE)
    {
        if (VSIFReadL(rowBuffer, nDataTypeSizeBytes, nWidth, pfIMG) !=
            static_cast<size_t>(nWidth))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "\nError while reading band");
            return CE_Failure;
        }
        return CE_None;
    }

    CPLErr peErr;
    switch (eMMDataType)
    {
        case MMDataType::DATATYPE_AND_COMPR_BYTE_RLE:
            peErr = UncompressRow<GByte>(rowBuffer, nCompressedRawSize);
            break;
        case MMDataType::DATATYPE_AND_COMPR_INTEGER_RLE:
            peErr = UncompressRow<GInt16>(rowBuffer, nCompressedRawSize);
            break;
        case MMDataType::DATATYPE_AND_COMPR_UINTEGER_RLE:
            peErr = UncompressRow<GUInt16>(rowBuffer, nCompressedRawSize);
            break;
        case MMDataType::DATATYPE_AND_COMPR_LONG_RLE:
            peErr = UncompressRow<GInt32>(rowBuffer, nCompressedRawSize);
            break;
        case MMDataType::DATATYPE_AND_COMPR_REAL_RLE:
            peErr = UncompressRow<float>(rowBuffer, nCompressedRawSize);
            break;
        case MMDataType::DATATYPE_AND_COMPR_DOUBLE_RLE:
            peErr = UncompressRow<double>(rowBuffer, nCompressedRawSize);
            break;

        default:
            CPLError(CE_Failure, CPLE_AppDefined, "\nError in datatype");
            peErr = CE_Failure;
    }

    return peErr;
}  // End of GetBlockData()

int MMRBand::PositionAtStartOfRowOffsetsInFile()
{
    vsi_l_offset nFileSize, nHeaderOffset;
    char szChain[16];
    short int nVersion, nSubVersion;
    int nOffsetSize, nOffsetsSectionType;

    if (VSIFSeekL(pfIMG, 0, SEEK_END))
        return 0;

    nFileSize = VSIFTellL(pfIMG);

    if (nFileSize < 32)  // Minimum required size
        return 0;

    if (nHeight)
    {
        if (nFileSize < static_cast<vsi_l_offset>(32) + nHeight + 32)
            return 0;
    }

    vsi_l_offset nHeadOffset = nFileSize - 32;

    if (VSIFSeekL(pfIMG, nHeadOffset, SEEK_SET))  // Reading final header.
        return 0;
    if (VSIFReadL(szChain, 16, 1, pfIMG) != 1)
        return 0;
    for (int nIndex = 0; nIndex < 16; nIndex++)
    {
        if (szChain[nIndex] != '\0')
            return 0;  // Supposed 0's are not 0.
    }

    if (VSIFReadL(szChain, 8, 1, pfIMG) != 1)
        return 0;

    if (strncmp(szChain, "IMG ", 4) || szChain[5] != '.')
        return 0;

    // Some version checks
    szChain[7] = 0;
    if (sscanf(szChain + 6, "%hd", &nSubVersion) != 1 || nSubVersion < 0)
        return 0;

    szChain[5] = 0;
    if (sscanf(szChain + 4, "%hd", &nVersion) != 1 || nVersion != 1)
        return 0;

    // Next header to be read
    if (VSIFReadL(&nHeaderOffset, sizeof(vsi_l_offset), 1, pfIMG) != 1)
        return 0;

    int bRepeat;
    do
    {
        bRepeat = FALSE;

        if (VSIFSeekL(pfIMG, nHeaderOffset, SEEK_SET))
            return 0;

        if (VSIFReadL(szChain, 8, 1, pfIMG) != 1)
            return 0;

        if (strncmp(szChain, "IMG ", 4) || szChain[5] != '.')
            return 0;

        if (VSIFReadL(&nOffsetsSectionType, 4, 1, pfIMG) != 1)
            return 0;

        if (nOffsetsSectionType != 2)  // 2 = row offsets section
        {
            // This is not the section I am looking for
            if (VSIFSeekL(pfIMG, 8 + 4, SEEK_CUR))
                return 0;

            if (VSIFReadL(&nHeaderOffset, sizeof(vsi_l_offset), 1, pfIMG) != 1)
                return 0;

            if (nHeaderOffset == 0)
                return 0;

            bRepeat = TRUE;
        }

    } while (bRepeat);

    szChain[7] = 0;
    if (sscanf(szChain + 6, "%hd", &nSubVersion) != 1 || nSubVersion < 0)
        return 0;
    szChain[5] = 0;
    if (sscanf(szChain + 4, "%hd", &nVersion) != 1 || nVersion != 1)
        return 0;

    /*
        Now I'm in the correct section
        -------------------------------
        Info about this section:
        RasterRLE: minumum size: nHeight*2
        Offsets:   minimum size: 32+nHeight*4
        Final:     size: 32
    */

    if (nHeight)
    {
        if (nHeaderOffset < static_cast<vsi_l_offset>(nHeight) *
                                2 ||  // Minumum size of an RLE
            nFileSize - nHeaderOffset <
                static_cast<vsi_l_offset>(32) + nHeight +
                    32)  // Minumum size of the section in version 1.0
            return 0;
    }

    if (VSIFReadL(&nOffsetSize, 4, 1, pfIMG) != 1 ||
        (nOffsetSize != 8 && nOffsetSize != 4 && nOffsetSize != 2 &&
         nOffsetSize != 1))
        return 0;

    if (nHeight)
    {
        if (nFileSize - nHeaderOffset <
            32 + static_cast<vsi_l_offset>(nOffsetSize) * nHeight +
                32)  // No space for this section in this file
            return 0;

        // I leave the file prepared to read offsets
        if (VSIFSeekL(pfIMG, 16, SEEK_CUR))
            return 0;
    }
    else
    {
        if (VSIFSeekL(pfIMG, 4, SEEK_CUR))
            return 0;

        if (VSIFSeekL(pfIMG, 4, SEEK_CUR))
            return 0;

        // I leave the file prepared to read offsets
        if (VSIFSeekL(pfIMG, 8, SEEK_CUR))
            return 0;
    }

    // There are offsets!
    return nOffsetSize;
}  // Fi de PositionAtStartOfRowOffsetsInFile()

/************************************************************************/
/*                              FillRowOffsets()                         */
/************************************************************************/
bool MMRBand::FillRowOffsets()
{
    vsi_l_offset nStartOffset;
    int nIRow;
    vsi_l_offset nBytesPerPixelPerNCol;
    int nSizeToRead;  // nSizeToRead is not an offset, but the size of the offsets being read
                      // directly from the IMG file (can be 1, 2, 4, or 8).
    vsi_l_offset nFileByte;
    size_t nMaxBytesPerCompressedRow;
    const int nGDALBlockSize = static_cast<int>(ceil(nBlockXSize / 8.0));
    ;

    // If it's filled, there is no need to fill it again
    if (aFileOffsets.size() > 0)
        return true;

    try
    {
        aFileOffsets.resize(static_cast<size_t>(nHeight) + 1);
    }
    catch (const std::bad_alloc &e)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
        return false;
    }

    switch (eMMDataType)
    {
        case MMDataType::DATATYPE_AND_COMPR_BIT:

            // "<=" it's ok. There is space and it's to make easier the programming
            for (nIRow = 0; nIRow <= nHeight; nIRow++)
                aFileOffsets[nIRow] =
                    static_cast<vsi_l_offset>(nIRow) * nGDALBlockSize;
            break;

        case MMDataType::DATATYPE_AND_COMPR_BYTE:
        case MMDataType::DATATYPE_AND_COMPR_INTEGER:
        case MMDataType::DATATYPE_AND_COMPR_UINTEGER:
        case MMDataType::DATATYPE_AND_COMPR_LONG:
        case MMDataType::DATATYPE_AND_COMPR_REAL:
        case MMDataType::DATATYPE_AND_COMPR_DOUBLE:
            nBytesPerPixelPerNCol =
                nDataTypeSizeBytes * static_cast<vsi_l_offset>(nWidth);
            // "<=" it's ok. There is space and it's to make easier the programming
            for (nIRow = 0; nIRow <= nHeight; nIRow++)
                aFileOffsets[nIRow] =
                    static_cast<vsi_l_offset>(nIRow) * nBytesPerPixelPerNCol;
            break;

        case MMDataType::DATATYPE_AND_COMPR_BYTE_RLE:
        case MMDataType::DATATYPE_AND_COMPR_INTEGER_RLE:
        case MMDataType::DATATYPE_AND_COMPR_UINTEGER_RLE:
        case MMDataType::DATATYPE_AND_COMPR_LONG_RLE:
        case MMDataType::DATATYPE_AND_COMPR_REAL_RLE:
        case MMDataType::DATATYPE_AND_COMPR_DOUBLE_RLE:

            nStartOffset = VSIFTellL(pfIMG);

            // Let's determine if are there offsets in the file
            if (0 < (nSizeToRead = PositionAtStartOfRowOffsetsInFile()))
            {
                // I have offsets!!
                nFileByte = 0L;  // all bits to 0
                for (nIRow = 0; nIRow < nHeight; nIRow++)
                {
                    if (VSIFReadL(&nFileByte, nSizeToRead, 1, pfIMG) != 1)
                        return false;

                    aFileOffsets[nIRow] = nFileByte;

                    // Let's check that the difference between two offsets is in a int range
                    if (nIRow > 0)
                    {
                        if (aFileOffsets[nIRow] <=
                            aFileOffsets[static_cast<size_t>(nIRow) - 1])
                            return false;

                        if (aFileOffsets[nIRow] -
                                aFileOffsets[static_cast<size_t>(nIRow) - 1] >=
                            static_cast<vsi_l_offset>(SIZE_MAX))
                            return false;
                    }
                }
                aFileOffsets[nIRow] = 0;  // Not reliable
                VSIFSeekL(pfIMG, nStartOffset, SEEK_SET);
                break;
            }

            // Not indexed RLE. We create a dynamic indexation
            nMaxBytesPerCompressedRow =
                static_cast<int>(eMMBytesPerPixel)
                    ? (nWidth * (static_cast<int>(eMMBytesPerPixel) + 1))
                    : (nWidth * (1 + 1));
            unsigned char *pBuffer;

            if (nullptr == (pBuffer = static_cast<unsigned char *>(
                                VSI_MALLOC_VERBOSE(nMaxBytesPerCompressedRow))))
            {
                VSIFSeekL(pfIMG, nStartOffset, SEEK_SET);
                return false;
            }

            VSIFSeekL(pfIMG, 0, SEEK_SET);
            aFileOffsets[0] = 0;
            for (nIRow = 0; nIRow < nHeight; nIRow++)
            {
                GetBlockData(pBuffer, SIZE_MAX);
                aFileOffsets[static_cast<size_t>(nIRow) + 1] = VSIFTellL(pfIMG);
            }
            VSIFree(pBuffer);
            VSIFSeekL(pfIMG, nStartOffset, SEEK_SET);
            break;

        default:
            return false;
    }  // End of switch (eMMDataType)
    return true;

}  // End of FillRowOffsets()

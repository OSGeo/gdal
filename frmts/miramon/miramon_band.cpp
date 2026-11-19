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
#include <limits>
#include "gdal_rat.h"

#include "miramon_rel.h"
#include "miramon_band.h"

#include "../miramon_common/mm_gdal_functions.h"  // For MM_CreateDBFHeader

/************************************************************************/
/*                              MMRBand()                               */
/************************************************************************/
MMRBand::MMRBand(MMRRel &fRel, const CPLString &osBandSectionIn)
    : m_pfRel(&fRel), m_nWidth(0), m_nHeight(0),
      m_osBandSection(osBandSectionIn)
{
    // Getting band and band file name from metadata.
    CPLString osNomFitxer;
    osNomFitxer = SECTION_ATTRIBUTE_DATA;
    osNomFitxer.append(":");
    osNomFitxer.append(osBandSectionIn);
    if (!m_pfRel->GetMetadataValue(osNomFitxer, KEY_NomFitxer,
                                   m_osRawBandFileName) ||
        m_osRawBandFileName.empty())
    {
        // A band name may be empty only if it is the only band present
        // in the REL file. Otherwise, inferring the band name from the
        // REL filename is considered an error.
        // Consequently, for a REL file containing exactly one band, if
        // the band name is empty, it shall be inferred from the REL
        // filename.
        // Example: REL: testI.rel  -->  IMG: test.img
        if (m_pfRel->GetNBands() >= 1)
            m_osBandFileName = "";
        else
        {
            m_osBandFileName = m_pfRel->MMRGetFileNameFromRelName(
                m_pfRel->GetRELName(), pszExtRaster);
        }

        if (m_osBandFileName.empty())
        {
            m_nWidth = 0;
            m_nHeight = 0;
            CPLError(CE_Failure, CPLE_AssertionFailed,
                     "The REL file '%s' contains a documented \
                band with no explicit or wrong name. Section [%s] or [%s:%s].",
                     m_pfRel->GetRELNameChar(), SECTION_ATTRIBUTE_DATA,
                     SECTION_ATTRIBUTE_DATA, m_osBandSection.c_str());
            return;
        }
        m_osBandName = CPLGetBasenameSafe(m_osBandFileName);
        m_osRawBandFileName = m_osBandName;
    }
    else
    {
        m_osBandName = CPLGetBasenameSafe(m_osRawBandFileName);
        CPLString osAux = CPLGetPathSafe(m_pfRel->GetRELNameChar());
        m_osBandFileName =
            CPLFormFilenameSafe(osAux.c_str(), m_osRawBandFileName.c_str(), "");

        CPLString osExtension =
            CPLString(CPLGetExtensionSafe(m_osBandFileName).c_str());
        if (!EQUAL(osExtension, pszExtRaster + 1))
            return;
    }

    // There is a band file documented?
    if (m_osBandName.empty())
    {
        m_nWidth = 0;
        m_nHeight = 0;
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "The REL file '%s' contains a documented \
            band with no explicit name. Section [%s] or [%s:%s].",
                 m_pfRel->GetRELNameChar(), SECTION_ATTRIBUTE_DATA,
                 SECTION_ATTRIBUTE_DATA, m_osBandSection.c_str());
        return;
    }

    // Getting essential metadata documented at
    // https://www.miramon.cat/new_note/eng/notes/MiraMon_raster_file_format.pdf

    // Getting number of columns and rows
    if (!UpdateColumnsNumberFromREL(m_osBandSection))
    {
        m_nWidth = 0;
        m_nHeight = 0;
        return;
    }

    if (!UpdateRowsNumberFromREL(m_osBandSection))
    {
        m_nWidth = 0;
        m_nHeight = 0;
        return;
    }

    if (m_nWidth <= 0 || m_nHeight <= 0)
    {
        m_nWidth = 0;
        m_nHeight = 0;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "MMRBand::MMRBand : (nWidth <= 0 || nHeight <= 0)");
        return;
    }

    // Getting data type and compression.
    // If error, message given inside.
    if (!UpdateDataTypeFromREL(m_osBandSection))
        return;

    // Let's see if there is RLE compression
    m_bIsCompressed =
        (((m_eMMDataType >= MMDataType::DATATYPE_AND_COMPR_BYTE_RLE) &&
          (m_eMMDataType <= MMDataType::DATATYPE_AND_COMPR_DOUBLE_RLE)) ||
         m_eMMDataType == MMDataType::DATATYPE_AND_COMPR_BIT);

    // Getting min and max values
    UpdateMinMaxValuesFromREL(m_osBandSection);

    // Getting unit type
    UpdateUnitTypeValueFromREL(m_osBandSection);

    // Getting min and max values for simbolization
    UpdateMinMaxVisuValuesFromREL(m_osBandSection);
    if (!m_bMinVisuSet)
    {
        if (m_bMinSet)
        {
            m_dfVisuMin = m_dfMin;
            m_bMinVisuSet = true;
        }
    }
    if (!m_bMaxVisuSet)
    {
        if (m_bMaxSet)
        {
            m_dfVisuMax = m_dfMax;
            m_bMaxVisuSet = true;
        }
    }

    // Getting the friendly description of the band
    UpdateFriendlyDescriptionFromREL(m_osBandSection);

    // Getting NoData value and definition
    UpdateNoDataValue(m_osBandSection);

    // Getting reference system and coordinates of the geographic bounding box
    UpdateReferenceSystemFromREL();

    // Getting the bounding box: coordinates in the terrain
    UpdateBoundingBoxFromREL(m_osBandSection);

    // Getting all information about simbolization
    UpdateSimbolizationInfo(m_osBandSection);

    // Getting all information about RAT
    UpdateRATInfo(m_osBandSection);

    // MiraMon IMG files are efficient in going to an specified row.
    // So let's configure the blocks as line blocks.
    m_nBlockXSize = m_nWidth;
    m_nBlockYSize = 1;
    m_nNRowsPerBlock = 1;

    // Can the binary file that contains all data for this band be opened?
    m_pfIMG = VSIFOpenL(m_osBandFileName, "rb");
    if (!m_pfIMG)
    {
        m_nWidth = 0;
        m_nHeight = 0;
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Failed to open MiraMon band file `%s' with access 'rb'.",
                 m_osBandFileName.c_str());
        return;
    }

    // We have a valid MMRBand.
    m_bIsValid = true;
}

MMRBand::MMRBand(GDALProgressFunc pfnProgress, void *pProgressData,
                 GDALDataset &oSrcDS, int nIBand, const CPLString &osDestPath,
                 GDALRasterBand &papoBand, bool bCompress, bool bCategorical,
                 const CPLString &osPattern, const CPLString &osBandSection,
                 bool bNeedOfNomFitxer)
    : m_pfnProgress(pfnProgress), m_pProgressData(pProgressData),
      m_nIBand(nIBand), m_osBandSection(osBandSection),
      m_osFriendlyDescription(papoBand.GetDescription()),
      m_bIsCompressed(bCompress), m_bIsCategorical(bCategorical)
{
    // Getting the binary filename
    if (bNeedOfNomFitxer)
        m_osBandName = osPattern + "_" + osBandSection;
    else
        m_osBandName = osPattern;

    m_osRawBandFileName = m_osBandName + pszExtRaster;
    m_osBandFileName =
        CPLFormFilenameSafe(osDestPath, m_osBandName, pszExtRaster);

    // Getting essential metadata documented at
    // https://www.miramon.cat/new_note/eng/notes/MiraMon_raster_file_format.pdf

    // Getting number of columns and rows
    m_nWidth = papoBand.GetXSize();
    m_nHeight = papoBand.GetYSize();

    if (m_nWidth <= 0 || m_nHeight <= 0)
    {
        m_nWidth = 0;
        m_nHeight = 0;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "MMRBand::MMRBand : (nWidth <= 0 || nHeight <= 0)");
        return;
    }

    // Getting units
    m_osBandUnitType = papoBand.GetUnitType();

    // Getting data type and compression from papoBand.
    // If error, message given inside.
    if (!UpdateDataTypeAndBytesPerPixelFromRasterBand(papoBand))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "MMRBand::MMRBand : DataType not supported");
        return;
    }
    m_nDataTypeSizeBytes = std::max(1, static_cast<int>(m_eMMBytesPerPixel));
    m_bIsCompressed = bCompress;

    // Getting NoData value and definition
    UpdateNoDataValueFromRasterBand(papoBand);

    if (WriteColorTable(oSrcDS))
    {
        m_osCTName.clear();
        CPLError(CE_Warning, CPLE_AppDefined,
                 "MMRBand::MMRBand : Existent color table but not imported"
                 "due to some existent errors");
    }

    if (WriteAttributeTable(oSrcDS))
    {
        m_osRATDBFName.clear();
        m_osRATRELName.clear();
        CPLError(CE_Warning, CPLE_AppDefined,
                 "MMRBand::MMRBand : Existent attribute table but not imported "
                 "due to some existent errors");
    }

    // We have a valid MMRBand.
    m_bIsValid = true;
}

/************************************************************************/
/*                              ~MMRBand()                              */
/************************************************************************/
MMRBand::~MMRBand()
{
    if (m_pfIMG == nullptr)
        return;

    CPL_IGNORE_RET_VAL(VSIFCloseL(m_pfIMG));
    m_pfIMG = nullptr;
}

const CPLString &MMRBand::GetRELFileName() const
{
    static const CPLString osEmpty;
    if (!m_pfRel)
        return osEmpty;
    return m_pfRel->GetRELName();
}

/************************************************************************/
/*                           GetRasterBlock()                           */
/************************************************************************/
CPLErr MMRBand::GetRasterBlock(int /*nXBlock*/, int nYBlock, void *pData,
                               int nDataSize)

{
    if (nYBlock > INT_MAX / (std::max(1, m_nNRowsPerBlock)))
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Error in GetRasterBlock");
        return CE_Failure;
    }
    const int iBlock = nYBlock * m_nNRowsPerBlock;

    if (m_nBlockXSize > INT_MAX / (std::max(1, m_nDataTypeSizeBytes)))
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Error in GetRasterBlock");
        return CE_Failure;
    }

    if (m_nBlockYSize >
        INT_MAX / (std::max(1, m_nDataTypeSizeBytes * m_nBlockXSize)))
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Error in GetRasterBlock");
        return CE_Failure;
    }

    const int nGDALBlockSize =
        m_nDataTypeSizeBytes * m_nBlockXSize * m_nBlockYSize;

    // Calculate block offset in case we have spill file. Use predefined
    // block map otherwise.

    if (nDataSize != -1 && nGDALBlockSize > nDataSize)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid block size: %d",
                 nGDALBlockSize);
        return CE_Failure;
    }

    // Getting the row offsets to optimize access.
    if (FillRowOffsets() == false || m_aFileOffsets.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Some error in offsets calculation");
        return CE_Failure;
    }

    // Read the block in the documented or deduced offset
    if (VSIFSeekL(m_pfIMG, m_aFileOffsets[iBlock], SEEK_SET))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Read from invalid offset for grid block.");
        return CE_Failure;
    }

    size_t nCompressedRawSize;
    if (iBlock == m_nHeight - 1)
        nCompressedRawSize = SIZE_MAX;  // We don't know it
    else
        nCompressedRawSize = static_cast<size_t>(m_aFileOffsets[iBlock + 1] -
                                                 m_aFileOffsets[iBlock]);

    return GetBlockData(pData, nCompressedRawSize);
}

void MMRBand::UpdateGeoTransform()
{
    m_gt.xorig = GetBoundingBoxMinX();
    m_gt.xscale = (GetBoundingBoxMaxX() - m_gt.xorig) / GetWidth();
    m_gt.xrot = 0.0;  // No rotation in MiraMon rasters
    m_gt.yorig = GetBoundingBoxMaxY();
    m_gt.yrot = 0.0;
    m_gt.yscale = (GetBoundingBoxMinY() - m_gt.yorig) / GetHeight();
}

/************************************************************************/
/*                           Other functions                            */
/************************************************************************/

// [ATTRIBUTE_DATA:xxxx] or [OVERVIEW:ASPECTES_TECNICS]
bool MMRBand::Get_ATTRIBUTE_DATA_or_OVERVIEW_ASPECTES_TECNICS_int(
    const CPLString &osSection, const char *pszKey, int *nValue,
    const char *pszErrorMessage)
{
    if (osSection.empty() || !pszKey || !nValue)
        return false;

    CPLString osValue;
    if (!m_pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, osSection, pszKey,
                                   osValue) ||
        osValue.empty())
    {
        if (m_pfRel->GetMetadataValue(SECTION_OVERVIEW,
                                      SECTION_ASPECTES_TECNICS, pszKey,
                                      osValue) == false ||
            osValue.empty())
        {
            if (pszErrorMessage)
                CPLError(CE_Failure, CPLE_AppDefined, "%s", pszErrorMessage);
            return false;
        }
    }

    if (1 != sscanf(osValue, "%d", nValue))
    {
        if (pszErrorMessage)
            CPLError(CE_Failure, CPLE_AppDefined, "%s", pszErrorMessage);
        return false;
    }
    return true;
}

bool MMRBand::GetDataTypeAndBytesPerPixel(const char *pszCompType,
                                          MMDataType *nCompressionType,
                                          MMBytesPerPixel *nBytesPerPixel)
{
    if (!nCompressionType || !nBytesPerPixel || !pszCompType)
        return false;

    if (EQUAL(pszCompType, "bit"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_BIT;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_BYTE_I_RLE;
        return true;
    }
    if (EQUAL(pszCompType, "byte"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_BYTE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_BYTE_I_RLE;
        return true;
    }
    if (EQUAL(pszCompType, "byte-RLE"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_BYTE_RLE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_BYTE_I_RLE;
        return true;
    }
    if (EQUAL(pszCompType, "integer"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_INTEGER;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_INTEGER_I_RLE;
        return true;
    }
    if (EQUAL(pszCompType, "integer-RLE"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_INTEGER_RLE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_INTEGER_I_RLE;
        return true;
    }
    if (EQUAL(pszCompType, "uinteger"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_UINTEGER;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_INTEGER_I_RLE;
        return true;
    }
    if (EQUAL(pszCompType, "uinteger-RLE"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_UINTEGER_RLE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_INTEGER_I_RLE;
        return true;
    }
    if (EQUAL(pszCompType, "long"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_LONG;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_LONG_REAL_I_RLE;
        return true;
    }
    if (EQUAL(pszCompType, "long-RLE"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_LONG_RLE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_LONG_REAL_I_RLE;
        return true;
    }
    if (EQUAL(pszCompType, "real"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_REAL;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_LONG_REAL_I_RLE;
        return true;
    }
    if (EQUAL(pszCompType, "real-RLE"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_REAL_RLE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_LONG_REAL_I_RLE;
        return true;
    }
    if (EQUAL(pszCompType, "double"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_DOUBLE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_DOUBLE_I_RLE;
        return true;
    }
    if (EQUAL(pszCompType, "double-RLE"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_DOUBLE_RLE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_DOUBLE_I_RLE;
        return true;
    }

    return false;
}

// Getting data type from metadata
bool MMRBand::UpdateDataTypeFromREL(const CPLString &osSection)
{
    m_eMMDataType = MMDataType::DATATYPE_AND_COMPR_UNDEFINED;
    m_eMMBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_UNDEFINED;

    CPLString osValue;
    if (!m_pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, osSection,
                                   "TipusCompressio", osValue) ||
        osValue.empty())
    {
        m_nWidth = 0;
        m_nHeight = 0;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "MiraMonRaster: no nDataType documented");
        return false;
    }

    if (!GetDataTypeAndBytesPerPixel(osValue.c_str(), &m_eMMDataType,
                                     &m_eMMBytesPerPixel))
    {
        m_nWidth = 0;
        m_nHeight = 0;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "MiraMonRaster: data type unhandled");
        return false;
    }

    m_nDataTypeSizeBytes = std::max(1, static_cast<int>(m_eMMBytesPerPixel));
    return true;
}

// Getting number of columns from metadata
bool MMRBand::UpdateColumnsNumberFromREL(const CPLString &osSection)
{
    return Get_ATTRIBUTE_DATA_or_OVERVIEW_ASPECTES_TECNICS_int(
        osSection, "columns", &m_nWidth,
        "MMRBand::MMRBand : No number of columns documented");
}

bool MMRBand::UpdateRowsNumberFromREL(const CPLString &osSection)
{
    return Get_ATTRIBUTE_DATA_or_OVERVIEW_ASPECTES_TECNICS_int(
        osSection, "rows", &m_nHeight,
        "MMRBand::MMRBand : No number of rows documented");
}

// Getting nodata value from metadata
void MMRBand::UpdateNoDataValue(const CPLString &osSection)
{
    CPLString osValue;
    if (!m_pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, osSection, "NODATA",
                                   osValue) ||
        osValue.empty())
    {
        m_dfNoData = 0;  // No a valid value.
        m_bNoDataSet = false;
    }
    else
    {
        m_dfNoData = CPLAtof(osValue);
        m_bNoDataSet = true;
    }
}

void MMRBand::UpdateMinMaxValuesFromREL(const CPLString &osSection)
{
    m_bMinSet = false;

    CPLString osValue;

    if (m_pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, osSection, "min",
                                  osValue) &&
        !osValue.empty())
    {
        if (1 == CPLsscanf(osValue, "%lf", &m_dfMin))
            m_bMinSet = true;
    }

    m_bMaxSet = false;
    if (m_pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, osSection, "max",
                                  osValue) &&
        !osValue.empty())
    {
        if (1 == CPLsscanf(osValue, "%lf", &m_dfMax))
            m_bMaxSet = true;
    }

    // Special case: dfMin > dfMax
    if (m_bMinSet && m_bMaxSet && m_dfMin > m_dfMax)
    {
        m_bMinSet = false;
        m_bMaxSet = false;
    }
}

void MMRBand::UpdateUnitTypeValueFromREL(const CPLString &osSection)
{
    CPLString osValue;

    if (m_pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, osSection, "unitats",
                                  osValue) &&
        !osValue.empty())
    {
        m_osBandUnitType = std::move(osValue);
    }
}

void MMRBand::UpdateMinMaxVisuValuesFromREL(const CPLString &osSection)
{
    m_bMinVisuSet = false;
    m_dfVisuMin = 1;

    CPLString osValue;
    if (m_pfRel->GetMetadataValue(SECTION_COLOR_TEXT, osSection,
                                  "Color_ValorColor_0", osValue) &&
        !osValue.empty())
    {
        if (1 == CPLsscanf(osValue, "%lf", &m_dfVisuMin))
            m_bMinVisuSet = true;
    }

    m_bMaxVisuSet = false;
    m_dfVisuMax = 1;

    if (m_pfRel->GetMetadataValue(SECTION_COLOR_TEXT, osSection,
                                  "Color_ValorColor_n_1", osValue) &&
        !osValue.empty())
    {
        if (1 == CPLsscanf(osValue, "%lf", &m_dfVisuMax))
            m_bMaxVisuSet = true;
    }
}

void MMRBand::UpdateFriendlyDescriptionFromREL(const CPLString &osSection)
{
    // This "if" is due to CID 1620830 in Coverity Scan
    if (!m_pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, osSection,
                                   KEY_descriptor, m_osFriendlyDescription))
        m_osFriendlyDescription = "";
}

void MMRBand::UpdateReferenceSystemFromREL()
{
    // This "if" is due to CID 1620842 in Coverity Scan
    if (!m_pfRel->GetMetadataValue("SPATIAL_REFERENCE_SYSTEM:HORIZONTAL",
                                   "HorizontalSystemIdentifier", m_osRefSystem))
        m_osRefSystem = "";
}

void MMRBand::UpdateBoundingBoxFromREL(const CPLString &osSection)
{
    // Bounding box of the band
    // [ATTRIBUTE_DATA:xxxx:EXTENT] or [EXTENT]
    CPLString osValue;
    if (!m_pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, osSection,
                                   SECTION_EXTENT, "MinX", osValue) ||
        osValue.empty())
    {
        m_dfBBMinX = 0;
    }
    else
    {
        if (1 != CPLsscanf(osValue, "%lf", &m_dfBBMinX))
            m_dfBBMinX = 0;
    }

    if (!m_pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, osSection,
                                   SECTION_EXTENT, "MaxX", osValue) ||
        osValue.empty())
    {
        m_dfBBMaxX = m_nWidth;
    }
    else
    {
        if (1 != CPLsscanf(osValue, "%lf", &m_dfBBMaxX))
        {
            // If the value is something that cannot be scanned,
            // we silently continue as it was undefined.
            m_dfBBMaxX = m_nWidth;
        }
    }

    if (!m_pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, osSection,
                                   SECTION_EXTENT, "MinY", osValue) ||
        osValue.empty())
    {
        m_dfBBMinY = 0;
    }
    else
    {
        if (1 != CPLsscanf(osValue, "%lf", &m_dfBBMinY))
            m_dfBBMinY = 0;
    }

    if (!m_pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, osSection,
                                   SECTION_EXTENT, "MaxY", osValue) ||
        osValue.empty())
    {
        m_dfBBMaxY = m_nHeight;
    }
    else
    {
        if (1 != CPLsscanf(osValue, "%lf", &m_dfBBMaxY))
        {
            // If the value is something that cannot be scanned,
            // we silently continue as it was undefined.
            m_dfBBMaxY = m_nHeight;
        }
    }
}

void MMRBand::UpdateSimbolizationInfo(const CPLString &osSection)
{
    m_pfRel->GetMetadataValue(SECTION_COLOR_TEXT, osSection, "Color_Const",
                              m_osColor_Const);

    if (EQUAL(m_osColor_Const, "1"))
    {
        if (CE_None == m_pfRel->UpdateGDALColorEntryFromBand(
                           osSection, m_sConstantColorRGB))
            m_osValidColorConst = true;
    }

    m_pfRel->GetMetadataValue(SECTION_COLOR_TEXT, osSection, "Color_Paleta",
                              m_osColor_Paleta);

    // Treatment of the color variable
    m_pfRel->GetMetadataValue(SECTION_COLOR_TEXT, osSection,
                              "Color_TractamentVariable",
                              m_osColor_TractamentVariable);

    m_pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, osSection,
                              KEY_TractamentVariable, m_osTractamentVariable);

    // Is categorical?
    if (m_osTractamentVariable.empty())
    {
        m_bIsCategorical = false;
    }
    else
    {
        if (EQUAL(m_osTractamentVariable, "Categoric"))
            m_bIsCategorical = true;
        else
            m_bIsCategorical = false;
    }

    m_pfRel->GetMetadataValue(SECTION_COLOR_TEXT, osSection,
                              "Color_EscalatColor", m_osColor_EscalatColor);

    m_pfRel->GetMetadataValue(SECTION_COLOR_TEXT, osSection,
                              "Color_N_SimbolsALaTaula",
                              m_osColor_N_SimbolsALaTaula);
}

void MMRBand::UpdateRATInfo(const CPLString &osSection)
{
    CPLString os_IndexJoin;

    if (!m_pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, osSection,
                                   "IndexsJoinTaula", os_IndexJoin) ||
        os_IndexJoin.empty())
    {
        return;
    }

    // Let's see if there is any table that can ve converted to RAT
    const CPLStringList aosTokens(CSLTokenizeString2(os_IndexJoin, ",", 0));
    const int nTokens = CSLCount(aosTokens);
    if (nTokens < 1)
        return;

    CPLString os_Join = "JoinTaula";
    os_Join.append("_");
    os_Join.append(aosTokens[0]);

    CPLString osTableNameSection_value;
    if (!m_pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, osSection, os_Join,
                                   osTableNameSection_value) ||
        osTableNameSection_value.empty())
        return;

    CPLString osTableNameSection = "TAULA_";
    osTableNameSection.append(osTableNameSection_value);

    if (!m_pfRel->GetMetadataValue(osTableNameSection, KEY_NomFitxer,
                                   m_osShortRATName) ||
        m_osShortRATName.empty())
    {
        m_osAssociateREL = "";
        return;
    }

    CPL_IGNORE_RET_VAL(m_pfRel->GetMetadataValue(
        osTableNameSection, "AssociatRel", m_osAssociateREL));
}

/************************************************************************/
/*             Functions that read bytes from IMG file band             */
/************************************************************************/
template <typename TYPE>
CPLErr MMRBand::UncompressRow(void *rowBuffer, size_t nCompressedRawSize)
{
    int nAccumulated = 0L, nIAccumulated = 0L;
    unsigned char cCounter;
    size_t nCompressedIndex = 0;

    TYPE RLEValue;
    TYPE *pDst;
    size_t sizeof_TYPE = sizeof(TYPE);

    std::vector<unsigned char> aCompressedRow;

    if (nCompressedRawSize != SIZE_MAX)
    {
        if (nCompressedRawSize > 1000 * 1000 &&
            GetFileSize() < nCompressedRawSize)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Too small file");
            return CE_Failure;
        }
        try
        {
            aCompressedRow.resize(nCompressedRawSize);
        }
        catch (const std::exception &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Out of memory allocating working buffer");
            return CE_Failure;
        }
        if (VSIFReadL(aCompressedRow.data(), nCompressedRawSize, 1, m_pfIMG) !=
            1)
            return CE_Failure;
    }

    while (nAccumulated < m_nWidth)
    {
        if (nCompressedRawSize == SIZE_MAX)
        {
            if (VSIFReadL(&cCounter, 1, 1, m_pfIMG) != 1)
                return CE_Failure;
        }
        else
        {
            if (nCompressedIndex >= aCompressedRow.size())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid nCompressedIndex");
                return CE_Failure;
            }
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
                if (VSIFReadL(&cCounter, 1, 1, m_pfIMG) != 1)
                    return CE_Failure;
            }
            else
            {
                if (nCompressedIndex >= aCompressedRow.size())
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Invalid nCompressedIndex");
                    return CE_Failure;
                }
                cCounter = aCompressedRow[nCompressedIndex];
                nCompressedIndex++;
            }

            nAccumulated += cCounter;

            if (nAccumulated > m_nWidth) /* This should not happen if the file
                                  is RLE and does not share counters across rows */
                return CE_Failure;

            for (; nIAccumulated < nAccumulated; nIAccumulated++)
            {
                if (nCompressedRawSize == SIZE_MAX)
                {
                    VSIFReadL(&RLEValue, sizeof_TYPE, 1, m_pfIMG);
                    memcpy((static_cast<TYPE *>(rowBuffer)) + nIAccumulated,
                           &RLEValue, sizeof_TYPE);
                }
                else
                {
                    if (nCompressedIndex + sizeof_TYPE > aCompressedRow.size())
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Invalid nCompressedIndex");
                        return CE_Failure;
                    }
                    memcpy((static_cast<TYPE *>(rowBuffer)) + nIAccumulated,
                           &aCompressedRow[nCompressedIndex], sizeof_TYPE);
                    nCompressedIndex += sizeof_TYPE;
                }
            }
        }
        else
        {
            nAccumulated += cCounter;
            if (nAccumulated > m_nWidth) /* This should not happen if the file
                                  is RLE and does not share counters across rows */
                return CE_Failure;

            if (nCompressedRawSize == SIZE_MAX)
            {
                if (VSIFReadL(&RLEValue, sizeof_TYPE, 1, m_pfIMG) != 1)
                    return CE_Failure;
            }
            else
            {
                if (nCompressedIndex + sizeof(TYPE) > aCompressedRow.size())
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Invalid nCompressedIndex");
                    return CE_Failure;
                }
                memcpy(&RLEValue, &aCompressedRow[nCompressedIndex],
                       sizeof(TYPE));
                nCompressedIndex += sizeof(TYPE);
            }

            const int nCount = nAccumulated - nIAccumulated;
            pDst = static_cast<TYPE *>(rowBuffer) + nIAccumulated;

            std::fill(pDst, pDst + nCount, RLEValue);

            nIAccumulated = nAccumulated;
        }
    }

    return CE_None;
}

CPLErr MMRBand::GetBlockData(void *rowBuffer, size_t nCompressedRawSize)
{
    if (m_eMMDataType == MMDataType::DATATYPE_AND_COMPR_BIT)
    {
        const int nGDALBlockSize = DIV_ROUND_UP(m_nBlockXSize, 8);

        if (VSIFReadL(rowBuffer, nGDALBlockSize, 1, m_pfIMG) != 1)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Error while reading band");
            return CE_Failure;
        }
        return CE_None;
    }

    if (m_eMMDataType == MMDataType::DATATYPE_AND_COMPR_BYTE ||
        m_eMMDataType == MMDataType::DATATYPE_AND_COMPR_INTEGER ||
        m_eMMDataType == MMDataType::DATATYPE_AND_COMPR_UINTEGER ||
        m_eMMDataType == MMDataType::DATATYPE_AND_COMPR_LONG ||
        m_eMMDataType == MMDataType::DATATYPE_AND_COMPR_REAL ||
        m_eMMDataType == MMDataType::DATATYPE_AND_COMPR_DOUBLE)
    {
        if (VSIFReadL(rowBuffer, m_nDataTypeSizeBytes, m_nWidth, m_pfIMG) !=
            static_cast<size_t>(m_nWidth))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Error while reading band");
            return CE_Failure;
        }
        return CE_None;
    }

    CPLErr eErr;
    switch (m_eMMDataType)
    {
        case MMDataType::DATATYPE_AND_COMPR_BYTE_RLE:
            eErr = UncompressRow<GByte>(rowBuffer, nCompressedRawSize);
            break;
        case MMDataType::DATATYPE_AND_COMPR_INTEGER_RLE:
            eErr = UncompressRow<GInt16>(rowBuffer, nCompressedRawSize);
            break;
        case MMDataType::DATATYPE_AND_COMPR_UINTEGER_RLE:
            eErr = UncompressRow<GUInt16>(rowBuffer, nCompressedRawSize);
            break;
        case MMDataType::DATATYPE_AND_COMPR_LONG_RLE:
            eErr = UncompressRow<GInt32>(rowBuffer, nCompressedRawSize);
            break;
        case MMDataType::DATATYPE_AND_COMPR_REAL_RLE:
            eErr = UncompressRow<float>(rowBuffer, nCompressedRawSize);
            break;
        case MMDataType::DATATYPE_AND_COMPR_DOUBLE_RLE:
            eErr = UncompressRow<double>(rowBuffer, nCompressedRawSize);
            break;

        default:
            CPLError(CE_Failure, CPLE_AppDefined, "Error in datatype");
            eErr = CE_Failure;
    }

    return eErr;
}  // End of GetBlockData()

int MMRBand::PositionAtStartOfRowOffsetsInFile()
{
    vsi_l_offset nFileSize, nHeaderOffset;
    char szChain[16];
    GInt16 nVersion, nSubVersion;
    int nOffsetSize, nOffsetsSectionType;

    if (VSIFSeekL(m_pfIMG, 0, SEEK_END))
        return 0;

    nFileSize = VSIFTellL(m_pfIMG);

    if (nFileSize < 32)  // Minimum required size
        return 0;

    if (m_nHeight)
    {
        if (nFileSize < static_cast<vsi_l_offset>(32) + m_nHeight + 32)
            return 0;
    }

    vsi_l_offset nHeadOffset = nFileSize - 32;

    if (VSIFSeekL(m_pfIMG, nHeadOffset, SEEK_SET))  // Reading final header.
        return 0;
    if (VSIFReadL(szChain, 16, 1, m_pfIMG) != 1)
        return 0;
    for (int nIndex = 0; nIndex < 16; nIndex++)
    {
        if (szChain[nIndex] != '\0')
            return 0;  // Supposed 0's are not 0.
    }

    if (VSIFReadL(szChain, 8, 1, m_pfIMG) != 1)
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
    if (VSIFReadL(&nHeaderOffset, sizeof(vsi_l_offset), 1, m_pfIMG) != 1)
        return 0;

    std::set<vsi_l_offset> alreadyVisitedOffsets;
    bool bRepeat;
    do
    {
        bRepeat = FALSE;

        if (VSIFSeekL(m_pfIMG, nHeaderOffset, SEEK_SET))
            return 0;

        if (VSIFReadL(szChain, 8, 1, m_pfIMG) != 1)
            return 0;

        if (strncmp(szChain, "IMG ", 4) || szChain[5] != '.')
            return 0;

        if (VSIFReadL(&nOffsetsSectionType, 4, 1, m_pfIMG) != 1)
            return 0;

        if (nOffsetsSectionType != 2)  // 2 = row offsets section
        {
            // This is not the section I am looking for
            if (VSIFSeekL(m_pfIMG, 8 + 4, SEEK_CUR))
                return 0;

            if (VSIFReadL(&nHeaderOffset, sizeof(vsi_l_offset), 1, m_pfIMG) !=
                1)
                return 0;

            if (nHeaderOffset == 0)
                return 0;

            if (cpl::contains(alreadyVisitedOffsets, nHeaderOffset))
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "Error reading offsets. They will be ignored.");
                return 0;
            }

            alreadyVisitedOffsets.insert(nHeaderOffset);

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
        RasterRLE: minimum size: nHeight*2
        Offsets:   minimum size: 32+nHeight*4
        Final:     size: 32
    */

    if (m_nHeight)
    {
        if (nHeaderOffset < static_cast<vsi_l_offset>(m_nHeight) *
                                2 ||  // Minimum size of an RLE
            nFileSize - nHeaderOffset <
                static_cast<vsi_l_offset>(32) + m_nHeight +
                    32)  // Minimum size of the section in version 1.0
            return 0;
    }

    if (VSIFReadL(&nOffsetSize, 4, 1, m_pfIMG) != 1 ||
        (nOffsetSize != 8 && nOffsetSize != 4 && nOffsetSize != 2 &&
         nOffsetSize != 1))
        return 0;

    if (m_nHeight)
    {
        if (nFileSize - nHeaderOffset <
            32 + static_cast<vsi_l_offset>(nOffsetSize) * m_nHeight +
                32)  // No space for this section in this file
            return 0;

        // I leave the file prepared to read offsets
        if (VSIFSeekL(m_pfIMG, 16, SEEK_CUR))
            return 0;
    }
    else
    {
        if (VSIFSeekL(m_pfIMG, 4, SEEK_CUR))
            return 0;

        if (VSIFSeekL(m_pfIMG, 4, SEEK_CUR))
            return 0;

        // I leave the file prepared to read offsets
        if (VSIFSeekL(m_pfIMG, 8, SEEK_CUR))
            return 0;
    }

    // There are offsets!
    return nOffsetSize;
}  // Fi de PositionAtStartOfRowOffsetsInFile()

/************************************************************************/
/*                            GetFileSize()                             */
/************************************************************************/

vsi_l_offset MMRBand::GetFileSize()
{
    if (m_nFileSize == 0)
    {
        const auto nCurPos = VSIFTellL(m_pfIMG);
        VSIFSeekL(m_pfIMG, 0, SEEK_END);
        m_nFileSize = VSIFTellL(m_pfIMG);
        VSIFSeekL(m_pfIMG, nCurPos, SEEK_SET);
    }
    return m_nFileSize;
}

/************************************************************************/
/*                           FillRowOffsets()                           */
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
    const int nGDALBlockSize = DIV_ROUND_UP(m_nBlockXSize, 8);

    // If it's filled, there is no need to fill it again
    if (!m_aFileOffsets.empty())
        return true;

    // Sanity check to avoid attempting huge memory allocation
    if (m_nHeight > 1000 * 1000)
    {
        if (GetFileSize() < static_cast<vsi_l_offset>(m_nHeight))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Too small file");
            return false;
        }
    }

    try
    {
        m_aFileOffsets.resize(static_cast<size_t>(m_nHeight) + 1);
    }
    catch (const std::bad_alloc &e)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
        return false;
    }

    switch (m_eMMDataType)
    {
        case MMDataType::DATATYPE_AND_COMPR_BIT:

            // "<=" it's ok. There is space and it's to make easier the programming
            for (nIRow = 0; nIRow <= m_nHeight; nIRow++)
                m_aFileOffsets[nIRow] =
                    static_cast<vsi_l_offset>(nIRow) * nGDALBlockSize;
            break;

        case MMDataType::DATATYPE_AND_COMPR_BYTE:
        case MMDataType::DATATYPE_AND_COMPR_INTEGER:
        case MMDataType::DATATYPE_AND_COMPR_UINTEGER:
        case MMDataType::DATATYPE_AND_COMPR_LONG:
        case MMDataType::DATATYPE_AND_COMPR_REAL:
        case MMDataType::DATATYPE_AND_COMPR_DOUBLE:
            nBytesPerPixelPerNCol =
                m_nDataTypeSizeBytes * static_cast<vsi_l_offset>(m_nWidth);
            // "<=" it's ok. There is space and it's to make easier the programming
            for (nIRow = 0; nIRow <= m_nHeight; nIRow++)
                m_aFileOffsets[nIRow] =
                    static_cast<vsi_l_offset>(nIRow) * nBytesPerPixelPerNCol;
            break;

        case MMDataType::DATATYPE_AND_COMPR_BYTE_RLE:
        case MMDataType::DATATYPE_AND_COMPR_INTEGER_RLE:
        case MMDataType::DATATYPE_AND_COMPR_UINTEGER_RLE:
        case MMDataType::DATATYPE_AND_COMPR_LONG_RLE:
        case MMDataType::DATATYPE_AND_COMPR_REAL_RLE:
        case MMDataType::DATATYPE_AND_COMPR_DOUBLE_RLE:

            nStartOffset = VSIFTellL(m_pfIMG);

            // Let's determine if are there offsets in the file
            if (0 < (nSizeToRead = PositionAtStartOfRowOffsetsInFile()))
            {
                // I have offsets!!
                nFileByte = 0L;  // all bits to 0
                for (nIRow = 0; nIRow < m_nHeight; nIRow++)
                {
                    if (VSIFReadL(&nFileByte, nSizeToRead, 1, m_pfIMG) != 1)
                        return false;

                    m_aFileOffsets[nIRow] = nFileByte;

                    // Let's check that the difference between two offsets is in a int range
                    if (nIRow > 0)
                    {
                        if (m_aFileOffsets[nIRow] <=
                            m_aFileOffsets[static_cast<size_t>(nIRow) - 1])
                            return false;

                        if (m_aFileOffsets[nIRow] -
                                m_aFileOffsets[static_cast<size_t>(nIRow) -
                                               1] >=
                            static_cast<vsi_l_offset>(SIZE_MAX))
                            return false;
                    }
                }
                m_aFileOffsets[nIRow] = 0;  // Not reliable
                VSIFSeekL(m_pfIMG, nStartOffset, SEEK_SET);
                break;
            }

            // Not indexed RLE. We create a dynamic indexation
            if (m_nWidth >
                INT_MAX /
                    (std::max(1, static_cast<int>(m_eMMBytesPerPixel)) + 1))
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Too large row: %d",
                         m_nWidth);
                VSIFSeekL(m_pfIMG, nStartOffset, SEEK_SET);
                return false;
            }

            nMaxBytesPerCompressedRow =
                static_cast<int>(m_eMMBytesPerPixel)
                    ? (m_nWidth * (static_cast<int>(m_eMMBytesPerPixel) + 1))
                    : (m_nWidth * (1 + 1));
            unsigned char *pBuffer;

            if (nullptr == (pBuffer = static_cast<unsigned char *>(
                                VSI_MALLOC_VERBOSE(nMaxBytesPerCompressedRow))))
            {
                VSIFSeekL(m_pfIMG, nStartOffset, SEEK_SET);
                return false;
            }

            VSIFSeekL(m_pfIMG, 0, SEEK_SET);
            m_aFileOffsets[0] = 0;
            for (nIRow = 0; nIRow < m_nHeight; nIRow++)
            {
                GetBlockData(pBuffer, SIZE_MAX);
                m_aFileOffsets[static_cast<size_t>(nIRow) + 1] =
                    VSIFTellL(m_pfIMG);
            }
            VSIFree(pBuffer);
            VSIFSeekL(m_pfIMG, nStartOffset, SEEK_SET);
            break;

        default:
            return false;
    }  // End of switch (eMMDataType)
    return true;

}  // End of FillRowOffsets()

/************************************************************************/
/*                              Writing part()                          */
/* Indexing a compressed file increments the efficiency when reading it */
/************************************************************************/
bool MMRBand::WriteRowOffsets()
{
    if (m_aFileOffsets.empty() || m_nHeight == 0)
        return true;

    VSIFSeekL(m_pfIMG, 0, SEEK_END);

    vsi_l_offset nStartOffset = VSIFTellL(m_pfIMG);
    if (nStartOffset == 0)
        return false;

    if (VSIFWriteL("IMG 1.0\0", sizeof("IMG 1.0"), 1, m_pfIMG) != 1)
        return false;

    // Type of section
    int nAux = 2;
    if (VSIFWriteL(&nAux, 4, 1, m_pfIMG) != 1)
        return false;

    size_t nIndexOffset = static_cast<size_t>(m_nHeight);
    int nOffsetSize;

    if (m_aFileOffsets[nIndexOffset - 1] < static_cast<vsi_l_offset>(UCHAR_MAX))
        nOffsetSize = 1;
    else if (m_aFileOffsets[nIndexOffset - 1] <
             static_cast<vsi_l_offset>(USHRT_MAX))
        nOffsetSize = 2;
    else if (m_aFileOffsets[nIndexOffset - 1] <
             static_cast<vsi_l_offset>(UINT32_MAX))
        nOffsetSize = 4;
    else
        nOffsetSize = 8;

    if (VSIFWriteL(&nOffsetSize, 4, 1, m_pfIMG) != 1)
        return false;

    nAux = 0;
    if (VSIFWriteL(&nAux, 4, 1, m_pfIMG) != 1)
        return false;
    if (VSIFWriteL(&nAux, 4, 1, m_pfIMG) != 1)
        return false;

    vsi_l_offset nUselessOffset = 0;
    if (VSIFWriteL(&nUselessOffset, sizeof(vsi_l_offset), 1, m_pfIMG) != 1)
        return false;

    // The main part
    size_t nSizeTOffsetSize = nOffsetSize;
    if (nSizeTOffsetSize == sizeof(vsi_l_offset))
    {
        if (VSIFWriteL(&(m_aFileOffsets[0]), sizeof(vsi_l_offset), nIndexOffset,
                       m_pfIMG) != nIndexOffset)
            return false;
    }
    else
    {
        for (nIndexOffset = 0; nIndexOffset < m_aFileOffsets.size() - 1;
             nIndexOffset++)
        {
            if (VSIFWriteL(&(m_aFileOffsets[nIndexOffset]), nSizeTOffsetSize, 1,
                           m_pfIMG) != 1)
                return false;
        }
    }

    // End part of file
    nAux = 0;
    for (int nIndex = 0; nIndex < 4; nIndex++)
    {
        if (VSIFWriteL(&nAux, 4, 1, m_pfIMG) != 1)
            return false;
    }
    if (VSIFWriteL("IMG 1.0\0", sizeof("IMG 1.0"), 1, m_pfIMG) != 1)
        return false;

    if (VSIFWriteL(&nStartOffset, sizeof(vsi_l_offset), 1, m_pfIMG) != 1)
        return false;

    return true;

}  // End of WriteRowOffsets()

// Getting data type from dataset
bool MMRBand::UpdateDataTypeAndBytesPerPixelFromRasterBand(
    GDALRasterBand &papoBand)
{
    switch (papoBand.GetRasterDataType())
    {
        case GDT_Byte:
            if (m_bIsCompressed)
                m_eMMDataType = MMDataType::DATATYPE_AND_COMPR_BYTE_RLE;
            else
                m_eMMDataType = MMDataType::DATATYPE_AND_COMPR_BYTE;

            m_eMMBytesPerPixel =
                MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_BYTE_I_RLE;
            break;

        case GDT_UInt16:
            if (m_bIsCompressed)
                m_eMMDataType = MMDataType::DATATYPE_AND_COMPR_UINTEGER_RLE;
            else
                m_eMMDataType = MMDataType::DATATYPE_AND_COMPR_UINTEGER;

            m_eMMBytesPerPixel =
                MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_INTEGER_I_RLE;
            break;

        case GDT_Int16:
            if (m_bIsCompressed)
                m_eMMDataType = MMDataType::DATATYPE_AND_COMPR_INTEGER_RLE;
            else
                m_eMMDataType = MMDataType::DATATYPE_AND_COMPR_INTEGER;

            m_eMMBytesPerPixel =
                MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_INTEGER_I_RLE;
            break;

        case GDT_Int32:
            if (m_bIsCompressed)
                m_eMMDataType = MMDataType::DATATYPE_AND_COMPR_LONG_RLE;
            else
                m_eMMDataType = MMDataType::DATATYPE_AND_COMPR_LONG;

            m_eMMBytesPerPixel =
                MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_LONG_REAL_I_RLE;
            break;

        case GDT_Float32:
            if (m_bIsCompressed)
                m_eMMDataType = MMDataType::DATATYPE_AND_COMPR_REAL_RLE;
            else
                m_eMMDataType = MMDataType::DATATYPE_AND_COMPR_REAL;

            m_eMMBytesPerPixel =
                MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_LONG_REAL_I_RLE;
            break;

        case GDT_Float64:
            if (m_bIsCompressed)
                m_eMMDataType = MMDataType::DATATYPE_AND_COMPR_DOUBLE_RLE;
            else
                m_eMMDataType = MMDataType::DATATYPE_AND_COMPR_DOUBLE;

            m_eMMBytesPerPixel =
                MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_DOUBLE_I_RLE;
            break;

        default:
            m_eMMDataType = MMDataType::DATATYPE_AND_COMPR_UNDEFINED;
            m_eMMBytesPerPixel =
                MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_UNDEFINED;
            m_nDataTypeSizeBytes = 0;
            m_nWidth = 0;
            m_nHeight = 0;
            CPLError(CE_Failure, CPLE_AppDefined,
                     "MiraMonRaster: data type unhandled");
            return false;
    }
    return true;
}

// Getting nodata value from metadata
void MMRBand::UpdateNoDataValueFromRasterBand(GDALRasterBand &papoBand)
{
    int pbSuccess;
    m_dfNoData = papoBand.GetNoDataValue(&pbSuccess);
    m_bNoDataSet = pbSuccess == 1 ? true : false;
}

CPLString MMRBand::GetRELDataType() const
{
    if (m_eMMDataType == MMDataType::DATATYPE_AND_COMPR_BIT)
        return "bit";
    if (m_eMMDataType == MMDataType::DATATYPE_AND_COMPR_BYTE ||
        m_eMMDataType == MMDataType::DATATYPE_AND_COMPR_BYTE_RLE)
    {
        if (m_bIsCompressed)
            return "byte-RLE";
        return "byte";
    }
    if (m_eMMDataType == MMDataType::DATATYPE_AND_COMPR_INTEGER ||
        m_eMMDataType == MMDataType::DATATYPE_AND_COMPR_INTEGER_RLE)
    {
        if (m_bIsCompressed)
            return "integer-RLE";
        return "integer";
    }

    if (m_eMMDataType == MMDataType::DATATYPE_AND_COMPR_UINTEGER ||
        m_eMMDataType == MMDataType::DATATYPE_AND_COMPR_UINTEGER_RLE)
    {
        if (m_bIsCompressed)
            return "uinteger-RLE";
        return "uinteger";
    }

    if (m_eMMDataType == MMDataType::DATATYPE_AND_COMPR_LONG ||
        m_eMMDataType == MMDataType::DATATYPE_AND_COMPR_LONG_RLE)
    {
        if (m_bIsCompressed)
            return "long-RLE";
        return "long";
    }

    if (m_eMMDataType == MMDataType::DATATYPE_AND_COMPR_REAL ||
        m_eMMDataType == MMDataType::DATATYPE_AND_COMPR_REAL_RLE)
    {
        if (m_bIsCompressed)
            return "real-RLE";
        return "real";
    }

    if (m_eMMDataType == MMDataType::DATATYPE_AND_COMPR_DOUBLE ||
        m_eMMDataType == MMDataType::DATATYPE_AND_COMPR_DOUBLE_RLE)
    {
        if (m_bIsCompressed)
            return "double-RLE";
        return "double";
    }

    return "";
}

void MMRBand::UpdateRowMinMax(const void *pBuffer)
{
    switch (m_eMMDataType)
    {
        case MMDataType::DATATYPE_AND_COMPR_BIT:
        case MMDataType::DATATYPE_AND_COMPR_BYTE:
        case MMDataType::DATATYPE_AND_COMPR_BYTE_RLE:
            UpdateRowMinMax<unsigned char>(pBuffer);
            break;

        case MMDataType::DATATYPE_AND_COMPR_UINTEGER:
        case MMDataType::DATATYPE_AND_COMPR_UINTEGER_RLE:
            UpdateRowMinMax<GUInt16>(pBuffer);
            break;

        case MMDataType::DATATYPE_AND_COMPR_INTEGER:
        case MMDataType::DATATYPE_AND_COMPR_INTEGER_RLE:
        case MMDataType::DATATYPE_AND_COMPR_INTEGER_ASCII:
            UpdateRowMinMax<GInt16>(pBuffer);
            break;

        case MMDataType::DATATYPE_AND_COMPR_LONG:
        case MMDataType::DATATYPE_AND_COMPR_LONG_RLE:
            UpdateRowMinMax<int>(pBuffer);
            break;

        case MMDataType::DATATYPE_AND_COMPR_REAL:
        case MMDataType::DATATYPE_AND_COMPR_REAL_RLE:
        case MMDataType::DATATYPE_AND_COMPR_REAL_ASCII:
            UpdateRowMinMax<float>(pBuffer);
            break;

        case MMDataType::DATATYPE_AND_COMPR_DOUBLE:
        case MMDataType::DATATYPE_AND_COMPR_DOUBLE_RLE:
            UpdateRowMinMax<double>(pBuffer);
            break;

        default:
            break;
    }
}

bool MMRBand::WriteBandFile(GDALDataset &oSrcDS, int nNBands, int nIBand)
{

    GDALRasterBand *pRasterBand = oSrcDS.GetRasterBand(nIBand + 1);
    if (!pRasterBand)
        return false;

    // Updating variable to suitable values
    m_nBlockXSize = m_nWidth;
    m_nBlockYSize = 1;
    m_nNRowsPerBlock = 1;

    // Opening the RAW file
    m_pfIMG = VSIFOpenL(m_osBandFileName, "wb");
    if (!m_pfIMG)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Failed to create MiraMon band file `%s' with access 'wb'.",
                 m_osBandFileName.c_str());
        return false;
    }

    // Creating index information
    if (m_bIsCompressed)
    {
        try
        {
            m_aFileOffsets.resize(static_cast<size_t>(m_nHeight) + 1);
        }
        catch (const std::bad_alloc &e)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
            return false;
        }
    }

    GDALDataType eDT = pRasterBand->GetRasterDataType();

    // Temporary buffer for one scanline
    void *pBuffer =
        VSI_MALLOC2_VERBOSE(m_nWidth, GDALGetDataTypeSizeBytes(eDT));
    if (pBuffer == nullptr)
        return false;

    void *pRow =
        VSI_MALLOC2_VERBOSE(m_nWidth, (GDALGetDataTypeSizeBytes(eDT) + 1));
    if (pRow == nullptr)
    {
        VSIFree(pBuffer);
        return false;
    }

    // Loop over each line
    double dfComplete = nIBand * 1.0 / nNBands;
    double dfIncr = 1.0 / (nNBands * m_nHeight);
    if (!m_pfnProgress(dfComplete, nullptr, m_pProgressData))
    {
        VSIFree(pBuffer);
        VSIFree(pRow);
        return false;
    }

    m_bMinSet = false;
    m_dfMin = std::numeric_limits<double>::max();
    m_bMaxSet = false;
    m_dfMax = -std::numeric_limits<double>::max();

    for (int iLine = 0; iLine < m_nHeight; ++iLine)
    {
        // Read one line from the raster band
        CPLErr err =
            pRasterBand->RasterIO(GF_Read, 0, iLine, m_nWidth, 1, pBuffer,
                                  m_nWidth, 1, eDT, 0, 0, nullptr);

        if (err != CE_None)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Error reading line %d from raster band", iLine);
            VSIFree(pBuffer);
            VSIFree(pRow);
            return false;
        }

        // MinMax calculation
        UpdateRowMinMax(pBuffer);

        if (m_bIsCompressed)
            m_aFileOffsets[iLine] = VSIFTellL(m_pfIMG);

        // Write the line to the MiraMon band file
        size_t nWritten, nCompressed;
        if (m_bIsCompressed)
        {
            nCompressed =
                CompressRowType(m_eMMDataType, pBuffer, m_nWidth, pRow);
            nWritten = VSIFWriteL(pRow, 1, nCompressed, m_pfIMG);
            if (nWritten != nCompressed)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to write line %d to MiraMon band file", iLine);
                VSIFree(pBuffer);
                VSIFree(pRow);
                return false;
            }
        }
        else
        {
            nWritten = VSIFWriteL(pBuffer, GDALGetDataTypeSizeBytes(eDT),
                                  m_nWidth, m_pfIMG);
            if (nWritten != static_cast<size_t>(m_nWidth))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to write line %d to MiraMon band file", iLine);
                VSIFree(pBuffer);
                VSIFree(pRow);
                return false;
            }
        }

        dfComplete += dfIncr;
        if (!m_pfnProgress(dfComplete, nullptr, m_pProgressData))
        {
            VSIFree(pBuffer);
            VSIFree(pRow);
            return false;
        }
    }
    VSIFree(pBuffer);
    VSIFree(pRow);

    // Updating min and max values for simbolization
    m_dfVisuMin = m_dfMin;
    m_bMinVisuSet = m_bMinSet;
    m_dfVisuMax = m_dfMax;
    m_bMaxVisuSet = m_bMaxSet;

    // There is a final part that contain the indexes to every row
    if (m_bIsCompressed)
    {
        if (WriteRowOffsets() == false)
            return false;
    }

    dfComplete = (nIBand + 1.0) / nNBands;
    if (!m_pfnProgress(dfComplete, nullptr, m_pProgressData))
        return false;

    return true;
}

constexpr uint8_t LIMIT = 255;

template <typename T>
size_t MMRBand::CompressRowTypeTpl(const T *pRow, int nNCol, void *pBufferVoid)
{
    uint8_t *pBuffer = static_cast<uint8_t *>(pBufferVoid);

    T tPreviousValue = pRow[0];
    uint8_t nByteCounter = 0;
    size_t nRowBytes = 0;

    for (int i = 0; i < nNCol; i++)
    {
        if (tPreviousValue == pRow[i] && nByteCounter < LIMIT)
            nByteCounter++;
        else  // I have found a different value or I have reached the limit of the counter
        {
            if (nByteCounter == 1)
            {
                //In cas of three consecutive different values, it's more efficient
                // to write them as uncompressed values than as three RLE
                if (i + 2 < nNCol && pRow[i] != pRow[i + 1] &&
                    pRow[i + 1] != pRow[i + 2])
                {
                    // Indicates that the following values are
                    // uncompressed, and how many of them are there
                    *pBuffer++ = 0;
                    uint8_t *pHowManyUncompressed = pBuffer++;
                    nRowBytes += 2;

                    // Writing first three
                    memcpy(pBuffer, &tPreviousValue, sizeof(T));
                    pBuffer += sizeof(T);

                    tPreviousValue = pRow[i];
                    memcpy(pBuffer, &tPreviousValue, sizeof(T));
                    pBuffer += sizeof(T);
                    i++;

                    tPreviousValue = pRow[i];
                    memcpy(pBuffer, &tPreviousValue, sizeof(T));
                    pBuffer += sizeof(T);

                    nRowBytes += 3 * sizeof(T);

                    // nByteCounter is now the number of bytes of the three uncompressed values
                    nByteCounter = 3;

                    for (i++; i + 1 < nNCol && nByteCounter < LIMIT;
                         i++, nByteCounter++)
                    {
                        if (pRow[i] == pRow[i + 1])
                            break;

                        memcpy(pBuffer, pRow + i, sizeof(T));
                        pBuffer += sizeof(T);
                        nRowBytes += sizeof(T);
                    }

                    if (i + 1 == nNCol && nByteCounter < LIMIT)
                    {
                        *pHowManyUncompressed = ++nByteCounter;
                        memcpy(pBuffer, pRow + i, sizeof(T));
                        nRowBytes += sizeof(T);
                        return nRowBytes;
                    }

                    *pHowManyUncompressed = nByteCounter;
                    tPreviousValue = pRow[i];
                    nByteCounter = 1;
                    continue;
                }
            }

            // Normal RLE
            *pBuffer++ = nByteCounter;
            memcpy(pBuffer, &tPreviousValue, sizeof(T));
            pBuffer += sizeof(T);
            nRowBytes += 1 + sizeof(T);

            tPreviousValue = pRow[i];
            nByteCounter = 1;
        }
    }

    // Last element
    *pBuffer++ = nByteCounter;

    memcpy(pBuffer, &tPreviousValue, sizeof(T));
    nRowBytes += 1 + sizeof(T);

    return nRowBytes;
}

size_t MMRBand::CompressRowType(MMDataType nDataType, const void *pRow,
                                int nNCol, void *pBuffer)
{
    switch (nDataType)
    {
        case MMDataType::DATATYPE_AND_COMPR_BYTE_RLE:
        case MMDataType::DATATYPE_AND_COMPR_BYTE:
            return CompressRowTypeTpl<uint8_t>(
                reinterpret_cast<const uint8_t *>(pRow), nNCol, pBuffer);

        case MMDataType::DATATYPE_AND_COMPR_INTEGER_RLE:
        case MMDataType::DATATYPE_AND_COMPR_INTEGER:
            return CompressRowTypeTpl<GInt16>(
                reinterpret_cast<const GInt16 *>(pRow), nNCol, pBuffer);

        case MMDataType::DATATYPE_AND_COMPR_UINTEGER_RLE:
        case MMDataType::DATATYPE_AND_COMPR_UINTEGER:
            return CompressRowTypeTpl<GUInt16>(
                reinterpret_cast<const GUInt16 *>(pRow), nNCol, pBuffer);

        case MMDataType::DATATYPE_AND_COMPR_LONG_RLE:
        case MMDataType::DATATYPE_AND_COMPR_LONG:
            return CompressRowTypeTpl<GInt32>(
                reinterpret_cast<const GInt32 *>(pRow), nNCol, pBuffer);

        case MMDataType::DATATYPE_AND_COMPR_REAL_RLE:
        case MMDataType::DATATYPE_AND_COMPR_REAL:
            return CompressRowTypeTpl<float>(
                reinterpret_cast<const float *>(pRow), nNCol, pBuffer);

        case MMDataType::DATATYPE_AND_COMPR_DOUBLE_RLE:
        case MMDataType::DATATYPE_AND_COMPR_DOUBLE:
            return CompressRowTypeTpl<double>(
                reinterpret_cast<const double *>(pRow), nNCol, pBuffer);

        default:
            // same treatment than the original
            return 0;
    }
}

int MMRBand::WriteColorTable(GDALDataset &oSrcDS)
{
    GDALRasterBand *pRasterBand = oSrcDS.GetRasterBand(m_nIBand + 1);
    if (!pRasterBand)
        return 0;

    m_poCT = pRasterBand->GetColorTable();
    if (!m_poCT)
    {
        // Perhaps the RAT contains colors and MiraMon can use them as a palette
        return WriteColorTableFromRAT(oSrcDS);
    }

    if (!m_poCT->GetColorEntryCount())
        return 0;

    // Creating DBF table name
    if (!cpl::ends_with(m_osBandFileName, pszExtRaster))
        return 1;

    // Extract .img
    m_osCTName = m_osBandFileName;
    m_osCTName.resize(m_osCTName.size() - strlen(".img"));
    m_osCTName.append("_CT.dbf");

    // Creating DBF
    struct MM_DATA_BASE_XP *pBD_XP =
        MM_CreateDBFHeader(4, MM_JOC_CARAC_UTF8_DBF);
    if (!pBD_XP)
        return 1;

    // Assigning DBF table name
    CPLStrlcpy(pBD_XP->szFileName, m_osCTName, sizeof(pBD_XP->szFileName));

    // Initializing the table
    MM_EXT_DBF_N_RECORDS nPaletteColors =
        static_cast<MM_EXT_DBF_N_RECORDS>(m_poCT->GetColorEntryCount());
    pBD_XP->nFields = 4;
    pBD_XP->nRecords = nPaletteColors;
    pBD_XP->FirstRecordOffset =
        static_cast<MM_FIRST_RECORD_OFFSET_TYPE>(33 + (pBD_XP->nFields * 32));
    pBD_XP->CharSet = MM_JOC_CARAC_ANSI_DBASE;
    pBD_XP->dbf_version = MM_MARCA_DBASE4;
    pBD_XP->BytesPerRecord = 1;

    MM_ACCUMULATED_BYTES_TYPE_DBF nClauSimbolNBytes = 0;
    do
    {
        nClauSimbolNBytes++;
        nPaletteColors /= 10;
    } while (nPaletteColors > 0);
    nClauSimbolNBytes++;

    // Fields of the DBF table
    struct MM_FIELD MMField;
    MM_InitializeField(&MMField);
    MMField.FieldType = 'N';
    CPLStrlcpy(MMField.FieldName, "CLAUSIMBOL", MM_MAX_LON_FIELD_NAME_DBF);
    MMField.BytesPerField = nClauSimbolNBytes;
    MM_DuplicateFieldDBXP(pBD_XP->pField, &MMField);

    CPLStrlcpy(MMField.FieldName, "R_COLOR", MM_MAX_LON_FIELD_NAME_DBF);
    MMField.BytesPerField = 3;
    MM_DuplicateFieldDBXP(pBD_XP->pField + 1, &MMField);

    CPLStrlcpy(MMField.FieldName, "G_COLOR", MM_MAX_LON_FIELD_NAME_DBF);
    MMField.BytesPerField = 3;
    MM_DuplicateFieldDBXP(pBD_XP->pField + 2, &MMField);

    CPLStrlcpy(MMField.FieldName, "B_COLOR", MM_MAX_LON_FIELD_NAME_DBF);
    MMField.BytesPerField = 3;
    MM_DuplicateFieldDBXP(pBD_XP->pField + 3, &MMField);

    // Opening the table
    if (!MM_CreateAndOpenDBFFile(pBD_XP, pBD_XP->szFileName))
    {
        MM_ReleaseDBFHeader(&pBD_XP);
        return 1;
    }

    // Writing records to the table
    if (0 != VSIFSeekL(pBD_XP->pfDataBase,
                       static_cast<vsi_l_offset>(pBD_XP->FirstRecordOffset),
                       SEEK_SET))
    {
        MM_ReleaseDBFHeader(&pBD_XP);
        return 1;
    }

    GDALColorEntry colorEntry;
    int nIColor;
    for (nIColor = 0; nIColor < static_cast<int>(pBD_XP->nRecords); nIColor++)
    {
        m_poCT->GetColorEntryAsRGB(nIColor, &colorEntry);

        // Deletion flag
        if (!VSIFPrintfL(pBD_XP->pfDataBase, " "))
        {
            MM_ReleaseDBFHeader(&pBD_XP);
            return 1;
        }

        if (!VSIFPrintfL(pBD_XP->pfDataBase, "%*d",
                         static_cast<int>(nClauSimbolNBytes), nIColor))
        {
            MM_ReleaseDBFHeader(&pBD_XP);
            return 1;
        }
        if (colorEntry.c4 == 0)
        {
            if (!VSIFPrintfL(pBD_XP->pfDataBase, "%3d%3d%3d", -1, -1, -1))
            {
                MM_ReleaseDBFHeader(&pBD_XP);
                return 1;
            }
        }
        else
        {
            if (!VSIFPrintfL(pBD_XP->pfDataBase, "%3d%3d%3d",
                             static_cast<int>(colorEntry.c1),
                             static_cast<int>(colorEntry.c2),
                             static_cast<int>(colorEntry.c3)))
            {
                MM_ReleaseDBFHeader(&pBD_XP);
                return 1;
            }
        }
    }

    fclose_and_nullify(&pBD_XP->pfDataBase);
    MM_ReleaseDBFHeader(&pBD_XP);
    return 0;
}

// Writes DBF and REL attribute table in MiraMon format
int MMRBand::WriteColorTableFromRAT(GDALDataset &oSrcDS)
{
    GDALRasterBand *pRasterBand = oSrcDS.GetRasterBand(m_nIBand + 1);
    if (!pRasterBand)
        return 0;

    m_poRAT = pRasterBand->GetDefaultRAT();
    if (!m_poRAT)
        return 0;

    // At least a value and RGB columns are required
    if (m_poRAT->GetColumnCount() < 4)
        return 0;

    if (!m_poRAT->GetRowCount())
        return 0;

    // Getting the columns that can be converted to a MiraMon palette
    int nIValueMinMax = m_poRAT->GetColOfUsage(GFU_MinMax);
    int nIValueMin = m_poRAT->GetColOfUsage(GFU_Min);
    int nIValueMax = m_poRAT->GetColOfUsage(GFU_Max);
    int nIRed = m_poRAT->GetColOfUsage(GFU_Red);
    int nIGreen = m_poRAT->GetColOfUsage(GFU_Green);
    int nIBlue = m_poRAT->GetColOfUsage(GFU_Blue);
    int nIAlpha = m_poRAT->GetColOfUsage(GFU_Alpha);

    // If the RAT has no value type nor RGB colors, MiraMon can't handle that
    // as a color table. MinMax or Min and Max are accepted as values.
    if (!(nIValueMinMax != -1 || (nIValueMin != -1 && nIValueMax != -1) ||
          nIRed == -1 || nIGreen == -1 || nIBlue == -1))
        return 0;

    // Creating DBF table name
    if (!cpl::ends_with(m_osBandFileName, pszExtRaster))
        return 1;

    // Extract .img
    m_osCTName = m_osBandFileName;
    m_osCTName.resize(m_osCTName.size() - strlen(".img"));
    m_osCTName.append("_CT.dbf");

    // Creating DBF
    struct MM_DATA_BASE_XP *pBD_XP =
        MM_CreateDBFHeader(4, MM_JOC_CARAC_UTF8_DBF);
    if (!pBD_XP)
        return 1;

    // Assigning DBF table name
    CPLStrlcpy(pBD_XP->szFileName, m_osCTName, sizeof(pBD_XP->szFileName));

    // Initializing the table
    MM_EXT_DBF_N_RECORDS nPaletteColors =
        static_cast<MM_EXT_DBF_N_RECORDS>(m_poRAT->GetRowCount());
    pBD_XP->nFields = 4;
    pBD_XP->nRecords = nPaletteColors;
    pBD_XP->FirstRecordOffset =
        static_cast<MM_FIRST_RECORD_OFFSET_TYPE>(33 + (pBD_XP->nFields * 32));
    pBD_XP->CharSet = MM_JOC_CARAC_ANSI_DBASE;
    pBD_XP->dbf_version = MM_MARCA_DBASE4;
    pBD_XP->BytesPerRecord = 1;

    MM_ACCUMULATED_BYTES_TYPE_DBF nClauSimbolNBytes = 0;
    do
    {
        nClauSimbolNBytes++;
        nPaletteColors /= 10;
    } while (nPaletteColors > 0);
    nClauSimbolNBytes++;

    // Fields of the DBF table
    struct MM_FIELD MMField;
    MM_InitializeField(&MMField);
    MMField.FieldType = 'N';
    CPLStrlcpy(MMField.FieldName, "CLAUSIMBOL", MM_MAX_LON_FIELD_NAME_DBF);
    MMField.BytesPerField = nClauSimbolNBytes;
    MM_DuplicateFieldDBXP(pBD_XP->pField, &MMField);

    CPLStrlcpy(MMField.FieldName, "R_COLOR", MM_MAX_LON_FIELD_NAME_DBF);
    MMField.BytesPerField = 3;
    MM_DuplicateFieldDBXP(pBD_XP->pField + 1, &MMField);

    CPLStrlcpy(MMField.FieldName, "G_COLOR", MM_MAX_LON_FIELD_NAME_DBF);
    MMField.BytesPerField = 3;
    MM_DuplicateFieldDBXP(pBD_XP->pField + 2, &MMField);

    CPLStrlcpy(MMField.FieldName, "B_COLOR", MM_MAX_LON_FIELD_NAME_DBF);
    MMField.BytesPerField = 3;
    MM_DuplicateFieldDBXP(pBD_XP->pField + 3, &MMField);

    // Opening the table
    if (!MM_CreateAndOpenDBFFile(pBD_XP, pBD_XP->szFileName))
    {
        MM_ReleaseDBFHeader(&pBD_XP);
        return 1;
    }

    // Writing records to the table
    if (0 != VSIFSeekL(pBD_XP->pfDataBase,
                       static_cast<vsi_l_offset>(pBD_XP->FirstRecordOffset),
                       SEEK_SET))
    {
        MM_ReleaseDBFHeader(&pBD_XP);
        return 1;
    }

    int nIRow;
    for (nIRow = 0; nIRow < static_cast<int>(pBD_XP->nRecords); nIRow++)
    {
        if (nIRow > 0 && nIValueMin != -1 && nIValueMax != -1)
        {
            if (m_poRAT->GetValueAsInt(nIRow, nIValueMin) ==
                m_poRAT->GetValueAsInt(nIRow, nIValueMax))
                break;
        }
        // Deletion flag
        if (!VSIFPrintfL(pBD_XP->pfDataBase, " "))
        {
            MM_ReleaseDBFHeader(&pBD_XP);
            return 1;
        }

        if (!VSIFPrintfL(pBD_XP->pfDataBase, "%*d",
                         static_cast<int>(nClauSimbolNBytes), nIRow))
        {
            MM_ReleaseDBFHeader(&pBD_XP);
            return 1;
        }
        if (nIAlpha != -1 && m_poRAT->GetValueAsInt(nIRow, nIAlpha) == 0)
        {
            if (!VSIFPrintfL(pBD_XP->pfDataBase, "%3d%3d%3d", -1, -1, -1))
            {
                MM_ReleaseDBFHeader(&pBD_XP);
                return 1;
            }
        }
        else
        {
            if (!VSIFPrintfL(pBD_XP->pfDataBase, "%3d%3d%3d",
                             m_poRAT->GetValueAsInt(nIRow, nIRed),
                             m_poRAT->GetValueAsInt(nIRow, nIGreen),
                             m_poRAT->GetValueAsInt(nIRow, nIBlue)))
            {
                MM_ReleaseDBFHeader(&pBD_XP);
                return 1;
            }
        }
    }

    // Nodata color
    if (!VSIFPrintfL(pBD_XP->pfDataBase, " "))
    {
        MM_ReleaseDBFHeader(&pBD_XP);
        return 1;
    }
    if (!VSIFPrintfL(pBD_XP->pfDataBase, "%*s",
                     static_cast<int>(nClauSimbolNBytes), ""))
    {
        MM_ReleaseDBFHeader(&pBD_XP);
        return 1;
    }
    if (!VSIFPrintfL(pBD_XP->pfDataBase, "%3d%3d%3d", -1, -1, -1))
    {
        MM_ReleaseDBFHeader(&pBD_XP);
        return 1;
    }
    nIRow++;

    if (nIRow != static_cast<int>(pBD_XP->nRecords))
    {
        pBD_XP->nRecords = nIRow;
        MM_WriteNRecordsMMBD_XPFile(pBD_XP);
    }

    fclose_and_nullify(&pBD_XP->pfDataBase);
    MM_ReleaseDBFHeader(&pBD_XP);
    return 0;
}

// Writes DBF and REL attribute table in MiraMon format
int MMRBand::WriteAttributeTable(GDALDataset &oSrcDS)
{
    GDALRasterBand *pRasterBand = oSrcDS.GetRasterBand(m_nIBand + 1);
    if (!pRasterBand)
        return 0;

    m_poRAT = pRasterBand->GetDefaultRAT();
    if (!m_poRAT)
        return 0;

    if (!m_poRAT->GetColumnCount())
        return 0;

    if (!m_poRAT->GetRowCount())
        return 0;

    // Getting the Value column
    int nIField;
    m_osValue = "";
    int nIValue = m_poRAT->GetColOfUsage(GFU_MinMax);
    // If the RAT has no value type, MiraMon can't handle that
    // as a RAT
    if (nIValue == -1)
    {
        nIValue = m_poRAT->GetColOfUsage(GFU_Min);
        if (nIValue == -1)
        {
            GDALRATFieldType nType = m_poRAT->GetTypeOfCol(0);
            if (nType == GFT_Integer)
                nIValue = 0;
            else
                return 1;
        }
    }

    m_osValue = m_poRAT->GetNameOfCol(nIValue);

    // Creating DBF table name
    if (!cpl::ends_with(m_osBandFileName, pszExtRaster))
        return 1;

    // Extract .img and create DBF file name
    m_osRATDBFName = m_osBandFileName;
    m_osRATDBFName.resize(m_osRATDBFName.size() - strlen(".img"));
    m_osRATDBFName.append("_RAT.dbf");

    // Extract .img and create REL file name
    m_osRATRELName = m_osBandFileName;
    m_osRATRELName.resize(m_osRATRELName.size() - strlen(".img"));
    m_osRATRELName.append("_RAT.rel");

    // Creating DBF
    int nFields = m_poRAT->GetColumnCount();
    struct MM_DATA_BASE_XP *pBD_XP =
        MM_CreateDBFHeader(nFields, MM_JOC_CARAC_UTF8_DBF);
    if (!pBD_XP)
        return 1;

    // Creating a simple REL that allows MiraMon user to
    // document this RAT in case of need.
    auto pRATRel = std::make_unique<MMRRel>(m_osRATRELName);
    if (!pRATRel->OpenRELFile("wb"))
    {
        MM_ReleaseDBFHeader(&pBD_XP);
        return 1;
    }

    pRATRel->AddSectionStart(SECTION_VERSIO);
    pRATRel->AddKeyValue(KEY_Vers, "4");
    pRATRel->AddKeyValue(KEY_SubVers, "3");
    pRATRel->AddSectionEnd();

    pRATRel->AddSectionStart(SECTION_TAULA_PRINCIPAL);
    // Get path relative to REL file
    pRATRel->AddKeyValue(KEY_NomFitxer, CPLGetFilename(m_osRATDBFName));
    pRATRel->AddKeyValue(KEY_TipusRelacio, "RELACIO_N_1");
    pRATRel->AddKeyValue("AssociatRel", m_osValue);
    pRATRel->AddSectionEnd();

    // Assigning DBF table name
    CPLStrlcpy(pBD_XP->szFileName, m_osRATDBFName, sizeof(pBD_XP->szFileName));

    // Initializing the table
    MM_EXT_DBF_N_RECORDS nRecords =
        static_cast<MM_EXT_DBF_N_RECORDS>(m_poRAT->GetRowCount());
    pBD_XP->nFields = nFields;
    pBD_XP->nRecords = nRecords;
    pBD_XP->FirstRecordOffset =
        static_cast<MM_FIRST_RECORD_OFFSET_TYPE>(33 + (pBD_XP->nFields * 32));
    pBD_XP->CharSet = MM_JOC_CARAC_ANSI_DBASE;
    pBD_XP->dbf_version = MM_MARCA_DBASE4;
    pBD_XP->BytesPerRecord = 1;

    // Fields of the DBF table
    struct MM_FIELD MMField;
    for (nIField = 0; nIField < m_poRAT->GetColumnCount(); nIField++)
    {
        // DBF part
        MM_InitializeField(&MMField);
        CPLStrlcpy(MMField.FieldName, m_poRAT->GetNameOfCol(nIField),
                   sizeof(MMField.FieldName));

        MMField.BytesPerField = 0;
        for (int nIRow = 0; nIRow < m_poRAT->GetRowCount(); nIRow++)
        {
            if (m_poRAT->GetTypeOfCol(nIField) == GFT_DateTime)
            {
                MMField.FieldType = 'D';
                MMField.DecimalsIfFloat = 0;
                MMField.BytesPerField = MM_MAX_AMPLADA_CAMP_D_DBF;
                break;
            }
            else if (m_poRAT->GetTypeOfCol(nIField) == GFT_Boolean)
            {
                MMField.FieldType = 'L';
                MMField.DecimalsIfFloat = 0;
                MMField.BytesPerField = 1;
                break;
            }
            else
            {
                if (m_poRAT->GetTypeOfCol(nIField) == GFT_String ||
                    m_poRAT->GetTypeOfCol(nIField) == GFT_WKBGeometry)
                {
                    MMField.FieldType = 'C';
                    MMField.DecimalsIfFloat = 0;
                }
                else
                {
                    MMField.FieldType = 'N';
                    if (m_poRAT->GetTypeOfCol(nIField) == GFT_Real)
                    {
                        MMField.BytesPerField = 20;
                        MMField.DecimalsIfFloat = MAX_RELIABLE_SF_DOUBLE;
                    }
                    else
                        MMField.DecimalsIfFloat = 0;
                }
                char *pszString =
                    CPLRecode(m_poRAT->GetValueAsString(nIRow, nIField),
                              CPL_ENC_UTF8, "CP1252");

                if (strlen(pszString) > MMField.BytesPerField)
                    MMField.BytesPerField =
                        static_cast<MM_BYTES_PER_FIELD_TYPE_DBF>(
                            strlen(pszString));

                CPLFree(pszString);
            }
        }
        MM_DuplicateFieldDBXP(pBD_XP->pField + nIField, &MMField);

        // REL part
        CPLString osSection = SECTION_TAULA_PRINCIPAL;
        osSection.append(":");
        osSection.append(MMField.FieldName);
        pRATRel->AddSectionStart(osSection);

        if (EQUAL(MMField.FieldName, m_osValue))
        {
            pRATRel->AddKeyValue("visible", "0");
            pRATRel->AddKeyValue(KEY_TractamentVariable, "Categoric");
        }
        pRATRel->AddKeyValue(KEY_descriptor, "");
        pRATRel->AddSectionEnd();
    }

    // Closing the REL
    pRATRel->CloseRELFile();

    // Opening the table
    if (!MM_CreateAndOpenDBFFile(pBD_XP, pBD_XP->szFileName))
    {
        MM_ReleaseDBFHeader(&pBD_XP);
        return 1;
    }

    // Writing records to the table
    if (0 != VSIFSeekL(pBD_XP->pfDataBase,
                       static_cast<vsi_l_offset>(pBD_XP->FirstRecordOffset),
                       SEEK_SET))
    {
        MM_ReleaseDBFHeader(&pBD_XP);
        return 1;
    }

    for (int nIRow = 0; nIRow < static_cast<int>(pBD_XP->nRecords); nIRow++)
    {
        // Deletion flag
        if (!VSIFPrintfL(pBD_XP->pfDataBase, " "))
        {
            MM_ReleaseDBFHeader(&pBD_XP);
            return 1;
        }
        for (nIField = 0; nIField < static_cast<int>(pBD_XP->nFields);
             nIField++)
        {
            if (m_poRAT->GetTypeOfCol(nIRow) == GFT_DateTime)
            {
                char szDate[15];
                const GDALRATDateTime osDT =
                    m_poRAT->GetValueAsDateTime(nIRow, nIField);
                if (osDT.nYear >= 0)
                    snprintf(szDate, sizeof(szDate), "%04d%02d%02d", osDT.nYear,
                             osDT.nMonth, osDT.nDay);
                else
                    snprintf(szDate, sizeof(szDate), "%04d%02d%02d", 0, 0, 0);

                if (pBD_XP->pField[nIField].BytesPerField !=
                    VSIFWriteL(szDate, 1, pBD_XP->pField[nIField].BytesPerField,
                               pBD_XP->pfDataBase))
                {
                    MM_ReleaseDBFHeader(&pBD_XP);
                    return 1;
                }
            }
            else if (m_poRAT->GetTypeOfCol(nIRow) == GFT_Boolean)
            {
                if (pBD_XP->pField[nIField].BytesPerField !=
                    VSIFWriteL(m_poRAT->GetValueAsBoolean(nIRow, nIField) ? "T"
                                                                          : "F",
                               1, pBD_XP->pField[nIField].BytesPerField,
                               pBD_XP->pfDataBase))
                {
                    MM_ReleaseDBFHeader(&pBD_XP);
                    return 1;
                }
            }
            else if (m_poRAT->GetTypeOfCol(nIRow) == GFT_WKBGeometry)
            {
                if (pBD_XP->pField[nIField].BytesPerField !=
                    VSIFWriteL(m_poRAT->GetValueAsString(nIRow, nIField), 1,
                               pBD_XP->pField[nIField].BytesPerField,
                               pBD_XP->pfDataBase))
                {
                    MM_ReleaseDBFHeader(&pBD_XP);
                    return 1;
                }
            }
            else
            {
                char *pszString =
                    CPLRecode(m_poRAT->GetValueAsString(nIRow, nIField),
                              CPL_ENC_UTF8, "CP1252");

                if (pBD_XP->pField[nIField].BytesPerField !=
                    VSIFWriteL(pszString, 1,
                               pBD_XP->pField[nIField].BytesPerField,
                               pBD_XP->pfDataBase))
                {
                    MM_ReleaseDBFHeader(&pBD_XP);
                    return 1;
                }
                CPLFree(pszString);
            }
        }
    }

    fclose_and_nullify(&pBD_XP->pfDataBase);
    MM_ReleaseDBFHeader(&pBD_XP);
    return 0;
}

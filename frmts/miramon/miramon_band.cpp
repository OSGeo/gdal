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
    // So le'ts configurate the blocks as line blocks.
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

const CPLString MMRBand::GetRELFileName() const
{
    if (!m_pfRel)
        return "";
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
bool MMRBand::UpdateDataTypeFromREL(const CPLString osSection)
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

    CPLString osAuxSection = SECTION_ATTRIBUTE_DATA;
    osAuxSection.append(":");
    osAuxSection.append(osSection);
    if (m_pfRel->GetMetadataValue(osAuxSection, "min", osValue) &&
        !osValue.empty())
    {
        if (1 == CPLsscanf(osValue, "%lf", &m_dfMin))
            m_bMinSet = true;
    }

    m_bMaxSet = false;
    if (m_pfRel->GetMetadataValue(osAuxSection, "max", osValue) &&
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

    CPLString osAuxSection = SECTION_ATTRIBUTE_DATA;
    osAuxSection.append(":");
    osAuxSection.append(osSection);
    if (m_pfRel->GetMetadataValue(osAuxSection, "unitats", osValue) &&
        !osValue.empty())
    {
        m_osBandUnitType = osValue;
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

    m_pfRel->GetMetadataValue(osTableNameSection, "AssociatRel",
                              m_osAssociateREL);
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

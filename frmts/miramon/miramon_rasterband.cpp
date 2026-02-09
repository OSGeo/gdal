/******************************************************************************
 *
 * Project:  MiraMonRaster driver
 * Purpose:  Implements MMRRasterBand class: responsible for converting the
 *           information stored in an MMRBand into a GDAL RasterBand
 * Author:   Abel Pau
 *
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "miramon_dataset.h"
#include "miramon_rasterband.h"
#include "miramon_band.h"  // Per a MMRBand

#include "../miramon_common/mm_gdal_functions.h"  // For MMCheck_REL_FILE()

/************************************************************************/
/*                           MMRRasterBand()                            */
/************************************************************************/
MMRRasterBand::MMRRasterBand(MMRDataset *poDSIn, int nBandIn)
    : m_pfRel(poDSIn->GetRel())
{
    poDS = poDSIn;
    nBand = nBandIn;

    eAccess = poDSIn->GetAccess();

    nRatOrCT = poDSIn->GetRatOrCT();

    MMRBand *poBand = m_pfRel->GetBand(nBand - 1);
    if (poBand == nullptr)
        return;

    // Getting some band info
    m_osBandSection = poBand->GetBandSection();
    m_eMMRDataTypeMiraMon = poBand->GeteMMDataType();
    m_eMMBytesPerPixel = poBand->GeteMMBytesPerPixel();
    SetUnitType(poBand->GetUnits());
    nBlockXSize = poBand->GetBlockXSize();
    nBlockYSize = poBand->GetBlockYSize();

    UpdateDataType();

    // We have a valid RasterBand.
    m_bIsValid = true;
}

/************************************************************************/
/*                           ~MMRRasterBand()                           */
/************************************************************************/

MMRRasterBand::~MMRRasterBand()

{
    FlushCache(true);
}

/************************************************************************/
/*                           UpdateDataType()                           */
/************************************************************************/
void MMRRasterBand::UpdateDataType()
{
    switch (m_eMMRDataTypeMiraMon)
    {
        case MMDataType::DATATYPE_AND_COMPR_BIT:
        case MMDataType::DATATYPE_AND_COMPR_BYTE:
        case MMDataType::DATATYPE_AND_COMPR_BYTE_RLE:
            eDataType = GDT_UInt8;
            break;

        case MMDataType::DATATYPE_AND_COMPR_UINTEGER:
        case MMDataType::DATATYPE_AND_COMPR_UINTEGER_RLE:
            eDataType = GDT_UInt16;
            break;

        case MMDataType::DATATYPE_AND_COMPR_INTEGER:
        case MMDataType::DATATYPE_AND_COMPR_INTEGER_RLE:
        case MMDataType::DATATYPE_AND_COMPR_INTEGER_ASCII:
            eDataType = GDT_Int16;
            break;

        case MMDataType::DATATYPE_AND_COMPR_LONG:
        case MMDataType::DATATYPE_AND_COMPR_LONG_RLE:
            eDataType = GDT_Int32;
            break;

        case MMDataType::DATATYPE_AND_COMPR_REAL:
        case MMDataType::DATATYPE_AND_COMPR_REAL_RLE:
        case MMDataType::DATATYPE_AND_COMPR_REAL_ASCII:
            eDataType = GDT_Float32;
            break;

        case MMDataType::DATATYPE_AND_COMPR_DOUBLE:
        case MMDataType::DATATYPE_AND_COMPR_DOUBLE_RLE:
            eDataType = GDT_Float64;
            break;

        default:
            eDataType = GDT_UInt8;
            // This should really report an error, but this isn't
            // so easy from within constructors.
            CPLDebug("GDAL", "Unsupported pixel type in MMRRasterBand: %d.",
                     static_cast<int>(m_eMMRDataTypeMiraMon));
            break;
    }
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double MMRRasterBand::GetNoDataValue(int *pbSuccess)

{
    double dfNoData = 0.0;
    if (pbSuccess)
        *pbSuccess = FALSE;

    MMRBand *poBand = m_pfRel->GetBand(nBand - 1);
    if (!poBand)
        return dfNoData;

    if (!poBand->BandHasNoData())
    {
        if (pbSuccess)
            *pbSuccess = FALSE;
        return dfNoData;
    }

    if (pbSuccess)
        *pbSuccess = TRUE;
    return poBand->GetNoDataValue();
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double MMRRasterBand::GetMinimum(int *pbSuccess)

{
    if (pbSuccess)
        *pbSuccess = FALSE;

    MMRBand *poBand = m_pfRel->GetBand(nBand - 1);
    if (!poBand || !poBand->GetMinSet())
        return 0.0;

    if (pbSuccess)
        *pbSuccess = TRUE;

    return poBand->GetMin();
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double MMRRasterBand::GetMaximum(int *pbSuccess)

{
    if (pbSuccess)
        *pbSuccess = FALSE;

    MMRBand *poBand = m_pfRel->GetBand(nBand - 1);
    if (!poBand || !poBand->GetMaxSet())
        return 0.0;

    if (pbSuccess)
        *pbSuccess = TRUE;

    return poBand->GetMax();
}

/************************************************************************/
/*                            GetUnitType()                             */
/************************************************************************/

const char *MMRRasterBand::GetUnitType()

{
    return m_osUnitType.c_str();
}

/************************************************************************/
/*                            SetUnitType()                             */
/************************************************************************/

CPLErr MMRRasterBand::SetUnitType(const char *pszUnit)

{
    if (pszUnit == nullptr)
        m_osUnitType.clear();
    else
        m_osUnitType = pszUnit;

    return CE_None;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr MMRRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff, void *pImage)

{
    CPLErr eErr = CE_None;

    if (nBand < 1 || nBand > m_pfRel->GetNBands())
        return CE_Failure;

    MMRBand *pBand = m_pfRel->GetBand(nBand - 1);
    if (!pBand)
        return CE_Failure;
    eErr = pBand->GetRasterBlock(nBlockXOff, nBlockYOff, pImage,
                                 nBlockXSize * nBlockYSize *
                                     GDALGetDataTypeSizeBytes(eDataType));

    if (eErr == CE_None &&
        m_eMMRDataTypeMiraMon == MMDataType::DATATYPE_AND_COMPR_BIT)
    {
        GByte *pabyData = static_cast<GByte *>(pImage);

        for (int nIAccumulated = nBlockXSize * nBlockYSize - 1;
             nIAccumulated >= 0; nIAccumulated--)
        {
            if ((pabyData[nIAccumulated >> 3] & (1 << (nIAccumulated & 0x7))))
                pabyData[nIAccumulated] = 1;
            else
                pabyData[nIAccumulated] = 0;
        }
    }

    return eErr;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *MMRRasterBand::GetColorTable()
{
    // If user doesn't want the CT, it's skipped
    if (nRatOrCT != RAT_OR_CT::ALL && nRatOrCT != RAT_OR_CT::CT)
        return nullptr;

    if (m_bTriedLoadColorTable)
        return m_poCT.get();

    m_bTriedLoadColorTable = true;

    m_Palette = std::make_unique<MMRPalettes>(*m_pfRel, nBand);

    if (!m_Palette->IsValid())
    {
        m_Palette = nullptr;
        return nullptr;
    }

    m_poCT = std::make_unique<GDALColorTable>();

    /*
    * GDALPaletteInterp
    */

    if (CE_None != UpdateTableColorsFromPalette())
    {
        // No color table available. Perhaps some attribute table with the colors?
        m_poCT = nullptr;
        return m_poCT.get();
    }

    ConvertColorsFromPaletteToColorTable();

    return m_poCT.get();
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp MMRRasterBand::GetColorInterpretation()
{
    GDALColorTable *ct = GetColorTable();

    if (ct)
        return GCI_PaletteIndex;

    return GCI_GrayIndex;
}

/************************************************************************/
/*                           GetDefaultRAT()                            */
/************************************************************************/

GDALRasterAttributeTable *MMRRasterBand::GetDefaultRAT()

{
    // If user doesn't want the RAT, it's skipped
    if (nRatOrCT != RAT_OR_CT::ALL && nRatOrCT != RAT_OR_CT::RAT)
        return nullptr;

    if (m_poDefaultRAT != nullptr)
        return m_poDefaultRAT.get();

    m_poDefaultRAT = std::make_unique<GDALDefaultRasterAttributeTable>();

    if (CE_None != FillRATFromPalette())
    {
        m_poDefaultRAT = nullptr;
    }

    return m_poDefaultRAT.get();
}

CPLErr MMRRasterBand::FillRATFromPalette()

{
    CPLString os_IndexJoin;

    MMRBand *poBand = m_pfRel->GetBand(nBand - 1);
    if (poBand == nullptr)
        return CE_None;

    GetColorTable();
    if (poBand->GetShortRATName().empty() && !m_poCT)
    {
        // I don't have any associated attribute table but
        // perhaps I can create an attribute table with
        // the colors (if I have them and are not at the color table)
        // assigned to the pixels).
        if (CE_None != UpdateAttributeColorsFromPalette())
            return CE_Failure;

        return CE_None;
    }

    // Let's see the conditions to have a RAT
    CPLString osRELName, osDBFName, osAssociateRel;
    if (CE_None != GetRATName(osRELName, osDBFName, osAssociateRel) ||
        osDBFName.empty() || osAssociateRel.empty())
    {
        return CE_Failure;
    }

    // Let's create and fill the RAT
    if (CE_None != CreateRATFromDBF(osRELName, osDBFName, osAssociateRel))
        return CE_Failure;

    return CE_None;
}

CPLErr MMRRasterBand::UpdateAttributeColorsFromPalette()

{
    // If there is no palette, let's get one
    if (!m_Palette)
    {
        m_Palette = std::make_unique<MMRPalettes>(*m_pfRel, nBand);

        if (!m_Palette->IsValid())
        {
            m_Palette = nullptr;
            return CE_None;
        }
    }

    return FromPaletteToAttributeTable();
}

CPLErr MMRRasterBand::CreateRATFromDBF(const CPLString &osRELName,
                                       const CPLString &osDBFName,
                                       const CPLString &osAssociateRel)
{
    // If there is no palette, let's try to get one
    // and try to get a normal RAT.
    if (!m_Palette)
    {
        m_Palette = std::make_unique<MMRPalettes>(*m_pfRel, nBand);

        if (!m_Palette->IsValid() || !m_Palette->IsCategorical())
            m_Palette = nullptr;
    }

    struct MM_DATA_BASE_XP oAttributteTable;
    memset(&oAttributteTable, 0, sizeof(oAttributteTable));

    if (!osRELName.empty())
    {
        if (MM_ReadExtendedDBFHeaderFromFile(
                osDBFName.c_str(), &oAttributteTable, osRELName.c_str()))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Error reading attribute table \"%s\".",
                     osDBFName.c_str());
            return CE_None;
        }
    }
    else
    {
        if (MM_ReadExtendedDBFHeaderFromFile(osDBFName.c_str(),
                                             &oAttributteTable, nullptr))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Error reading attribute table \"%s\".",
                     osDBFName.c_str());
            return CE_None;
        }
    }

    MM_EXT_DBF_N_FIELDS nIField;
    MM_EXT_DBF_N_FIELDS nFieldIndex = oAttributteTable.nFields;
    MM_EXT_DBF_N_FIELDS nCategIndex = oAttributteTable.nFields;
    for (nIField = 0; nIField < oAttributteTable.nFields; nIField++)
    {
        if (EQUAL(oAttributteTable.pField[nIField].FieldName, osAssociateRel))
        {
            nFieldIndex = nIField;
            if (nIField + 1 < oAttributteTable.nFields)
                nCategIndex = nIField + 1;
            else if (nIField > 1)
                nCategIndex = nIField - 1;
            break;
        }
    }

    if (nFieldIndex == oAttributteTable.nFields)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid attribute table: \"%s\"",
                 oAttributteTable.szFileName);
        return CE_Failure;
    }

    if (oAttributteTable.pField[nFieldIndex].FieldType != 'N')
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid attribute table field: \"%s\"",
                 oAttributteTable.szFileName);
        return CE_Failure;
    }

    int nNRATColumns = 0;
    // 0 column: category value
    if (oAttributteTable.pField[nFieldIndex].DecimalsIfFloat)
    {
        if (CE_None != m_poDefaultRAT->CreateColumn(
                           oAttributteTable.pField[nFieldIndex].FieldName,
                           GFT_Real, GFU_MinMax))
            return CE_Failure;

        nNRATColumns++;
    }
    else
    {
        if (CE_None != m_poDefaultRAT->CreateColumn(
                           oAttributteTable.pField[nFieldIndex].FieldName,
                           GFT_Integer, GFU_MinMax))
            return CE_Failure;

        nNRATColumns++;
    }

    GDALRATFieldUsage eFieldUsage;
    GDALRATFieldType eFieldType;

    for (nIField = 0; nIField < oAttributteTable.nFields; nIField++)
    {
        if (nIField == nFieldIndex)
            continue;

        if (oAttributteTable.pField[nIField].FieldType == 'N')
        {
            eFieldUsage = GFU_MinMax;
            if (oAttributteTable.pField[nIField].DecimalsIfFloat)
                eFieldType = GFT_Real;
            else
                eFieldType = GFT_Integer;
        }
        else
        {
            eFieldUsage = GFU_Generic;
            eFieldType = GFT_String;
        }
        if (nIField == nCategIndex)
            eFieldUsage = GFU_Name;

        if (CE_None != m_poDefaultRAT->CreateColumn(
                           oAttributteTable.pField[nIField].FieldName,
                           eFieldType, eFieldUsage))
            return CE_Failure;

        nNRATColumns++;
    }

    VSIFSeekL(oAttributteTable.pfDataBase,
              static_cast<vsi_l_offset>(oAttributteTable.FirstRecordOffset),
              SEEK_SET);
    m_poDefaultRAT->SetRowCount(nNRATColumns);

    MM_ACCUMULATED_BYTES_TYPE_DBF nBufferSize =
        oAttributteTable.BytesPerRecord + 1;
    char *pzsRecord = static_cast<char *>(VSI_CALLOC_VERBOSE(1, nBufferSize));
    if (!pzsRecord)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Out of memory allocating working buffer");
        VSIFCloseL(oAttributteTable.pfDataBase);
        MM_ReleaseMainFields(&oAttributteTable);
    }

    char *pszField = static_cast<char *>(VSI_CALLOC_VERBOSE(1, nBufferSize));
    if (!pszField)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Out of memory allocating working buffer");
        VSIFree(pzsRecord);
        VSIFCloseL(oAttributteTable.pfDataBase);
        MM_ReleaseMainFields(&oAttributteTable);
    }

    for (int nIRecord = 0;
         nIRecord < static_cast<int>(oAttributteTable.nRecords); nIRecord++)
    {
        if (oAttributteTable.BytesPerRecord !=
            VSIFReadL(pzsRecord, sizeof(unsigned char),
                      oAttributteTable.BytesPerRecord,
                      oAttributteTable.pfDataBase))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid attribute table: \"%s\"", osDBFName.c_str());

            VSIFree(pzsRecord);
            VSIFree(pszField);
            VSIFCloseL(oAttributteTable.pfDataBase);
            MM_ReleaseMainFields(&oAttributteTable);
            return CE_Failure;
        }

        // Category index
        memcpy(pszField,
               pzsRecord +
                   oAttributteTable.pField[nFieldIndex].AccumulatedBytes,
               oAttributteTable.pField[nFieldIndex].BytesPerField);
        pszField[oAttributteTable.pField[nFieldIndex].BytesPerField] = '\0';
        CPLString osCatField = pszField;

        int nCatField;
        if (1 != sscanf(osCatField, "%d", &nCatField))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid attribute table: \"%s\"", osDBFName.c_str());

            VSIFree(pzsRecord);
            VSIFree(pszField);
            VSIFCloseL(oAttributteTable.pfDataBase);
            MM_ReleaseMainFields(&oAttributteTable);
            return CE_Failure;
        }
        m_poDefaultRAT->SetValue(nCatField, 0, osCatField.c_str());

        int nIOrderedField = 1;
        for (nIField = 0; nIField < oAttributteTable.nFields; nIField++)
        {
            if (nIField == nFieldIndex)
                continue;

            // Category value
            memcpy(pszField,
                   pzsRecord +
                       oAttributteTable.pField[nIField].AccumulatedBytes,
                   oAttributteTable.pField[nIField].BytesPerField);
            pszField[oAttributteTable.pField[nIField].BytesPerField] = '\0';

            if (oAttributteTable.CharSet == MM_JOC_CARAC_OEM850_DBASE)
                MM_oemansi(pszField);

            CPLString osField = pszField;
            osField.Trim();

            if (oAttributteTable.CharSet != MM_JOC_CARAC_UTF8_DBF &&
                oAttributteTable.pField[nIField].FieldType != 'N')
            {
                // MiraMon encoding is ISO 8859-1 (Latin1) -> Recode to UTF-8
                osField.Recode(CPL_ENC_ISO8859_1, CPL_ENC_UTF8);
            }

            if (1 != sscanf(osCatField, "%d", &nCatField))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid attribute table: \"%s\"", osDBFName.c_str());

                VSIFree(pzsRecord);
                VSIFree(pszField);
                VSIFCloseL(oAttributteTable.pfDataBase);
                MM_ReleaseMainFields(&oAttributteTable);
                return CE_Failure;
            }
            m_poDefaultRAT->SetValue(nCatField, nIOrderedField,
                                     osField.c_str());
            nIOrderedField++;
        }
    }

    VSIFree(pszField);
    VSIFree(pzsRecord);
    VSIFCloseL(oAttributteTable.pfDataBase);
    MM_ReleaseMainFields(&oAttributteTable);

    return CE_None;
}

CPLErr MMRRasterBand::UpdateTableColorsFromPalette()

{
    if (!m_Palette || !m_Palette->IsValid())
        return CE_Failure;

    if (m_Palette->IsConstantColor())
        return AssignUniformColorTable();

    CPLErr peErr;
    if (m_Palette->IsCategorical())
        peErr = FromPaletteToColorTableCategoricalMode();
    else
        peErr = FromPaletteToColorTableContinuousMode();

    return peErr;
}

CPLErr MMRRasterBand::AssignUniformColorTable()

{
    MMRBand *poBand = m_pfRel->GetBand(nBand - 1);
    if (!poBand)
        return CE_Failure;

    // Only for 1 or 2 bytes images
    if (m_eMMBytesPerPixel !=
            MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_BYTE_I_RLE &&
        m_eMMBytesPerPixel !=
            MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_INTEGER_I_RLE)
    {
        return CE_None;
    }

    const int nNPossibleValues = 1
                                 << (8 * static_cast<int>(m_eMMBytesPerPixel));
    for (int iColumn = 0; iColumn < 4; iColumn++)
    {
        try
        {
            m_aadfPCT[iColumn].resize(nNPossibleValues);
        }
        catch (std::bad_alloc &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
            return CE_Failure;
        }
    }

    for (int nITableColor = 0; nITableColor < nNPossibleValues; nITableColor++)
    {
        if (poBand->BandHasNoData() && nITableColor == poBand->GetNoDataValue())
        {
            m_aadfPCT[0][nITableColor] = 0;
            m_aadfPCT[1][nITableColor] = 0;
            m_aadfPCT[2][nITableColor] = 0;
            m_aadfPCT[3][nITableColor] = 0;
        }
        else
        {
            // Before the minimum, we apply the color of the first
            // element (as a placeholder).
            m_aadfPCT[0][nITableColor] = m_Palette->GetConstantColorRGB().c1;
            m_aadfPCT[1][nITableColor] = m_Palette->GetConstantColorRGB().c2;
            m_aadfPCT[2][nITableColor] = m_Palette->GetConstantColorRGB().c3;
            m_aadfPCT[3][nITableColor] = 255;
        }
    }

    return CE_None;
}

// Converts palette Colors to Colors of pixels
CPLErr MMRRasterBand::FromPaletteToColorTableCategoricalMode()

{
    if (!m_Palette)
        return CE_Failure;

    if (!m_Palette->IsCategorical())
        return CE_Failure;

    // If the palette is not loaded, then, ignore the conversion silently
    if (m_Palette->GetSizeOfPaletteColors() == 0)
        return CE_Failure;

    if (m_Palette->GetColorScaling() == ColorTreatment::DEFAULT_SCALING)
        m_Palette->SetColorScaling(ColorTreatment::DIRECT_ASSIGNATION);
    else if (m_Palette->GetColorScaling() != ColorTreatment::DIRECT_ASSIGNATION)
        return CE_Failure;

    // Getting number of color in the palette
    int nNPaletteColors = m_Palette->GetSizeOfPaletteColors();
    int nNPossibleValues;

    if (m_eMMRDataTypeMiraMon != MMDataType::DATATYPE_AND_COMPR_BYTE &&
        m_eMMRDataTypeMiraMon != MMDataType::DATATYPE_AND_COMPR_BYTE_RLE &&
        m_eMMRDataTypeMiraMon != MMDataType::DATATYPE_AND_COMPR_UINTEGER &&
        m_eMMRDataTypeMiraMon != MMDataType::DATATYPE_AND_COMPR_UINTEGER_RLE)
    {
        // Rare case where its a not byte or uinteger img file
        // but it has a categorical palettte.
        nNPossibleValues = nNPaletteColors;
    }
    else
    {
        // To relax Coverity Scan (CID 1620826)
        MMRBand *poBand = m_pfRel->GetBand(nBand - 1);
        if (!poBand)
            return CE_Failure;

        if (m_Palette->IsAutomatic() && poBand->GetMaxSet())
        {
            // In that case (byte, uint) we can limit the number
            // of colours at the maximum value that the band has.
            nNPossibleValues = static_cast<int>(poBand->GetMax()) + 1;
        }
        else
        {
            CPLAssert(static_cast<int>(m_eMMBytesPerPixel) > 0);
            nNPossibleValues = 1 << (8 * static_cast<int>(m_eMMBytesPerPixel));
        }
    }

    for (int iColumn = 0; iColumn < 4; iColumn++)
    {
        try
        {
            m_aadfPCT[iColumn].resize(nNPossibleValues);
        }
        catch (std::bad_alloc &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
            return CE_Failure;
        }
    }

    // No more colors than needed.
    if (nNPaletteColors > nNPossibleValues)
        nNPaletteColors = nNPossibleValues;

    int nIPaletteColor = 0;
    for (nIPaletteColor = 0; nIPaletteColor < nNPaletteColors; nIPaletteColor++)
    {
        m_aadfPCT[0][nIPaletteColor] =
            m_Palette->GetPaletteColorsValue(0, nIPaletteColor);
        m_aadfPCT[1][nIPaletteColor] =
            m_Palette->GetPaletteColorsValue(1, nIPaletteColor);
        m_aadfPCT[2][nIPaletteColor] =
            m_Palette->GetPaletteColorsValue(2, nIPaletteColor);
        m_aadfPCT[3][nIPaletteColor] =
            m_Palette->GetPaletteColorsValue(3, nIPaletteColor);
    }

    // Rest of colors
    for (; nIPaletteColor < nNPossibleValues; nIPaletteColor++)
    {
        m_aadfPCT[0][nIPaletteColor] = m_Palette->GetDefaultColorRGB().c1;
        m_aadfPCT[1][nIPaletteColor] = m_Palette->GetDefaultColorRGB().c2;
        m_aadfPCT[2][nIPaletteColor] = m_Palette->GetDefaultColorRGB().c3;
        m_aadfPCT[3][nIPaletteColor] = m_Palette->GetDefaultColorRGB().c4;
    }

    return CE_None;
}

// Converts paletteColors to Colors of pixels for the Color Table
CPLErr MMRRasterBand::FromPaletteToColorTableContinuousMode()

{
    if (!m_Palette)
        return CE_Failure;

    if (m_Palette->IsCategorical())
        return CE_Failure;

    // TODO: more types of scaling
    if (m_Palette->GetColorScaling() != ColorTreatment::LINEAR_SCALING &&
        m_Palette->GetColorScaling() != ColorTreatment::DIRECT_ASSIGNATION)
        return CE_Failure;

    MMRBand *poBand = m_pfRel->GetBand(nBand - 1);
    if (!poBand)
        return CE_Failure;

    bool bAcceptPalette = false;
    if ((m_eMMRDataTypeMiraMon == MMDataType::DATATYPE_AND_COMPR_BYTE ||
         m_eMMRDataTypeMiraMon == MMDataType::DATATYPE_AND_COMPR_BYTE_RLE) &&
        (m_Palette->GetColorScaling() == ColorTreatment::LINEAR_SCALING ||
         m_Palette->GetColorScaling() == ColorTreatment::DIRECT_ASSIGNATION))
        bAcceptPalette = true;
    else if ((m_eMMRDataTypeMiraMon ==
                  MMDataType::DATATYPE_AND_COMPR_UINTEGER ||
              m_eMMRDataTypeMiraMon ==
                  MMDataType::DATATYPE_AND_COMPR_UINTEGER_RLE) &&
             m_Palette->GetColorScaling() == ColorTreatment::DIRECT_ASSIGNATION)
        bAcceptPalette = true;

    if (!bAcceptPalette)
        return CE_Failure;  // Attribute table

    // Some necessary information
    if (!poBand->GetVisuMinSet() || !poBand->GetVisuMaxSet())
        return CE_Failure;

    // To relax Coverity Scan (CID 1620843)
    CPLAssert(static_cast<int>(m_eMMBytesPerPixel) > 0);

    const int nNPossibleValues = 1
                                 << (8 * static_cast<int>(m_eMMBytesPerPixel));
    for (int iColumn = 0; iColumn < 4; iColumn++)
    {
        try
        {
            m_aadfPCT[iColumn].resize(nNPossibleValues);
        }
        catch (std::bad_alloc &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
            return CE_Failure;
        }
    }

    if (static_cast<int>(m_eMMBytesPerPixel) > 2 &&
        m_Palette->GetNumberOfColors() < nNPossibleValues)
        return CE_Failure;

    if (m_Palette->GetNumberOfColors() < 1)
        return CE_Failure;

    int nFirstValidPaletteIndex;
    unsigned short nIndexColor;
    double dfSlope = 1, dfIntercept = 0;

    if (m_Palette->HasNodata() && m_Palette->GetNoDataPaletteIndex() == 0)
        nFirstValidPaletteIndex = 1;
    else
        nFirstValidPaletteIndex = 0;

    int nIPaletteColorNoData = 0;
    if (static_cast<int>(m_eMMBytesPerPixel) == 2 ||
        m_Palette->GetColorScaling() != ColorTreatment::DIRECT_ASSIGNATION)
    {
        // A scaling is applied between the minimum and maximum display values.
        dfSlope = (static_cast<double>(m_Palette->GetNumberOfColors()) - 1) /
                  (poBand->GetVisuMax() - poBand->GetVisuMin());

        dfIntercept = -dfSlope * poBand->GetVisuMin();

        if (poBand->BandHasNoData())
        {
            if (m_Palette->GetNoDataPaletteIndex() ==
                m_Palette->GetNumberOfColors())
                nIPaletteColorNoData = nNPossibleValues - 1;
        }
    }

    for (int nIPaletteColor = 0; nIPaletteColor < nNPossibleValues;
         nIPaletteColor++)
    {
        if (poBand->BandHasNoData() && nIPaletteColor == nIPaletteColorNoData)
        {
            if (m_Palette->HasNodata())
                AssignRGBColor(nIPaletteColor,
                               m_Palette->GetNoDataPaletteIndex());
            else
                AssignRGBColorDirectly(nIPaletteColor, 255);
        }
        else
        {
            if (nIPaletteColor < static_cast<int>(poBand->GetVisuMin()))
            {
                // Before the minimum, we apply the color of the first
                // element (as a placeholder).
                AssignRGBColor(nIPaletteColor, 0);
            }
            else if (nIPaletteColor <= static_cast<int>(poBand->GetVisuMax()))
            {
                // Between the minimum and maximum, we apply the value
                // read from the table.
                if (static_cast<int>(m_eMMBytesPerPixel) < 2 ||
                    m_Palette->GetColorScaling() ==
                        ColorTreatment::DIRECT_ASSIGNATION)
                {
                    // The value is applied directly.
                    AssignRGBColor(nIPaletteColor, nFirstValidPaletteIndex);
                    nFirstValidPaletteIndex++;
                }
                else
                {
                    // The value is applied according to the scaling.
                    nIndexColor = static_cast<unsigned short>(
                        dfSlope * nIPaletteColor + dfIntercept);
                    if (nIndexColor > m_Palette->GetNumberOfColors())
                        nIndexColor = static_cast<unsigned short>(
                            m_Palette->GetNumberOfColors());
                    AssignRGBColor(nIPaletteColor, nIndexColor);
                }
            }
            else
            {
                // After the maximum, we apply the value of the last
                // element (as a placeholder).
                AssignRGBColor(nIPaletteColor,
                               m_Palette->GetNumberOfColors() - 1);
            }
        }
    }

    return CE_None;
}

CPLErr MMRRasterBand::GetRATName(CPLString &osRELName, CPLString &osDBFName,
                                 CPLString &osAssociateREL)
{
    MMRBand *poBand = m_pfRel->GetBand(nBand - 1);
    if (!poBand)
        return CE_Failure;

    if (poBand->GetShortRATName().empty())
        return CE_None;  // There is no RAT

    CPLString osExtension = CPLGetExtensionSafe(poBand->GetShortRATName());
    if (osExtension.tolower() == "rel")
    {
        // Get path relative to REL file
        osRELName = CPLFormFilenameSafe(
            CPLGetPathSafe(m_pfRel->GetRELNameChar()).c_str(),
            poBand->GetShortRATName(), "");

        // Getting information from the associated REL
        MMRRel localRel(osRELName, false);
        CPLString osShortDBFName;

        if (!localRel.GetMetadataValue(SECTION_TAULA_PRINCIPAL, KEY_NomFitxer,
                                       osShortDBFName) ||
            osShortDBFName.empty())
        {
            osRELName = "";
            return CE_Failure;
        }

        // Get path relative to REL file
        osDBFName = CPLFormFilenameSafe(
            CPLGetPathSafe(localRel.GetRELNameChar()).c_str(), osShortDBFName,
            "");

        if (!localRel.GetMetadataValue(SECTION_TAULA_PRINCIPAL, "AssociatRel",
                                       osAssociateREL) ||
            osAssociateREL.empty())
        {
            osRELName = "";
            return CE_Failure;
        }

        CPLString osSection = SECTION_TAULA_PRINCIPAL;
        osSection.append(":");
        osSection.append(osAssociateREL);

        CPLString osTactVar;

        if (localRel.GetMetadataValue(osSection, KEY_TractamentVariable,
                                      osTactVar) &&
            osTactVar == "Categoric")
            m_poDefaultRAT->SetTableType(GRTT_THEMATIC);
        else
        {
            osRELName = "";
            return CE_Failure;
        }

        return CE_None;
    }

    osExtension = CPLGetExtensionSafe(poBand->GetShortRATName());
    if (osExtension.tolower() == "dbf")
    {
        if (CPLIsFilenameRelative(poBand->GetShortRATName()))
        {
            // Get path relative to REL file
            osDBFName = CPLFormFilenameSafe(
                CPLGetPathSafe(m_pfRel->GetRELNameChar()).c_str(),
                poBand->GetShortRATName(), "");
        }
        else
            osDBFName = poBand->GetShortRATName();

        osAssociateREL = poBand->GetAssociateREL();
        if (osAssociateREL.empty())
        {
            osRELName = "";
            osDBFName = "";
            return CE_Failure;
        }
        m_poDefaultRAT->SetTableType(GRTT_THEMATIC);
        return CE_None;
    }

    osRELName = "";
    osDBFName = "";
    osAssociateREL = "";
    return CE_Failure;
}

// Converts paletteColors to Colors of pixels in the attribute table
CPLErr MMRRasterBand::FromPaletteToAttributeTable()

{
    if (!m_Palette)
        return CE_None;

    // TODO: more types of scaling
    if (m_Palette->GetColorScaling() != ColorTreatment::LINEAR_SCALING &&
        m_Palette->GetColorScaling() != ColorTreatment::DIRECT_ASSIGNATION)
        return CE_Failure;

    MMRBand *poBand = m_pfRel->GetBand(nBand - 1);
    if (!poBand)
        return CE_Failure;

    if (m_Palette->IsConstantColor())
        return FromPaletteToAttributeTableConstant();

    if (m_Palette->GetNumberOfColors() <= 0)
        return CE_Failure;

    if (m_Palette->GetColorScaling() == ColorTreatment::DIRECT_ASSIGNATION)
        return FromPaletteToAttributeTableDirectAssig();

    // A scaling is applied between the minimum and maximum display values.
    return FromPaletteToAttributeTableLinear();
}

CPLErr MMRRasterBand::FromPaletteToAttributeTableConstant()
{
    MMRBand *poBand = m_pfRel->GetBand(nBand - 1);
    if (!poBand)
        return CE_Failure;

    // Some necessary information
    if (!poBand->GetVisuMinSet() || !poBand->GetVisuMaxSet())
        return CE_Failure;

    m_poDefaultRAT->CreateColumn("MIN", GFT_Real, GFU_Min);
    m_poDefaultRAT->CreateColumn("MAX", GFT_Real, GFU_Max);
    m_poDefaultRAT->CreateColumn("Red", GFT_Integer, GFU_Red);
    m_poDefaultRAT->CreateColumn("Green", GFT_Integer, GFU_Green);
    m_poDefaultRAT->CreateColumn("Blue", GFT_Integer, GFU_Blue);

    m_poDefaultRAT->SetTableType(GRTT_THEMATIC);

    int nRow = 0;
    if (poBand->BandHasNoData())
    {
        m_poDefaultRAT->SetRowCount(2);

        m_poDefaultRAT->SetValue(0, 0, poBand->GetNoDataValue());
        m_poDefaultRAT->SetValue(0, 1, poBand->GetNoDataValue());
        m_poDefaultRAT->SetValue(0, 2, m_Palette->GetNoDataDefaultColor().c1);
        m_poDefaultRAT->SetValue(0, 3, m_Palette->GetNoDataDefaultColor().c2);
        m_poDefaultRAT->SetValue(0, 4, m_Palette->GetNoDataDefaultColor().c3);
        nRow++;
    }
    else
        m_poDefaultRAT->SetRowCount(1);

    // Sets the constant color from min to max
    m_poDefaultRAT->SetValue(nRow, 0, poBand->GetVisuMin());
    m_poDefaultRAT->SetValue(nRow, 1, poBand->GetVisuMax());
    m_poDefaultRAT->SetValue(nRow, 2, m_Palette->GetConstantColorRGB().c1);
    m_poDefaultRAT->SetValue(nRow, 3, m_Palette->GetConstantColorRGB().c2);
    m_poDefaultRAT->SetValue(nRow, 4, m_Palette->GetConstantColorRGB().c3);

    return CE_None;
}

CPLErr MMRRasterBand::FromPaletteToAttributeTableDirectAssig()
{
    MMRBand *poBand = m_pfRel->GetBand(nBand - 1);
    if (!poBand)
        return CE_Failure;

    if (!m_Palette)
        return CE_Failure;

    if (m_Palette->GetNumberOfColors() <= 0)
        return CE_Failure;

    m_poDefaultRAT->SetTableType(GRTT_THEMATIC);

    m_poDefaultRAT->CreateColumn("MIN_MAX", GFT_Real, GFU_MinMax);
    m_poDefaultRAT->CreateColumn("Red", GFT_Integer, GFU_Red);
    m_poDefaultRAT->CreateColumn("Green", GFT_Integer, GFU_Green);
    m_poDefaultRAT->CreateColumn("Blue", GFT_Integer, GFU_Blue);

    m_poDefaultRAT->SetRowCount(static_cast<int>(
        m_Palette->GetNumberOfColorsIncludingNodata()));  // +1 for last element

    // Nodata color assignation
    int nIRow = 0;
    if (poBand->BandHasNoData() && m_Palette->HasNodata())
    {
        m_poDefaultRAT->SetValue(nIRow, 0, poBand->GetNoDataValue());
        m_poDefaultRAT->SetValue(nIRow, 1,
                                 m_Palette->GetPaletteColorsValue(
                                     0, m_Palette->GetNoDataPaletteIndex()));
        m_poDefaultRAT->SetValue(nIRow, 2,
                                 m_Palette->GetPaletteColorsValue(
                                     1, m_Palette->GetNoDataPaletteIndex()));
        m_poDefaultRAT->SetValue(nIRow, 3,
                                 m_Palette->GetPaletteColorsValue(
                                     2, m_Palette->GetNoDataPaletteIndex()));
        nIRow++;
    }

    int nIPaletteColor = 0;
    for (; nIPaletteColor < m_Palette->GetNumberOfColors(); nIPaletteColor++)
    {
        if (nIPaletteColor == m_Palette->GetNoDataPaletteIndex())
            continue;

        m_poDefaultRAT->SetValue(nIRow, 0, nIPaletteColor);

        m_poDefaultRAT->SetValue(
            nIRow, 1, m_Palette->GetPaletteColorsValue(0, nIPaletteColor));
        m_poDefaultRAT->SetValue(
            nIRow, 2, m_Palette->GetPaletteColorsValue(1, nIPaletteColor));
        m_poDefaultRAT->SetValue(
            nIRow, 3, m_Palette->GetPaletteColorsValue(2, nIPaletteColor));

        nIRow++;
    }

    return CE_None;
}

CPLErr MMRRasterBand::FromPaletteToAttributeTableLinear()
{
    MMRBand *poBand = m_pfRel->GetBand(nBand - 1);
    if (!poBand)
        return CE_Failure;

    if (!m_Palette)
        return CE_Failure;

    if (m_Palette->GetNumberOfColors() <= 0)
        return CE_Failure;

    // Some necessary information
    if (!poBand->GetVisuMinSet() || !poBand->GetVisuMaxSet())
        return CE_Failure;

    m_poDefaultRAT->SetTableType(GRTT_ATHEMATIC);
    m_poDefaultRAT->CreateColumn("MIN", GFT_Real, GFU_Min);
    m_poDefaultRAT->CreateColumn("MAX", GFT_Real, GFU_Max);
    m_poDefaultRAT->CreateColumn("Red", GFT_Integer, GFU_Red);
    m_poDefaultRAT->CreateColumn("Green", GFT_Integer, GFU_Green);
    m_poDefaultRAT->CreateColumn("Blue", GFT_Integer, GFU_Blue);

    m_poDefaultRAT->SetRowCount(m_Palette->GetNumberOfColorsIncludingNodata() +
                                1);  // +1 for last element

    // Nodata color assignation
    int nIRow = 0;
    if (poBand->BandHasNoData() && m_Palette->HasNodata())
    {
        m_poDefaultRAT->SetValue(nIRow, 0, poBand->GetNoDataValue());
        m_poDefaultRAT->SetValue(nIRow, 1, poBand->GetNoDataValue());
        m_poDefaultRAT->SetValue(nIRow, 2,
                                 m_Palette->GetPaletteColorsValue(
                                     0, m_Palette->GetNoDataPaletteIndex()));
        m_poDefaultRAT->SetValue(nIRow, 3,
                                 m_Palette->GetPaletteColorsValue(
                                     1, m_Palette->GetNoDataPaletteIndex()));
        m_poDefaultRAT->SetValue(nIRow, 4,
                                 m_Palette->GetPaletteColorsValue(
                                     2, m_Palette->GetNoDataPaletteIndex()));
        nIRow++;
    }

    double dfInterval =
        (poBand->GetVisuMax() - poBand->GetVisuMin()) /
        (static_cast<double>(m_Palette->GetNumberOfColors()) + 1);

    int nIPaletteColorNoData = 0;
    if (poBand->BandHasNoData())
    {
        if (m_Palette->GetNoDataPaletteIndex() ==
            m_Palette->GetNumberOfColors())
            nIPaletteColorNoData =
                m_Palette->GetNumberOfColorsIncludingNodata();
    }

    bool bFirstIteration = true;
    int nIPaletteColor = 0;
    for (; nIPaletteColor < m_Palette->GetNumberOfColors() - 1;
         nIPaletteColor++)
    {
        if (poBand->BandHasNoData() && m_Palette->HasNodata() &&
            nIPaletteColor == nIPaletteColorNoData)
            continue;
        if (bFirstIteration)
        {
            m_poDefaultRAT->SetValue(
                nIRow, 0, poBand->GetVisuMin() + dfInterval * nIPaletteColor);
        }
        else
        {
            if (IsInteger())
            {
                m_poDefaultRAT->SetValue(
                    nIRow, 0,
                    ceil(poBand->GetVisuMin() + dfInterval * nIPaletteColor));
            }
            else
            {
                m_poDefaultRAT->SetValue(nIRow, 0,
                                         poBand->GetVisuMin() +
                                             dfInterval * nIPaletteColor);
            }
        }
        bFirstIteration = false;

        if (IsInteger())
        {
            m_poDefaultRAT->SetValue(
                nIRow, 1,
                ceil(poBand->GetVisuMin() +
                     dfInterval * (static_cast<double>(nIPaletteColor) + 1)));
        }
        else
        {
            m_poDefaultRAT->SetValue(
                nIRow, 1,
                poBand->GetVisuMin() +
                    dfInterval * (static_cast<double>(nIPaletteColor) + 1));
        }

        m_poDefaultRAT->SetValue(
            nIRow, 2, m_Palette->GetPaletteColorsValue(0, nIPaletteColor));
        m_poDefaultRAT->SetValue(
            nIRow, 3, m_Palette->GetPaletteColorsValue(1, nIPaletteColor));
        m_poDefaultRAT->SetValue(
            nIRow, 4, m_Palette->GetPaletteColorsValue(2, nIPaletteColor));

        nIRow++;
    }

    // Last interval
    if (IsInteger())
    {
        m_poDefaultRAT->SetValue(
            nIRow, 0,
            ceil(
                poBand->GetVisuMin() +
                dfInterval *
                    (static_cast<double>(m_Palette->GetNumberOfColors()) - 1)));
    }
    else
    {
        m_poDefaultRAT->SetValue(
            nIRow, 0,
            poBand->GetVisuMin() +
                dfInterval *
                    (static_cast<double>(m_Palette->GetNumberOfColors()) - 1));
    }
    m_poDefaultRAT->SetValue(nIRow, 1, poBand->GetVisuMax());
    m_poDefaultRAT->SetValue(
        nIRow, 2, m_Palette->GetPaletteColorsValue(0, nIPaletteColor - 1));
    m_poDefaultRAT->SetValue(
        nIRow, 3, m_Palette->GetPaletteColorsValue(1, nIPaletteColor - 1));
    m_poDefaultRAT->SetValue(
        nIRow, 4, m_Palette->GetPaletteColorsValue(2, nIPaletteColor - 1));

    nIRow++;

    // Last value
    m_poDefaultRAT->SetValue(nIRow, 0, poBand->GetVisuMax());
    m_poDefaultRAT->SetValue(nIRow, 1, poBand->GetVisuMax());
    m_poDefaultRAT->SetValue(
        nIRow, 2, m_Palette->GetPaletteColorsValue(0, nIPaletteColor - 1));
    m_poDefaultRAT->SetValue(
        nIRow, 3, m_Palette->GetPaletteColorsValue(1, nIPaletteColor - 1));
    m_poDefaultRAT->SetValue(
        nIRow, 4, m_Palette->GetPaletteColorsValue(2, nIPaletteColor - 1));

    return CE_None;
}

void MMRRasterBand::ConvertColorsFromPaletteToColorTable()
{
    int nColors = static_cast<int>(GetPCT_Red().size());

    if (nColors > 0)
    {
        for (int iColor = 0; iColor < nColors; iColor++)
        {
            GDALColorEntry sEntry = {
                static_cast<short int>(GetPCT_Red()[iColor]),
                static_cast<short int>(GetPCT_Green()[iColor]),
                static_cast<short int>(GetPCT_Blue()[iColor]),
                static_cast<short int>(GetPCT_Alpha()[iColor])};

            if ((sEntry.c1 < 0 || sEntry.c1 > 255) ||
                (sEntry.c2 < 0 || sEntry.c2 > 255) ||
                (sEntry.c3 < 0 || sEntry.c3 > 255) ||
                (sEntry.c4 < 0 || sEntry.c4 > 255))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Color table entry appears to be corrupt, skipping "
                         "the rest. ");
                break;
            }

            m_poCT->SetColorEntry(iColor, &sEntry);
        }
    }
}

void MMRRasterBand::AssignRGBColor(int nIndexDstCT, int nIndexSrcPalette)
{
    m_aadfPCT[0][nIndexDstCT] =
        m_Palette->GetPaletteColorsValue(0, nIndexSrcPalette);
    m_aadfPCT[1][nIndexDstCT] =
        m_Palette->GetPaletteColorsValue(1, nIndexSrcPalette);
    m_aadfPCT[2][nIndexDstCT] =
        m_Palette->GetPaletteColorsValue(2, nIndexSrcPalette);
    m_aadfPCT[3][nIndexDstCT] =
        m_Palette->GetPaletteColorsValue(3, nIndexSrcPalette);
}

void MMRRasterBand::AssignRGBColorDirectly(int nIndexDstCT, double dfValue)
{
    m_aadfPCT[0][nIndexDstCT] = dfValue;
    m_aadfPCT[1][nIndexDstCT] = dfValue;
    m_aadfPCT[2][nIndexDstCT] = dfValue;
    m_aadfPCT[3][nIndexDstCT] = dfValue;
}

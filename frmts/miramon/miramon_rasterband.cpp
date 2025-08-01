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
    : pfRel(poDSIn->GetRel())
{
    poDS = poDSIn;
    nBand = nBandIn;

    eAccess = poDSIn->GetAccess();

    MMRBand *poBand = pfRel->GetBand(nBand - 1);
    if (poBand == nullptr)
        return;

    // Getting some band info
    osBandSection = poBand->GetBandSection();
    eMMRDataTypeMiraMon = poBand->GeteMMDataType();
    eMMBytesPerPixel = poBand->GeteMMBytesPerPixel();
    nBlockXSize = poBand->GetBlockXSize();
    nBlockYSize = poBand->GetBlockYSize();

    UpdateDataType();

    // We have a valid RasterBand.
    bIsValid = true;
}

/************************************************************************/
/*                           ~MMRRasterBand()                           */
/************************************************************************/

MMRRasterBand::~MMRRasterBand()

{
    FlushCache(true);

    if (poCT != nullptr)
    {
        delete poCT;
        poCT = nullptr;
    }

    if (Palette != nullptr)
        delete Palette;

    if (poDefaultRAT != nullptr)
        delete poDefaultRAT;
}

/************************************************************************/
/*                             UpdateDataType()                         */
/************************************************************************/
void MMRRasterBand::UpdateDataType()
{
    switch (eMMRDataTypeMiraMon)
    {
        case MMDataType::DATATYPE_AND_COMPR_BIT:
        case MMDataType::DATATYPE_AND_COMPR_BYTE:
        case MMDataType::DATATYPE_AND_COMPR_BYTE_RLE:
            eDataType = GDT_Byte;
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
            eDataType = GDT_Byte;
            // This should really report an error, but this isn't
            // so easy from within constructors.
            CPLDebug("GDAL", "Unsupported pixel type in MMRRasterBand: %d.",
                     static_cast<int>(eMMRDataTypeMiraMon));
            break;
    }
}

/************************************************************************/
/*                             GetNoDataValue()                         */
/************************************************************************/

double MMRRasterBand::GetNoDataValue(int *pbSuccess)

{
    double dfNoData = 0.0;
    if (pbSuccess)
        *pbSuccess = FALSE;

    MMRBand *poBand = pfRel->GetBand(nBand - 1);
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

    MMRBand *poBand = pfRel->GetBand(nBand - 1);
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

    MMRBand *poBand = pfRel->GetBand(nBand - 1);
    if (!poBand || !poBand->GetMaxSet())
        return 0.0;

    if (pbSuccess)
        *pbSuccess = TRUE;

    return poBand->GetMax();
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr MMRRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff, void *pImage)

{
    CPLErr eErr = CE_None;

    if (nBand < 1 || nBand > pfRel->GetNBands())
        return CE_Failure;

    MMRBand *pBand = pfRel->GetBand(nBand - 1);
    if (!pBand)
        return CE_Failure;
    eErr = pBand->GetRasterBlock(nBlockXOff, nBlockYOff, pImage,
                                 nBlockXSize * nBlockYSize *
                                     GDALGetDataTypeSizeBytes(eDataType));

    if (eErr == CE_None &&
        eMMRDataTypeMiraMon == MMDataType::DATATYPE_AND_COMPR_BIT)
    {
        GByte *pabyData = static_cast<GByte *>(pImage);

        for (int nIAcumulated = nBlockXSize * nBlockYSize - 1;
             nIAcumulated >= 0; nIAcumulated--)
        {
            if ((pabyData[nIAcumulated >> 3] & (1 << (nIAcumulated & 0x7))))
                pabyData[nIAcumulated] = 1;
            else
                pabyData[nIAcumulated] = 0;
        }
    }

    return eErr;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *MMRRasterBand::GetColorTable()
{
    if (bTriedLoadColorTable)
        return poCT;

    bTriedLoadColorTable = true;

    Palette = new MMRPalettes(*pfRel, osBandSection);

    if (!Palette->IsValid())
    {
        delete Palette;
        Palette = nullptr;
        return nullptr;
    }

    poCT = new GDALColorTable();

    /*
    * GDALPaletteInterp
    */

    if (CE_None != UpdateTableColorsFromPalette())
    {
        // No color table available. Perhaps some attribute table with the colors?
        delete poCT;
        poCT = nullptr;
        return poCT;
    }

    ConvertColorsFromPaletteToColorTable();

    return poCT;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp MMRRasterBand::GetColorInterpretation()
{
    return GCI_PaletteIndex;
}

/************************************************************************/
/*                           GetDefaultRAT()                            */
/************************************************************************/

GDALRasterAttributeTable *MMRRasterBand::GetDefaultRAT()

{
    if (poDefaultRAT != nullptr)
        return poDefaultRAT;

    poDefaultRAT = new GDALDefaultRasterAttributeTable();

    if (CE_None != FillRATFromPalette())
    {
        delete poDefaultRAT;
        poDefaultRAT = nullptr;
    }

    return poDefaultRAT;
}

CPLErr MMRRasterBand::FillRATFromPalette()

{
    CPLString os_IndexJoin;

    if (!pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, osBandSection,
                                 "IndexsJoinTaula", os_IndexJoin) ||
        os_IndexJoin.empty())
    {
        // I don't have any associated attribute table but
        // perhaps I can create an attribute table with
        // the colors (if I have them and are not at the color table)
        // assigned to the pixels).
        if (CE_None != UpdateAttributeColorsFromPalette())
            return CE_Failure;

        return CE_None;
    }

    // Here I have some attribute table. I want to expose to RAT.
    char **papszTokens = CSLTokenizeString2(os_IndexJoin, ",", 0);
    const int nTokens = CSLCount(papszTokens);

    if (nTokens < 1)
    {
        CSLDestroy(papszTokens);
        return CE_Failure;
    }

    // Let's see the conditions to have one.
    CPLString osRELName, osDBFName, osAssociateRel;
    if (CE_None !=
            GetRATName(papszTokens[0], osRELName, osDBFName, osAssociateRel) ||
        osDBFName.empty() || osAssociateRel.empty())
    {
        CSLDestroy(papszTokens);
        return CE_Failure;
    }

    // Let's create and fill the RAT
    if (CE_None != CreateRATFromDBF(osRELName, osDBFName, osAssociateRel))
    {
        CSLDestroy(papszTokens);
        return CE_Failure;
    }
    CSLDestroy(papszTokens);
    return CE_None;
}

CPLErr MMRRasterBand::UpdateAttributeColorsFromPalette()

{
    // If there is no palette, let's get one
    if (!Palette)
    {
        Palette = new MMRPalettes(*pfRel, osBandSection);

        if (!Palette->IsValid())
        {
            delete Palette;
            Palette = nullptr;
            return CE_None;
        }
    }

    return FromPaletteToAttributeTable();
}

CPLErr MMRRasterBand::CreateRATFromDBF(CPLString osRELName, CPLString osDBFName,
                                       CPLString osAssociateRel)
{
    // If there is no palette, let's get one
    if (!Palette)
    {
        Palette = new MMRPalettes(*pfRel, osBandSection);

        if (!Palette->IsValid())
        {
            delete Palette;
            Palette = nullptr;
            return CE_None;
        }
    }

    if (!Palette->IsCategorical())
        return CE_Failure;

    struct MM_DATA_BASE_XP oAttributteTable;
    memset(&oAttributteTable, 0, sizeof(oAttributteTable));

    if (!osRELName.empty())
    {
        if (MM_ReadExtendedDBFHeaderFromFile(
                osDBFName.c_str(), &oAttributteTable,
                static_cast<const char *>(osRELName)))
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
        if (CE_None != poDefaultRAT->CreateColumn(
                           oAttributteTable.pField[nFieldIndex].FieldName,
                           GFT_Real, GFU_MinMax))
            return CE_Failure;

        nNRATColumns++;
    }
    else
    {
        if (CE_None != poDefaultRAT->CreateColumn(
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

        if (CE_None != poDefaultRAT->CreateColumn(
                           oAttributteTable.pField[nIField].FieldName,
                           eFieldType, eFieldUsage))
            return CE_Failure;

        nNRATColumns++;
    }

    VSIFSeekL(oAttributteTable.pfDataBase, oAttributteTable.FirstRecordOffset,
              SEEK_SET);
    poDefaultRAT->SetRowCount(nNRATColumns);

    MM_ACCUMULATED_BYTES_TYPE_DBF nBufferSize =
        oAttributteTable.BytesPerRecord + 1;
    char *pzsRecord = static_cast<char *>(VSI_CALLOC_VERBOSE(1, nBufferSize));
    char *pszField = static_cast<char *>(VSI_CALLOC_VERBOSE(1, nBufferSize));

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
            return CE_Failure;
        }
        poDefaultRAT->SetValue(nCatField, 0, osCatField);

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
                return CE_Failure;
            }
            poDefaultRAT->SetValue(nCatField, nIOrderedField, osField);
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
    if (!Palette)
        return CE_Failure;

    if (Palette->IsConstantColor())
        return AssignUniformColorTable();

    CPLString os_Color_Paleta;

    if (!pfRel->GetMetadataValue(SECTION_COLOR_TEXT, osBandSection,
                                 "Color_Paleta", os_Color_Paleta) ||
        os_Color_Paleta.empty() || os_Color_Paleta == "<Automatic>")
        return CE_Failure;

    CPLErr peErr;
    if (Palette->IsCategorical())
        peErr = FromPaletteToColorTableCategoricalMode();
    else
        peErr = FromPaletteToColorTableContinousMode();

    return peErr;
}

CPLErr MMRRasterBand::AssignUniformColorTable()

{
    MMRBand *poBand = pfRel->GetBand(nBand - 1);
    if (!poBand)
        return CE_Failure;

    // Only for 1 or 2 bytes images
    if (eMMBytesPerPixel != MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_BYTE_I_RLE &&
        eMMBytesPerPixel != MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_INTEGER_I_RLE)
    {
        return CE_None;
    }

    int nNPossibleValues = static_cast<int>(
        pow(2, static_cast<double>(8) * static_cast<int>(eMMBytesPerPixel)));
    for (int iColumn = 0; iColumn < 4; iColumn++)
    {
        try
        {
            aadfPCT[iColumn].resize(nNPossibleValues);
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
            aadfPCT[0][nITableColor] = 0;
            aadfPCT[1][nITableColor] = 0;
            aadfPCT[2][nITableColor] = 0;
            aadfPCT[3][nITableColor] = 0;
        }
        else
        {
            // Before the minimum, we apply the color of the first
            // element (as a placeholder).
            aadfPCT[0][nITableColor] = Palette->GetConstantColorRGB().c1;
            aadfPCT[1][nITableColor] = Palette->GetConstantColorRGB().c2;
            aadfPCT[2][nITableColor] = Palette->GetConstantColorRGB().c3;
            aadfPCT[3][nITableColor] = 255;
        }
    }

    return CE_None;
}

// Converts pallete Colors to Colors of pixels
CPLErr MMRRasterBand::FromPaletteToColorTableCategoricalMode()

{
    if (!Palette)
        return CE_Failure;

    if (!Palette->IsCategorical())
        return CE_Failure;

    // If the palette is not loaded, then, ignore the conversion silently
    if (Palette->GetSizeOfPaletteColors() == 0)
        return CE_Failure;

    if (Palette->ColorScaling == ColorTreatment::DEFAULT_SCALING)
        Palette->ColorScaling = ColorTreatment::DIRECT_ASSIGNATION;
    else if (Palette->ColorScaling != ColorTreatment::DIRECT_ASSIGNATION)
        return CE_Failure;

    // Getting number of color in the palette
    int nNPaletteColors = Palette->GetSizeOfPaletteColors();
    int nNPossibleValues;

    if (eMMRDataTypeMiraMon != MMDataType::DATATYPE_AND_COMPR_BYTE &&
        eMMRDataTypeMiraMon != MMDataType::DATATYPE_AND_COMPR_BYTE_RLE &&
        eMMRDataTypeMiraMon != MMDataType::DATATYPE_AND_COMPR_UINTEGER &&
        eMMRDataTypeMiraMon != MMDataType::DATATYPE_AND_COMPR_UINTEGER_RLE)
    {
        // Rare case where its a not byte or uinteger img file
        // but it has a categorical palettte.
        nNPossibleValues = nNPaletteColors;
    }
    else
    {
        nNPossibleValues = static_cast<int>(pow(
            2, static_cast<double>(8) * static_cast<int>(eMMBytesPerPixel)));
    }

    for (int iColumn = 0; iColumn < 4; iColumn++)
    {
        try
        {
            aadfPCT[iColumn].resize(nNPossibleValues);
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
        aadfPCT[0][nIPaletteColor] =
            Palette->GetPaletteColorsValue(0, nIPaletteColor);
        aadfPCT[1][nIPaletteColor] =
            Palette->GetPaletteColorsValue(1, nIPaletteColor);
        aadfPCT[2][nIPaletteColor] =
            Palette->GetPaletteColorsValue(2, nIPaletteColor);
        aadfPCT[3][nIPaletteColor] =
            Palette->GetPaletteColorsValue(3, nIPaletteColor);
    }

    // Rest of colors
    for (; nIPaletteColor < nNPossibleValues; nIPaletteColor++)
    {
        aadfPCT[0][nIPaletteColor] = Palette->GetDefaultColorRGB().c1;
        aadfPCT[1][nIPaletteColor] = Palette->GetDefaultColorRGB().c2;
        aadfPCT[2][nIPaletteColor] = Palette->GetDefaultColorRGB().c3;
        aadfPCT[3][nIPaletteColor] = Palette->GetDefaultColorRGB().c4;
    }

    return CE_None;
}

// Converts palleteColors to Colors of pixels for the Color Table
CPLErr MMRRasterBand::FromPaletteToColorTableContinousMode()

{
    if (!Palette)
        return CE_Failure;

    if (Palette->IsCategorical())
        return CE_Failure;

    // TODO: more types of scaling
    if (Palette->ColorScaling != ColorTreatment::LINEAR_SCALING)
        return CE_Failure;

    MMRBand *poBand = pfRel->GetBand(nBand - 1);
    if (!poBand)
        return CE_Failure;

    if (eMMRDataTypeMiraMon != MMDataType::DATATYPE_AND_COMPR_BYTE &&
        eMMRDataTypeMiraMon != MMDataType::DATATYPE_AND_COMPR_BYTE_RLE &&
        eMMRDataTypeMiraMon != MMDataType::DATATYPE_AND_COMPR_UINTEGER &&
        eMMRDataTypeMiraMon != MMDataType::DATATYPE_AND_COMPR_UINTEGER_RLE)
        return CE_Failure;  // Attribute table

    // Some necessary information
    if (!poBand->GetVisuMinSet() || !poBand->GetVisuMaxSet())
        return CE_Failure;

    int nNPossibleValues = static_cast<int>(
        pow(2, static_cast<double>(8) * static_cast<int>(eMMBytesPerPixel)));
    for (int iColumn = 0; iColumn < 4; iColumn++)
    {
        try
        {
            aadfPCT[iColumn].resize(nNPossibleValues);
        }
        catch (std::bad_alloc &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
            return CE_Failure;
        }
    }

    if (static_cast<int>(eMMBytesPerPixel) > 2 &&
        Palette->GetNumberOfColors() < nNPossibleValues)
        return CE_Failure;

    if (Palette->GetNumberOfColors() < 1)
        return CE_Failure;

    int nFirstValidPaletteIndex;
    unsigned short nIndexColor;
    double dfSlope = 1, dfIntercept = 0;

    if (Palette->HasNodata() && Palette->GetNoDataPaletteIndex() == 0)
        nFirstValidPaletteIndex = 1;
    else
        nFirstValidPaletteIndex = 0;

    int nIPaletteColorNoData = 0;
    if (static_cast<int>(eMMBytesPerPixel) == 2)
    {
        // A scaling is applied between the minimum and maximum display values.
        dfSlope = (static_cast<double>(Palette->GetNumberOfColors()) - 1) /
                  (poBand->GetVisuMax() - poBand->GetVisuMin());

        dfIntercept = -dfSlope * poBand->GetVisuMin();

        if (poBand->BandHasNoData())
        {
            if (Palette->GetNoDataPaletteIndex() ==
                Palette->GetNumberOfColors())
                nIPaletteColorNoData = nNPossibleValues - 1;
        }
    }

    for (int nIPaletteColor = 0; nIPaletteColor < nNPossibleValues;
         nIPaletteColor++)
    {
        if (poBand->BandHasNoData() && nIPaletteColor == nIPaletteColorNoData)
        {
            if (Palette->HasNodata())
                AssignRGBColor(nIPaletteColor,
                               Palette->GetNoDataPaletteIndex());
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
                if (static_cast<int>(eMMBytesPerPixel) < 2)
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
                    if (nIndexColor > Palette->GetNumberOfColors())
                        nIndexColor = static_cast<unsigned short>(
                            Palette->GetNumberOfColors());
                    AssignRGBColor(nIPaletteColor, nIndexColor);
                }
            }
            else
            {
                // After the maximum, we apply the value of the last
                // element (as a placeholder).
                AssignRGBColor(nIPaletteColor,
                               Palette->GetNumberOfColors() - 1);
            }
        }
    }

    return CE_None;
}

CPLErr MMRRasterBand::GetRATName(char *papszToken, CPLString &osRELName,
                                 CPLString &osDBFName,
                                 CPLString &osAssociateREL)
{
    CPLString os_Join = "JoinTaula";
    os_Join.append("_");
    os_Join.append(papszToken);

    CPLString osTableNameSection_value;

    if (!pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, osBandSection, os_Join,
                                 osTableNameSection_value) ||
        osTableNameSection_value.empty())
        return CE_Failure;  // No attribute available

    CPLString osTableNameSection = "TAULA_";
    osTableNameSection.append(osTableNameSection_value);

    CPLString osShortRELName;

    if (!pfRel->GetMetadataValue(osTableNameSection, "NomFitxer",
                                 osShortRELName) ||
        osShortRELName.empty())
    {
        osRELName = "";
        osAssociateREL = "";
        return CE_Failure;
    }

    CPLString osExtension = CPLGetExtensionSafe(osShortRELName);
    if (osExtension.tolower() == "rel")
    {
        // Get path relative to REL file
        osRELName =
            CPLFormFilenameSafe(CPLGetPathSafe(pfRel->GetRELNameChar()).c_str(),
                                osShortRELName, "");

        // Getting information from the associated REL
        MMRRel *fLocalRel = new MMRRel(osRELName, false);
        CPLString osShortDBFName;

        if (!fLocalRel->GetMetadataValue("TAULA_PRINCIPAL", "NomFitxer",
                                         osShortDBFName) ||
            osShortDBFName.empty())
        {
            osRELName = "";
            delete fLocalRel;
            return CE_Failure;
        }

        // Get path relative to REL file
        osDBFName = CPLFormFilenameSafe(
            CPLGetPathSafe(fLocalRel->GetRELNameChar()).c_str(), osShortDBFName,
            "");

        if (!fLocalRel->GetMetadataValue("TAULA_PRINCIPAL", "AssociatRel",
                                         osAssociateREL) ||
            osAssociateREL.empty())
        {
            osRELName = "";
            delete fLocalRel;
            return CE_Failure;
        }

        CPLString osSection = "TAULA_PRINCIPAL:";
        osSection.append(osAssociateREL);

        CPLString osTactVar;

        if (fLocalRel->GetMetadataValue(osSection, "TractamentVariable",
                                        osTactVar) &&
            osTactVar == "Categoric")
            poDefaultRAT->SetTableType(GRTT_THEMATIC);
        else
        {
            osRELName = "";
            delete fLocalRel;
            return CE_Failure;
        }

        delete fLocalRel;
        return CE_None;
    }

    osExtension = CPLGetExtensionSafe(osShortRELName);
    if (osExtension.tolower() == "dbf")
    {
        // Get path relative to REL file
        osDBFName =
            CPLFormFilenameSafe(CPLGetPathSafe(pfRel->GetRELNameChar()).c_str(),
                                osShortRELName, "");

        if (!pfRel->GetMetadataValue(osTableNameSection, "AssociatRel",
                                     osAssociateREL) ||
            osAssociateREL.empty())
        {
            osRELName = "";
            osAssociateREL = "";
            return CE_Failure;
        }
        poDefaultRAT->SetTableType(GRTT_THEMATIC);
        return CE_None;
    }

    osRELName = "";
    osAssociateREL = "";
    return CE_Failure;
}

// Converts palleteColors to Colors of pixels in the attribute table
CPLErr MMRRasterBand::FromPaletteToAttributeTable()

{
    if (!Palette)
        return CE_None;

    // TODO: more types of scaling
    if (Palette->ColorScaling != ColorTreatment::LINEAR_SCALING &&
        Palette->ColorScaling != ColorTreatment::DIRECT_ASSIGNATION)
        return CE_Failure;

    MMRBand *poBand = pfRel->GetBand(nBand - 1);
    if (!poBand)
        return CE_Failure;

    if (Palette->IsConstantColor())
        return FromPaletteToAttributeTableConstant();

    if (Palette->GetNumberOfColors() <= 0)
        return CE_Failure;

    if (Palette->ColorScaling == ColorTreatment::DIRECT_ASSIGNATION)
        return FromPaletteToAttributeTableDirectAssig();

    // A scaling is applied between the minimum and maximum display values.
    return FromPaletteToAttributeTableLinear();
}

CPLErr MMRRasterBand::FromPaletteToAttributeTableConstant()
{
    MMRBand *poBand = pfRel->GetBand(nBand - 1);
    if (!poBand)
        return CE_Failure;

    // Some necessary information
    if (!poBand->GetVisuMinSet() || !poBand->GetVisuMaxSet())
        return CE_Failure;

    poDefaultRAT->CreateColumn("MIN", GFT_Real, GFU_Min);
    poDefaultRAT->CreateColumn("MAX", GFT_Real, GFU_Max);
    poDefaultRAT->CreateColumn("Red", GFT_Integer, GFU_Red);
    poDefaultRAT->CreateColumn("Green", GFT_Integer, GFU_Green);
    poDefaultRAT->CreateColumn("Blue", GFT_Integer, GFU_Blue);

    poDefaultRAT->SetTableType(GRTT_THEMATIC);

    int nRow = 0;
    if (poBand->BandHasNoData())
    {
        poDefaultRAT->SetRowCount(2);

        poDefaultRAT->SetValue(0, 0, poBand->GetNoDataValue());
        poDefaultRAT->SetValue(0, 1, poBand->GetNoDataValue());
        poDefaultRAT->SetValue(0, 2, Palette->GetNoDataDefaultColor().c1);
        poDefaultRAT->SetValue(0, 3, Palette->GetNoDataDefaultColor().c2);
        poDefaultRAT->SetValue(0, 4, Palette->GetNoDataDefaultColor().c3);
        nRow++;
    }
    else
        poDefaultRAT->SetRowCount(1);

    // Sets the constant color from min to max
    poDefaultRAT->SetValue(nRow, 0, poBand->GetVisuMin());
    poDefaultRAT->SetValue(nRow, 1, poBand->GetVisuMax());
    poDefaultRAT->SetValue(nRow, 2, Palette->GetConstantColorRGB().c1);
    poDefaultRAT->SetValue(nRow, 3, Palette->GetConstantColorRGB().c2);
    poDefaultRAT->SetValue(nRow, 4, Palette->GetConstantColorRGB().c3);

    return CE_None;
}

CPLErr MMRRasterBand::FromPaletteToAttributeTableDirectAssig()
{
    MMRBand *poBand = pfRel->GetBand(nBand - 1);
    if (!poBand)
        return CE_Failure;

    poDefaultRAT->SetTableType(GRTT_THEMATIC);

    poDefaultRAT->CreateColumn("MIN_MAX", GFT_Real, GFU_MinMax);
    poDefaultRAT->CreateColumn("Red", GFT_Integer, GFU_Red);
    poDefaultRAT->CreateColumn("Green", GFT_Integer, GFU_Green);
    poDefaultRAT->CreateColumn("Blue", GFT_Integer, GFU_Blue);

    poDefaultRAT->SetRowCount(static_cast<int>(
        Palette->GetNumberOfColorsIncludingNodata()));  // +1 for last element

    // Nodata color assignation
    int nIRow = 0;
    if (poBand->BandHasNoData() && Palette->HasNodata())
    {
        poDefaultRAT->SetValue(nIRow, 0, poBand->GetNoDataValue());
        poDefaultRAT->SetValue(nIRow, 1,
                               Palette->GetPaletteColorsValue(
                                   0, Palette->GetNoDataPaletteIndex()));
        poDefaultRAT->SetValue(nIRow, 2,
                               Palette->GetPaletteColorsValue(
                                   1, Palette->GetNoDataPaletteIndex()));
        poDefaultRAT->SetValue(nIRow, 3,
                               Palette->GetPaletteColorsValue(
                                   2, Palette->GetNoDataPaletteIndex()));
        nIRow++;
    }

    int nIPaletteColor = 0;
    for (; nIPaletteColor < Palette->GetNumberOfColors(); nIPaletteColor++)
    {
        if (nIPaletteColor == Palette->GetNoDataPaletteIndex())
            continue;

        poDefaultRAT->SetValue(nIRow, 0, nIPaletteColor);

        poDefaultRAT->SetValue(
            nIRow, 1, Palette->GetPaletteColorsValue(0, nIPaletteColor));
        poDefaultRAT->SetValue(
            nIRow, 2, Palette->GetPaletteColorsValue(1, nIPaletteColor));
        poDefaultRAT->SetValue(
            nIRow, 3, Palette->GetPaletteColorsValue(2, nIPaletteColor));

        nIRow++;
    }

    return CE_None;
}

CPLErr MMRRasterBand::FromPaletteToAttributeTableLinear()
{
    MMRBand *poBand = pfRel->GetBand(nBand - 1);
    if (!poBand)
        return CE_Failure;

    // Some necessary information
    if (!poBand->GetVisuMinSet() || !poBand->GetVisuMaxSet())
        return CE_Failure;

    poDefaultRAT->SetTableType(GRTT_ATHEMATIC);
    poDefaultRAT->CreateColumn("MIN", GFT_Real, GFU_Min);
    poDefaultRAT->CreateColumn("MAX", GFT_Real, GFU_Max);
    poDefaultRAT->CreateColumn("Red", GFT_Integer, GFU_Red);
    poDefaultRAT->CreateColumn("Green", GFT_Integer, GFU_Green);
    poDefaultRAT->CreateColumn("Blue", GFT_Integer, GFU_Blue);

    poDefaultRAT->SetRowCount(Palette->GetNumberOfColorsIncludingNodata() +
                              1);  // +1 for last element

    // Nodata color assignation
    int nIRow = 0;
    if (poBand->BandHasNoData() && Palette->HasNodata())
    {
        poDefaultRAT->SetValue(nIRow, 0, poBand->GetNoDataValue());
        poDefaultRAT->SetValue(nIRow, 1, poBand->GetNoDataValue());
        poDefaultRAT->SetValue(nIRow, 2,
                               Palette->GetPaletteColorsValue(
                                   0, Palette->GetNoDataPaletteIndex()));
        poDefaultRAT->SetValue(nIRow, 3,
                               Palette->GetPaletteColorsValue(
                                   1, Palette->GetNoDataPaletteIndex()));
        poDefaultRAT->SetValue(nIRow, 4,
                               Palette->GetPaletteColorsValue(
                                   2, Palette->GetNoDataPaletteIndex()));
        nIRow++;
    }

    double dfInterval = (poBand->GetVisuMax() - poBand->GetVisuMin()) /
                        (static_cast<double>(Palette->GetNumberOfColors()) + 1);

    int nIPaletteColorNoData = 0;
    if (poBand->BandHasNoData())
    {
        if (Palette->GetNoDataPaletteIndex() == Palette->GetNumberOfColors())
            nIPaletteColorNoData = Palette->GetNumberOfColorsIncludingNodata();
    }

    bool bFirstIteration = true;
    int nIPaletteColor = 0;
    for (; nIPaletteColor < Palette->GetNumberOfColors() - 1; nIPaletteColor++)
    {
        if (poBand->BandHasNoData() && Palette->HasNodata() &&
            nIPaletteColor == nIPaletteColorNoData)
            continue;
        if (bFirstIteration)
        {
            poDefaultRAT->SetValue(
                nIRow, 0, poBand->GetVisuMin() + dfInterval * nIPaletteColor);
        }
        else
        {
            if (IsInteger())
            {
                poDefaultRAT->SetValue(
                    nIRow, 0,
                    ceil(poBand->GetVisuMin() + dfInterval * nIPaletteColor));
            }
            else
            {
                poDefaultRAT->SetValue(nIRow, 0,
                                       poBand->GetVisuMin() +
                                           dfInterval * nIPaletteColor);
            }
        }
        bFirstIteration = false;

        if (IsInteger())
        {
            poDefaultRAT->SetValue(
                nIRow, 1,
                ceil(poBand->GetVisuMin() +
                     dfInterval * (static_cast<double>(nIPaletteColor) + 1)));
        }
        else
        {
            poDefaultRAT->SetValue(
                nIRow, 1,
                poBand->GetVisuMin() +
                    dfInterval * (static_cast<double>(nIPaletteColor) + 1));
        }

        poDefaultRAT->SetValue(
            nIRow, 2, Palette->GetPaletteColorsValue(0, nIPaletteColor));
        poDefaultRAT->SetValue(
            nIRow, 3, Palette->GetPaletteColorsValue(1, nIPaletteColor));
        poDefaultRAT->SetValue(
            nIRow, 4, Palette->GetPaletteColorsValue(2, nIPaletteColor));

        nIRow++;
    }

    // Last interval
    if (IsInteger())
    {
        poDefaultRAT->SetValue(
            nIRow, 0,
            ceil(poBand->GetVisuMin() +
                 dfInterval *
                     (static_cast<double>(Palette->GetNumberOfColors()) - 1)));
    }
    else
    {
        poDefaultRAT->SetValue(
            nIRow, 0,
            poBand->GetVisuMin() +
                dfInterval *
                    (static_cast<double>(Palette->GetNumberOfColors()) - 1));
    }
    poDefaultRAT->SetValue(nIRow, 1, poBand->GetVisuMax());
    poDefaultRAT->SetValue(
        nIRow, 2, Palette->GetPaletteColorsValue(0, nIPaletteColor - 1));
    poDefaultRAT->SetValue(
        nIRow, 3, Palette->GetPaletteColorsValue(1, nIPaletteColor - 1));
    poDefaultRAT->SetValue(
        nIRow, 4, Palette->GetPaletteColorsValue(2, nIPaletteColor - 1));

    nIRow++;

    // Last value
    poDefaultRAT->SetValue(nIRow, 0, poBand->GetVisuMax());
    poDefaultRAT->SetValue(nIRow, 1, poBand->GetVisuMax());
    poDefaultRAT->SetValue(
        nIRow, 2, Palette->GetPaletteColorsValue(0, nIPaletteColor - 1));
    poDefaultRAT->SetValue(
        nIRow, 3, Palette->GetPaletteColorsValue(1, nIPaletteColor - 1));
    poDefaultRAT->SetValue(
        nIRow, 4, Palette->GetPaletteColorsValue(2, nIPaletteColor - 1));

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

            poCT->SetColorEntry(iColor, &sEntry);
        }
    }
}

void MMRRasterBand::AssignRGBColor(int nIndexDstCT, int nIndexSrcPalete)
{
    aadfPCT[0][nIndexDstCT] =
        Palette->GetPaletteColorsValue(0, nIndexSrcPalete);
    aadfPCT[1][nIndexDstCT] =
        Palette->GetPaletteColorsValue(1, nIndexSrcPalete);
    aadfPCT[2][nIndexDstCT] =
        Palette->GetPaletteColorsValue(2, nIndexSrcPalete);
    aadfPCT[3][nIndexDstCT] =
        Palette->GetPaletteColorsValue(3, nIndexSrcPalete);
}

void MMRRasterBand::AssignRGBColorDirectly(int nIndexDstCT, double dfValue)
{
    aadfPCT[0][nIndexDstCT] = dfValue;
    aadfPCT[1][nIndexDstCT] = dfValue;
    aadfPCT[2][nIndexDstCT] = dfValue;
    aadfPCT[3][nIndexDstCT] = dfValue;
}

/******************************************************************************
 *
 * Project:  MiraMonRaster driver
 * Purpose:  Implements MMRPalettes class: handles access to a DBF file
 *           containing color information, which is then converted into
 *           either a color table or an attribute table, depending on the
 *           context.
 * Author:   Abel Pau
 * 
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "miramon_rel.h"
#include "miramon_palettes.h"

#include "../miramon_common/mm_gdal_functions.h"  // For MMCheck_REL_FILE()

MMRPalettes::MMRPalettes(MMRRel &fRel, CPLString osBandSectionIn)
    : pfRel(&fRel), osBandSection(osBandSectionIn)
{
    // Is a constant color, and which colors is it?
    CPLString os_Color_Const;
    pfRel->GetMetadataValue(SECTION_COLOR_TEXT, osBandSection, "Color_Const",
                            os_Color_Const);

    if (EQUAL(os_Color_Const, "1"))
    {
        bIsConstantColor = true;
        if (CE_None != UpdateConstantColor())
            return;  // The constant color indicated is wrong
    }

    CPLString os_Color_Paleta;

    if (!pfRel->GetMetadataValue(SECTION_COLOR_TEXT, osBandSection,
                                 "Color_Paleta", os_Color_Paleta) ||
        EQUAL(os_Color_Paleta, "<Automatic>"))
    {
        bIsValid = true;
        ColorScaling = ColorTreatment::DIRECT_ASSIGNATION;
        return;
    }

    // Treatment of the color variable
    CPLString os_Color_TractamentVariable;
    if (!pfRel->GetMetadataValue(SECTION_COLOR_TEXT, osBandSection,
                                 "Color_TractamentVariable",
                                 os_Color_TractamentVariable) ||
        os_Color_TractamentVariable.empty())
    {
        CPLString os_TractamentVariable;
        pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, "TractamentVariable",
                                os_TractamentVariable);
        if (EQUAL(os_TractamentVariable, "Categoric"))
            SetIsCategorical(true);
        else
            SetIsCategorical(false);
    }
    else
    {
        if (EQUAL(os_Color_TractamentVariable, "Categoric"))
            SetIsCategorical(true);
        else
            SetIsCategorical(false);
    }

    UpdateColorInfo();

    CPLString osExtension = CPLGetExtensionSafe(os_Color_Paleta);
    if (osExtension.tolower() == "dbf")
    {
        if (CE_None != GetPaletteColors_DBF(os_Color_Paleta))
            return;

        bIsValid = true;
    }
    else if (osExtension.tolower() == "pal" || osExtension.tolower() == "p25" ||
             osExtension.tolower() == "p65")
    {
        if (CE_None != GetPaletteColors_PAL_P25_P65(os_Color_Paleta))
            return;

        bIsValid = true;
    }
    else
        return;

    nRealNPaletteColors = nNPaletteColors;
    if (HasNodata())
    {
        if (nNPaletteColors < 1)
            return;
        nNPaletteColors--;
    }
    else
    {
        // If palette doesn't have nodata let's set some index
        nNoDataPaletteIndex = nRealNPaletteColors;
    }
}

MMRPalettes::~MMRPalettes()
{
}

void MMRPalettes::AssignColorFromDBF(struct MM_DATA_BASE_XP &oColorTable,
                                     char *pzsRecord, char *pszField,
                                     MM_EXT_DBF_N_FIELDS &nRIndex,
                                     MM_EXT_DBF_N_FIELDS &nGIndex,
                                     MM_EXT_DBF_N_FIELDS &nBIndex,
                                     int nIPaletteIndex)
{
    // RED
    memcpy(pszField, pzsRecord + oColorTable.pField[nRIndex].AccumulatedBytes,
           oColorTable.pField[nRIndex].BytesPerField);
    pszField[oColorTable.pField[nRIndex].BytesPerField] = '\0';
    aadfPaletteColors[0][nIPaletteIndex] = CPLAtof(pszField);

    // GREEN
    memcpy(pszField, pzsRecord + oColorTable.pField[nGIndex].AccumulatedBytes,
           oColorTable.pField[nGIndex].BytesPerField);
    pszField[oColorTable.pField[nGIndex].BytesPerField] = '\0';
    aadfPaletteColors[1][nIPaletteIndex] = CPLAtof(pszField);

    // BLUE
    memcpy(pszField, pzsRecord + oColorTable.pField[nBIndex].AccumulatedBytes,
           oColorTable.pField[nBIndex].BytesPerField);
    pszField[oColorTable.pField[nBIndex].BytesPerField] = '\0';
    aadfPaletteColors[2][nIPaletteIndex] = CPLAtof(pszField);

    // ALPHA
    if (aadfPaletteColors[0][nIPaletteIndex] == -1 &&
        aadfPaletteColors[1][nIPaletteIndex] == -1 &&
        aadfPaletteColors[2][nIPaletteIndex] == -1)
    {
        // Transparent (white or whatever color)
        aadfPaletteColors[0][nIPaletteIndex] = sNoDataColorRGB.c1;
        aadfPaletteColors[1][nIPaletteIndex] = sNoDataColorRGB.c2;
        aadfPaletteColors[2][nIPaletteIndex] = sNoDataColorRGB.c3;
        aadfPaletteColors[3][nIPaletteIndex] = sNoDataColorRGB.c4;
    }
    else
        aadfPaletteColors[3][nIPaletteIndex] = 255;
}

CPLErr MMRPalettes::GetPaletteColors_DBF_Indexs(
    struct MM_DATA_BASE_XP &oColorTable, MM_EXT_DBF_N_FIELDS &nClauSimbol,
    MM_EXT_DBF_N_FIELDS &nRIndex, MM_EXT_DBF_N_FIELDS &nGIndex,
    MM_EXT_DBF_N_FIELDS &nBIndex)
{
    nClauSimbol = oColorTable.nFields;
    nRIndex = oColorTable.nFields;
    nGIndex = oColorTable.nFields;
    nBIndex = oColorTable.nFields;

    for (MM_EXT_DBF_N_FIELDS nIField = 0; nIField < oColorTable.nFields;
         nIField++)
    {
        if (EQUAL(oColorTable.pField[nIField].FieldName, "CLAUSIMBOL"))
            nClauSimbol = nIField;
        else if (EQUAL(oColorTable.pField[nIField].FieldName, "R_COLOR"))
            nRIndex = nIField;
        else if (EQUAL(oColorTable.pField[nIField].FieldName, "G_COLOR"))
            nGIndex = nIField;
        else if (EQUAL(oColorTable.pField[nIField].FieldName, "B_COLOR"))
            nBIndex = nIField;
    }

    if (nClauSimbol == oColorTable.nFields || nRIndex == oColorTable.nFields ||
        nGIndex == oColorTable.nFields || nBIndex == oColorTable.nFields)
        return CE_Failure;

    return CE_None;
}

// Updates nNPaletteColors
CPLErr MMRPalettes::GetPaletteColors_DBF(CPLString os_Color_Paleta_DBF)

{
    // Getting the full path name of the DBF
    CPLString osAux =
        CPLGetPathSafe(static_cast<const char *>(pfRel->GetRELNameChar()));
    CPLString osColorTableFileName =
        CPLFormFilenameSafe(osAux.c_str(), os_Color_Paleta_DBF.c_str(), "");

    // Reading the DBF file
    struct MM_DATA_BASE_XP oColorTable;
    memset(&oColorTable, 0, sizeof(oColorTable));

    if (MM_ReadExtendedDBFHeaderFromFile(
            osColorTableFileName.c_str(), &oColorTable,
            static_cast<const char *>(pfRel->GetRELNameChar())))
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "Invalid color table:"
                 "\"%s\".",
                 osColorTableFileName.c_str());

        return CE_Failure;
    }

    // Getting indices of fields that determine the colors.
    MM_EXT_DBF_N_FIELDS nClauSimbol, nRIndex, nGIndex, nBIndex;
    if (CE_Failure == GetPaletteColors_DBF_Indexs(oColorTable, nClauSimbol,
                                                  nRIndex, nGIndex, nBIndex))
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "Invalid color table:"
                 "\"%s\".",
                 osColorTableFileName.c_str());

        VSIFCloseL(oColorTable.pfDataBase);
        MM_ReleaseMainFields(&oColorTable);
        return CE_Failure;
    }

    // Cheking the structure to be correct
    if (oColorTable.pField[nClauSimbol].BytesPerField == 0 ||
        oColorTable.pField[nRIndex].BytesPerField == 0 ||
        oColorTable.pField[nGIndex].BytesPerField == 0 ||
        oColorTable.pField[nBIndex].BytesPerField == 0 ||
        oColorTable.pField[nClauSimbol].FieldType != 'N' ||
        oColorTable.pField[nRIndex].FieldType != 'N' ||
        oColorTable.pField[nGIndex].FieldType != 'N' ||
        oColorTable.pField[nBIndex].FieldType != 'N')
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "Invalid color table:"
                 "\"%s\".",
                 osColorTableFileName.c_str());

        VSIFCloseL(oColorTable.pfDataBase);
        MM_ReleaseMainFields(&oColorTable);
        return CE_Failure;
    }

    // Guessing or reading the number of colors of the palette.
    MM_ACCUMULATED_BYTES_TYPE_DBF nBufferSize = oColorTable.BytesPerRecord + 1;
    char *pzsRecord = static_cast<char *>(VSI_CALLOC_VERBOSE(1, nBufferSize));
    char *pszField = static_cast<char *>(VSI_CALLOC_VERBOSE(1, nBufferSize));

    nNPaletteColors = static_cast<int>(oColorTable.nRecords);  // Safe cast

    // Checking the size of the palette.
    if (nNPaletteColors < 0 || nNPaletteColors > 65536)
    {
        nNPaletteColors = 0;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid number of colors: %d "
                 "in color table \"%s\".",
                 nNPaletteColors, osColorTableFileName.c_str());

        VSIFree(pszField);
        VSIFree(pzsRecord);
        VSIFCloseL(oColorTable.pfDataBase);
        MM_ReleaseMainFields(&oColorTable);
        return CE_Failure;
    }

    // Getting the memory to allocate the color values
    for (int iColumn = 0; iColumn < 4; iColumn++)
    {
        try
        {
            aadfPaletteColors[iColumn].resize(nNPaletteColors, 0);
        }
        catch (std::bad_alloc &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
            VSIFree(pszField);
            VSIFree(pzsRecord);
            VSIFCloseL(oColorTable.pfDataBase);
            MM_ReleaseMainFields(&oColorTable);
            return CE_Failure;
        }
    }

    VSIFSeekL(oColorTable.pfDataBase, oColorTable.FirstRecordOffset, SEEK_SET);
    /*
        Each record's CLAUSIMBOL field doesn't match a pixel value present in the raster,
        and it's used only for discovering nodata value (blanc value).
        The list of values is used to map every value in a color using:
            - Direct assignation: mode used in categorical modes but possible in continous.
            - Linear scaling
            - Logarithmic scaling
        */
    for (int nIPaletteColors = 0; nIPaletteColors < nNPaletteColors;
         nIPaletteColors++)
    {
        if (oColorTable.BytesPerRecord !=
            VSIFReadL(pzsRecord, sizeof(unsigned char),
                      oColorTable.BytesPerRecord, oColorTable.pfDataBase))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid color table: \"%s\"",
                     osColorTableFileName.c_str());
            VSIFree(pszField);
            VSIFree(pzsRecord);
            VSIFCloseL(oColorTable.pfDataBase);
            MM_ReleaseMainFields(&oColorTable);
            return CE_Failure;
        }

        // Nodata identification
        memcpy(pszField,
               pzsRecord + oColorTable.pField[nClauSimbol].AccumulatedBytes,
               oColorTable.pField[nClauSimbol].BytesPerField);
        pszField[oColorTable.pField[nClauSimbol].BytesPerField] = '\0';
        CPLString osField = pszField;
        osField.replaceAll(" ", "");
        if (osField.empty())  // Nodata value
        {
            bHasNodata = true;
            nNoDataPaletteIndex = nIPaletteColors;
        }

        AssignColorFromDBF(oColorTable, pzsRecord, pszField, nRIndex, nGIndex,
                           nBIndex, nIPaletteColors);
    }

    VSIFree(pszField);
    VSIFree(pzsRecord);
    VSIFCloseL(oColorTable.pfDataBase);
    MM_ReleaseMainFields(&oColorTable);

    return CE_None;
}

// Colors in a PAL, P25 or P65 format files
// Updates nNPaletteColors
CPLErr MMRPalettes::GetPaletteColors_PAL_P25_P65(CPLString os_Color_Paleta_DBF)

{
    CPLString osAux =
        CPLGetPathSafe(static_cast<const char *>(pfRel->GetRELNameChar()));
    CPLString osColorTableFileName =
        CPLFormFilenameSafe(osAux.c_str(), os_Color_Paleta_DBF.c_str(), "");

    // This kind of palette has not NoData color.
    //bHasNodata = false;

    CPLString osExtension = CPLGetExtensionSafe(os_Color_Paleta_DBF);
    int nNReadPaletteColors = 0;
    nNPaletteColors = 0;

    if (osExtension.tolower() == "pal")
        nNPaletteColors = 64;
    else if (osExtension.tolower() == "p25")
        nNPaletteColors = 256;
    else if (osExtension.tolower() == "p65")
        nNPaletteColors = 65536;
    else
        return CE_None;

    for (int iColumn = 0; iColumn < 4; iColumn++)
    {
        try
        {
            aadfPaletteColors[iColumn].resize(nNPaletteColors, 0);
        }
        catch (std::bad_alloc &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
            return CE_Failure;
        }
    }

    VSILFILE *fpColorTable = VSIFOpenL(osColorTableFileName, "rt");
    if (!fpColorTable)
    {
        VSIFCloseL(fpColorTable);
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid color table: \"%s\"",
                 osColorTableFileName.c_str());
        return CE_Failure;
    }

    nNReadPaletteColors = 0;
    const char *pszLine;
    while ((pszLine = CPLReadLineL(fpColorTable)) != nullptr)
    {
        // Ignore empty lines
        if (pszLine[0] == '\0')
            continue;

        char **papszTokens = CSLTokenizeString2(pszLine, " \t", 0);
        if (CSLCount(papszTokens) != 4)
        {
            VSIFCloseL(fpColorTable);
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid color table: \"%s\"",
                     osColorTableFileName.c_str());
            return CE_Failure;
        }

        // Index of the color
        // papszTokens[0] is ignored;

        // RED
        aadfPaletteColors[0][nNReadPaletteColors] = CPLAtof(papszTokens[1]);

        // GREEN
        aadfPaletteColors[1][nNReadPaletteColors] = CPLAtof(papszTokens[2]);

        // BLUE
        aadfPaletteColors[2][nNReadPaletteColors] = CPLAtof(papszTokens[3]);

        // ALPHA
        aadfPaletteColors[3][nNReadPaletteColors] = 255.0;

        CSLDestroy(papszTokens);
        nNReadPaletteColors++;
    }

    // Filling the rest of colors.
    for (int nIColorIndex = nNReadPaletteColors; nIColorIndex < nNPaletteColors;
         nIColorIndex++)
    {
        aadfPaletteColors[0][nNReadPaletteColors] = sDefaultColorRGB.c1;
        aadfPaletteColors[1][nNReadPaletteColors] = sDefaultColorRGB.c2;
        aadfPaletteColors[2][nNReadPaletteColors] = sDefaultColorRGB.c3;
        aadfPaletteColors[3][nNReadPaletteColors] = sDefaultColorRGB.c4;
        nNReadPaletteColors++;
    }

    if (nNReadPaletteColors != nNPaletteColors)
    {
        VSIFCloseL(fpColorTable);
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid color table: \"%s\"",
                 osColorTableFileName.c_str());
        return CE_Failure;
    }

    VSIFCloseL(fpColorTable);

    return CE_None;
}

void MMRPalettes::UpdateColorInfo()
{
    CPLString os_Color_EscalatColor;
    if (pfRel->GetMetadataValue(SECTION_COLOR_TEXT, osBandSection,
                                "Color_EscalatColor", os_Color_EscalatColor) &&
        !os_Color_EscalatColor.empty())
    {
        if (os_Color_EscalatColor.compare("AssigDirecta") == 0)
            ColorScaling = ColorTreatment::DIRECT_ASSIGNATION;
        else if (os_Color_EscalatColor.compare("DespOrigen") == 0)
            ColorScaling = ColorTreatment::ORIGIN_DISPLACEMENT;
        else if (os_Color_EscalatColor.compare("lineal") == 0)
            ColorScaling = ColorTreatment::LINEAR_SCALING;
        else if (os_Color_EscalatColor.compare("log_10") == 0)
            ColorScaling = ColorTreatment::LOG_10_SCALING;
        else if (os_Color_EscalatColor.compare("IntervalsUsuari") == 0)
            ColorScaling = ColorTreatment::USER_INTERVALS;
    }
    else
    {
        if (IsCategorical())
            ColorScaling = ColorTreatment::DIRECT_ASSIGNATION;
        else
            ColorScaling = ColorTreatment::LINEAR_SCALING;
    }
}

CPLErr MMRPalettes::UpdateConstantColor()
{
    // Example: Color_Smb=(255,0,255)
    CPLString os_Color_Smb;
    if (!pfRel->GetMetadataValue(SECTION_COLOR_TEXT, osBandSection, "Color_Smb",
                                 os_Color_Smb))
        return CE_None;

    os_Color_Smb.replaceAll(" ", "");
    if (!os_Color_Smb.empty() && os_Color_Smb.size() >= 7 &&
        os_Color_Smb[0] == '(' && os_Color_Smb[os_Color_Smb.size() - 1] == ')')
    {
        os_Color_Smb.replaceAll("(", "");
        os_Color_Smb.replaceAll(")", "");
        char **papszTokens = CSLTokenizeString2(os_Color_Smb, ",", 0);
        if (CSLCount(papszTokens) != 3)
        {
            CSLDestroy(papszTokens);
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid constant color: \"%s\"", pfRel->GetRELNameChar());
            return CE_Failure;
        }

        int nIColor0;
        if (1 != sscanf(papszTokens[0], "%d", &nIColor0))
        {
            CSLDestroy(papszTokens);
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid constant color: \"%s\"", pfRel->GetRELNameChar());
            return CE_Failure;
        }

        int nIColor1;
        if (1 != sscanf(papszTokens[1], "%d", &nIColor1))
        {
            CSLDestroy(papszTokens);
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid constant color: \"%s\"", pfRel->GetRELNameChar());
            return CE_Failure;
        }

        int nIColor2;
        if (1 != sscanf(papszTokens[2], "%d", &nIColor2))
        {
            CSLDestroy(papszTokens);
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid constant color: \"%s\"", pfRel->GetRELNameChar());
            return CE_Failure;
        }

        SetConstantColorRGB(static_cast<short>(nIColor0),
                            static_cast<short>(nIColor1),
                            static_cast<short>(nIColor2));

        CSLDestroy(papszTokens);
    }
    return CE_None;
}

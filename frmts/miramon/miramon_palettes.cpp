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

MMRPalettes::MMRPalettes(MMRRel &fRel, const CPLString &osBandSectionIn)
    : m_pfRel(&fRel), m_osBandSection(osBandSectionIn)
{
    // Is the palette a constant color? Then, which color is it?
    CPLString os_Color_Const;
    if (m_pfRel->GetMetadataValue(SECTION_COLOR_TEXT, m_osBandSection,
                                  "Color_Const", os_Color_Const) &&
        EQUAL(os_Color_Const, "1"))
    {
        m_bIsConstantColor = true;
        if (CE_None != UpdateConstantColor())
            return;  // The constant color indicated is wrong
        m_nRealNPaletteColors = 1;
        m_bIsValid = true;
        m_ColorScaling = ColorTreatment::DIRECT_ASSIGNATION;
        SetIsCategorical(true);
        return;
    }

    // Is this an authomatic palette or has a color table (dbf, pal,...)?
    CPLString os_Color_Paleta = "";
    ;
    if (m_pfRel->GetMetadataValue(SECTION_COLOR_TEXT, m_osBandSection,
                                  "Color_Paleta", os_Color_Paleta))
    {
        if (EQUAL(os_Color_Paleta, "<Automatic>"))
            m_bIsAutomatic = true;
    }

    // Treatment of the color variable
    CPLString os_Color_TractamentVariable;
    if (!m_pfRel->GetMetadataValue(SECTION_COLOR_TEXT, m_osBandSection,
                                   "Color_TractamentVariable",
                                   os_Color_TractamentVariable) ||
        os_Color_TractamentVariable.empty())
    {
        CPLString os_TractamentVariable;
        if (!m_pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA,
                                       "TractamentVariable",
                                       os_TractamentVariable))
            os_TractamentVariable = "";

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

    if (UpdateColorInfo() == CE_Failure)
        return;

    if (m_bIsAutomatic)
    {
        // How many "colors" are involved?
        CPLString os_Color_N_SimbolsALaTaula = "";
        if (m_pfRel->GetMetadataValue(SECTION_COLOR_TEXT, m_osBandSection,
                                      "Color_N_SimbolsALaTaula",
                                      os_Color_N_SimbolsALaTaula))
        {
            GIntBig nBigVal = CPLAtoGIntBig(os_Color_N_SimbolsALaTaula);
            if (nBigVal >= INT_MAX)
                return;
            m_nRealNPaletteColors = m_nNPaletteColors =
                static_cast<int>(nBigVal);
            if (m_nNPaletteColors <= 0 || m_nNPaletteColors >= 256)
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "Invalid number of colors "
                         "(Color_N_SimbolsALaTaula) in \"%s\".",
                         m_pfRel->GetRELName().c_str());
                return;
            }
        }
        else
        {
            if (IsCategorical())
            {
                // Predefined color table: m_ThematicPalette
                if (CE_None != GetPaletteColors_Automatic())
                    return;
            }
            else  // No palette associated
                return;
        }
        m_bIsValid = true;
        return;
    }

    // If color is no automatic, from where we got this?
    CPLString osExtension = CPLGetExtensionSafe(os_Color_Paleta);
    if (osExtension.tolower() == "dbf")
    {
        if (CE_None != GetPaletteColors_DBF(os_Color_Paleta))
            return;

        m_bIsValid = true;
    }
    else if (osExtension.tolower() == "pal" || osExtension.tolower() == "p25" ||
             osExtension.tolower() == "p65")
    {
        if (CE_None != GetPaletteColors_PAL_P25_P65(os_Color_Paleta))
            return;

        m_bIsValid = true;
    }
    else
        return;

    m_nRealNPaletteColors = m_nNPaletteColors;
    if (HasNodata())
    {
        if (m_nNPaletteColors < 1)
            return;
        m_nNPaletteColors--;
    }
    else
    {
        // If palette doesn't have nodata let's set some index
        m_nNoDataPaletteIndex = m_nRealNPaletteColors;
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
    m_aadfPaletteColors[0][nIPaletteIndex] = CPLAtof(pszField);

    // GREEN
    memcpy(pszField, pzsRecord + oColorTable.pField[nGIndex].AccumulatedBytes,
           oColorTable.pField[nGIndex].BytesPerField);
    pszField[oColorTable.pField[nGIndex].BytesPerField] = '\0';
    m_aadfPaletteColors[1][nIPaletteIndex] = CPLAtof(pszField);

    // BLUE
    memcpy(pszField, pzsRecord + oColorTable.pField[nBIndex].AccumulatedBytes,
           oColorTable.pField[nBIndex].BytesPerField);
    pszField[oColorTable.pField[nBIndex].BytesPerField] = '\0';
    m_aadfPaletteColors[2][nIPaletteIndex] = CPLAtof(pszField);

    // ALPHA
    if (m_aadfPaletteColors[0][nIPaletteIndex] == -1 &&
        m_aadfPaletteColors[1][nIPaletteIndex] == -1 &&
        m_aadfPaletteColors[2][nIPaletteIndex] == -1)
    {
        // Transparent (white or whatever color)
        m_aadfPaletteColors[0][nIPaletteIndex] = m_sNoDataColorRGB.c1;
        m_aadfPaletteColors[1][nIPaletteIndex] = m_sNoDataColorRGB.c2;
        m_aadfPaletteColors[2][nIPaletteIndex] = m_sNoDataColorRGB.c3;
        m_aadfPaletteColors[3][nIPaletteIndex] = m_sNoDataColorRGB.c4;
    }
    else
        m_aadfPaletteColors[3][nIPaletteIndex] = 255;
}

CPLErr MMRPalettes::GetPaletteColors_DBF_Indexes(
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

// Colors in a PAL, P25 or P65 format files
// Updates nNPaletteColors
CPLErr MMRPalettes::GetPaletteColors_Automatic()
{
    m_nRealNPaletteColors = m_nNPaletteColors =
        static_cast<int>(m_ThematicPalette.size());

    for (int iColumn = 0; iColumn < 4; iColumn++)
    {
        try
        {
            m_aadfPaletteColors[iColumn].resize(m_nNPaletteColors, 0);
        }
        catch (std::bad_alloc &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
            return CE_Failure;
        }
    }

    for (int nIndex = 0; nIndex < m_nRealNPaletteColors; nIndex++)
    {
        // Index of the color

        // RED
        m_aadfPaletteColors[0][nIndex] = m_ThematicPalette[nIndex].c1;

        // GREEN
        m_aadfPaletteColors[1][nIndex] = m_ThematicPalette[nIndex].c2;

        // BLUE
        m_aadfPaletteColors[2][nIndex] = m_ThematicPalette[nIndex].c3;

        // ALPHA
        m_aadfPaletteColors[3][nIndex] = m_ThematicPalette[nIndex].c4;
    }

    return CE_None;
}

// Updates nNPaletteColors
CPLErr MMRPalettes::GetPaletteColors_DBF(const CPLString &os_Color_Paleta_DBF)

{
    // Getting the full path name of the DBF
    CPLString osAux = CPLGetPathSafe(m_pfRel->GetRELNameChar());
    CPLString osColorTableFileName =
        CPLFormFilenameSafe(osAux.c_str(), os_Color_Paleta_DBF.c_str(), "");

    // Reading the DBF file
    struct MM_DATA_BASE_XP oColorTable;
    memset(&oColorTable, 0, sizeof(oColorTable));

    if (MM_ReadExtendedDBFHeaderFromFile(osColorTableFileName.c_str(),
                                         &oColorTable,
                                         m_pfRel->GetRELNameChar()))
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "Invalid color table:"
                 "\"%s\".",
                 osColorTableFileName.c_str());

        return CE_Failure;
    }

    // Getting indices of fields that determine the colors.
    MM_EXT_DBF_N_FIELDS nClauSimbol, nRIndex, nGIndex, nBIndex;
    if (CE_Failure == GetPaletteColors_DBF_Indexes(oColorTable, nClauSimbol,
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

    // Checking the structure to be correct
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
    if (!pzsRecord)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Out of memory allocating working buffer");
        VSIFCloseL(oColorTable.pfDataBase);
        MM_ReleaseMainFields(&oColorTable);
    }
    char *pszField = static_cast<char *>(VSI_CALLOC_VERBOSE(1, nBufferSize));

    if (!pszField)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Out of memory allocating working buffer");
        VSIFree(pzsRecord);
        VSIFCloseL(oColorTable.pfDataBase);
        MM_ReleaseMainFields(&oColorTable);
    }

    m_nNPaletteColors = static_cast<int>(oColorTable.nRecords);  // Safe cast

    // Checking the size of the palette.
    if (m_nNPaletteColors < 0 || m_nNPaletteColors > 65536)
    {
        m_nNPaletteColors = 0;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid number of colors: %d "
                 "in color table \"%s\".",
                 m_nNPaletteColors, osColorTableFileName.c_str());

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
            m_aadfPaletteColors[iColumn].resize(m_nNPaletteColors, 0);
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

    VSIFSeekL(oColorTable.pfDataBase,
              static_cast<vsi_l_offset>(oColorTable.FirstRecordOffset),
              SEEK_SET);
    /*
        Each record's CLAUSIMBOL field doesn't match a pixel value present in the raster,
        and it's used only for discovering nodata value (blanc value).
        The list of values is used to map every value in a color using:
            - Direct assignation: mode used in categorical modes but possible in continuous.
            - Linear scaling
            - Logarithmic scaling
        */
    for (int nIPaletteColors = 0; nIPaletteColors < m_nNPaletteColors;
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
            m_bHasNodata = true;
            m_nNoDataPaletteIndex = nIPaletteColors;
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
CPLErr
MMRPalettes::GetPaletteColors_PAL_P25_P65(const CPLString &os_Color_Paleta_DBF)

{
    CPLString osAux = CPLGetPathSafe(m_pfRel->GetRELNameChar());
    CPLString osColorTableFileName =
        CPLFormFilenameSafe(osAux.c_str(), os_Color_Paleta_DBF.c_str(), "");

    // This kind of palette has not NoData color.
    //bHasNodata = false;

    CPLString osExtension = CPLGetExtensionSafe(os_Color_Paleta_DBF);
    int nNReadPaletteColors = 0;
    m_nNPaletteColors = 0;

    if (osExtension.tolower() == "pal")
        m_nNPaletteColors = 64;
    else if (osExtension.tolower() == "p25")
        m_nNPaletteColors = 256;
    else if (osExtension.tolower() == "p65")
        m_nNPaletteColors = 65536;
    else
        return CE_None;

    for (int iColumn = 0; iColumn < 4; iColumn++)
    {
        try
        {
            m_aadfPaletteColors[iColumn].resize(m_nNPaletteColors, 0);
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

        const CPLStringList aosTokens(CSLTokenizeString2(pszLine, " \t", 0));
        if (aosTokens.size() != 4)
        {
            VSIFCloseL(fpColorTable);
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid color table: \"%s\"",
                     osColorTableFileName.c_str());
            return CE_Failure;
        }

        // Index of the color
        // papszTokens[0] is ignored;

        // RED
        m_aadfPaletteColors[0][nNReadPaletteColors] = CPLAtof(aosTokens[1]);

        // GREEN
        m_aadfPaletteColors[1][nNReadPaletteColors] = CPLAtof(aosTokens[2]);

        // BLUE
        m_aadfPaletteColors[2][nNReadPaletteColors] = CPLAtof(aosTokens[3]);

        // ALPHA
        m_aadfPaletteColors[3][nNReadPaletteColors] = 255.0;
        nNReadPaletteColors++;
    }

    // Filling the rest of colors.
    for (int nIColorIndex = nNReadPaletteColors;
         nIColorIndex < m_nNPaletteColors; nIColorIndex++)
    {
        m_aadfPaletteColors[0][nNReadPaletteColors] = m_sDefaultColorRGB.c1;
        m_aadfPaletteColors[1][nNReadPaletteColors] = m_sDefaultColorRGB.c2;
        m_aadfPaletteColors[2][nNReadPaletteColors] = m_sDefaultColorRGB.c3;
        m_aadfPaletteColors[3][nNReadPaletteColors] = m_sDefaultColorRGB.c4;
        nNReadPaletteColors++;
    }

    if (nNReadPaletteColors != m_nNPaletteColors)
    {
        VSIFCloseL(fpColorTable);
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid color table: \"%s\"",
                 osColorTableFileName.c_str());
        return CE_Failure;
    }

    VSIFCloseL(fpColorTable);

    return CE_None;
}

CPLErr MMRPalettes::UpdateColorInfo()
{
    CPLString os_Color_EscalatColor;
    if (m_pfRel->GetMetadataValue(SECTION_COLOR_TEXT, m_osBandSection,
                                  "Color_EscalatColor",
                                  os_Color_EscalatColor) &&
        !os_Color_EscalatColor.empty())
    {
        if (os_Color_EscalatColor.compare("AssigDirecta") == 0)
            m_ColorScaling = ColorTreatment::DIRECT_ASSIGNATION;
        else if (os_Color_EscalatColor.compare("DespOrigen") == 0)
            m_ColorScaling = ColorTreatment::ORIGIN_DISPLACEMENT;
        else if (os_Color_EscalatColor.compare("lineal") == 0)
            m_ColorScaling = ColorTreatment::LINEAR_SCALING;
        else if (os_Color_EscalatColor.compare("log_10") == 0)
            m_ColorScaling = ColorTreatment::LOG_10_SCALING;
        else if (os_Color_EscalatColor.compare("IntervalsUsuari") == 0)
            m_ColorScaling = ColorTreatment::USER_INTERVALS;
    }
    else
    {
        if (IsCategorical())
            m_ColorScaling = ColorTreatment::DIRECT_ASSIGNATION;
        else
            m_ColorScaling = ColorTreatment::LINEAR_SCALING;
    }

    if (m_ColorScaling == ColorTreatment::DEFAULT_SCALING)
        return CE_Failure;
    return CE_None;
}

CPLErr MMRPalettes::UpdateConstantColor()
{
    // Example: Color_Smb=(255,0,255)
    CPLString os_Color_Smb;
    if (!m_pfRel->GetMetadataValue(SECTION_COLOR_TEXT, m_osBandSection,
                                   "Color_Smb", os_Color_Smb))
        return CE_None;

    os_Color_Smb.replaceAll(" ", "");
    if (!os_Color_Smb.empty() && os_Color_Smb.size() >= 7 &&
        os_Color_Smb[0] == '(' && os_Color_Smb[os_Color_Smb.size() - 1] == ')')
    {
        os_Color_Smb.replaceAll("(", "");
        os_Color_Smb.replaceAll(")", "");
        const CPLStringList aosTokens(CSLTokenizeString2(os_Color_Smb, ",", 0));
        if (CSLCount(aosTokens) != 3)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid constant color: \"%s\"",
                     m_pfRel->GetRELNameChar());
            return CE_Failure;
        }

        int nIColor0;
        if (1 != sscanf(aosTokens[0], "%d", &nIColor0))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid constant color: \"%s\"",
                     m_pfRel->GetRELNameChar());
            return CE_Failure;
        }

        int nIColor1;
        if (1 != sscanf(aosTokens[1], "%d", &nIColor1))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid constant color: \"%s\"",
                     m_pfRel->GetRELNameChar());
            return CE_Failure;
        }

        int nIColor2;
        if (1 != sscanf(aosTokens[2], "%d", &nIColor2))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid constant color: \"%s\"",
                     m_pfRel->GetRELNameChar());
            return CE_Failure;
        }

        SetConstantColorRGB(static_cast<short>(nIColor0),
                            static_cast<short>(nIColor1),
                            static_cast<short>(nIColor2));
    }
    return CE_None;
}

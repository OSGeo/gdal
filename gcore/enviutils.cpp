/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Read ENVI .hdr file
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_string.h"
#include "cpl_vsi.h"

#include "gdal_cpp_functions.h"
#include "gdal_colortable.h"
#include "gdal_dataset.h"
#include "gdal_rasterband.h"
#include "rawdataset.h"

/************************************************************************/
/*                         GDALReadENVIHeader()                         */
/************************************************************************/

CPLStringList GDALReadENVIHeader(VSILFILE *fpHdr)

{
    CPLStringList aosHeaders;

    constexpr int MAX_LINE_SIZE = 10000;

    // Skip first line with "ENVI"
    CPLReadLine2L(fpHdr, MAX_LINE_SIZE, nullptr);

    // Start forming sets of name/value pairs.
    CPLString osWorkingLine;
    std::string osValue;
    while (true)
    {
        const char *pszNewLine = CPLReadLine2L(fpHdr, MAX_LINE_SIZE, nullptr);
        if (pszNewLine == nullptr)
            break;

        // Skip leading spaces. This may happen for example with
        // AVIRIS datasets (https://aviris.jpl.nasa.gov/dataportal/) whose
        // wavelength metadata starts with a leading space.
        while (*pszNewLine == ' ')
            ++pszNewLine;
        if (strchr(pszNewLine, '=') == nullptr)
            continue;

        osWorkingLine = pszNewLine;

        // Collect additional lines if we have open curly bracket.
        if (osWorkingLine.find("{") != std::string::npos &&
            osWorkingLine.find("}") == std::string::npos)
        {
            do
            {
                pszNewLine = CPLReadLine2L(fpHdr, MAX_LINE_SIZE, nullptr);
                if (pszNewLine)
                {
                    osWorkingLine += pszNewLine;
                }
                if (osWorkingLine.size() > 10 * 1024 * 1024)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Concatenated line exceeds 10 MB");
                    return aosHeaders;
                }
            } while (pszNewLine != nullptr &&
                     strchr(pszNewLine, '}') == nullptr);
        }

        // Try to break input into name and value portions. Trim whitespace.
        size_t iEqual = osWorkingLine.find("=");

        if (iEqual != std::string::npos && iEqual > 0)
        {
            osValue = osWorkingLine.substr(iEqual + 1);
            const auto found = osValue.find_first_not_of(" \t");
            if (found != std::string::npos)
                osValue = osValue.substr(found);
            else
                osValue.clear();

            iEqual--;
            while (iEqual > 0 && (osWorkingLine[iEqual] == ' ' ||
                                  osWorkingLine[iEqual] == '\t'))
            {
                iEqual--;
            }
            osWorkingLine.resize(iEqual + 1);
            osWorkingLine.replaceAll(' ', '_');
            aosHeaders.SetNameValue(osWorkingLine.c_str(), osValue.c_str());
        }
    }

    return aosHeaders;
}

/************************************************************************/
/*                       GDALENVISplitList()                            */
/*                                                                      */
/*      Split an ENVI value list into component fields, and strip       */
/*      white space.                                                    */
/************************************************************************/

CPLStringList GDALENVISplitList(const char *pszCleanInput)

{
    CPLStringList aosList;

    if (!pszCleanInput || pszCleanInput[0] != '{')
    {
        return aosList;
    }

    char *pszInput = CPLStrdup(pszCleanInput);

    int iChar = 1;
    while (pszInput[iChar] != '}' && pszInput[iChar] != '\0')
    {
        // Find start of token.
        int iFStart = iChar;
        while (pszInput[iFStart] == ' ')
            iFStart++;

        int iFEnd = iFStart;
        while (pszInput[iFEnd] != ',' && pszInput[iFEnd] != '}' &&
               pszInput[iFEnd] != '\0')
            iFEnd++;

        if (pszInput[iFEnd] == '\0')
            break;

        iChar = iFEnd + 1;
        iFEnd = iFEnd - 1;

        while (iFEnd > iFStart && pszInput[iFEnd] == ' ')
            iFEnd--;

        pszInput[iFEnd + 1] = '\0';
        aosList.AddString(pszInput + iFStart);
    }

    CPLFree(pszInput);

    return aosList;
}

/************************************************************************/
/*                        GDALApplyENVIHeaders()                        */
/************************************************************************/

void GDALApplyENVIHeaders(GDALDataset *poDS, const CPLStringList &aosHeaders,
                          CSLConstList papszOptions)
{
    const int nBands = poDS->GetRasterCount();
    auto poPamDS = dynamic_cast<GDALPamDataset *>(poDS);
    const int nPAMFlagsBackup = poPamDS ? poPamDS->GetPamFlags() : -1;

    // Apply band names if we have them.
    // Use wavelength for more descriptive information if possible.
    const char *pszBandNames = aosHeaders["band_names"];
    const char *pszWaveLength = aosHeaders["wavelength"];
    if (pszBandNames || pszWaveLength)
    {
        const bool bSetDatasetLevelMetadata = CPLTestBool(CSLFetchNameValueDef(
            papszOptions, "SET_DATASET_LEVEL_METADATA", "YES"));
        const bool bSetBandName = CPLTestBool(
            CSLFetchNameValueDef(papszOptions, "SET_BAND_NAME", "YES"));
        const CPLStringList aosBandNames(GDALENVISplitList(pszBandNames));
        const CPLStringList aosWL(GDALENVISplitList(pszWaveLength));
        const char *pszFWHM = aosHeaders["fwhm"];
        const CPLStringList aosFWHM(GDALENVISplitList(pszFWHM ? pszFWHM : ""));

        const char *pszWLUnits = nullptr;
        const int nWLCount = aosWL.size();
        const int nFWHMCount = aosFWHM.size();
        if (nWLCount)
        {
            // If WL information is present, process wavelength units.
            pszWLUnits = aosHeaders["wavelength_units"];
            if (pszWLUnits)
            {
                // Don't show unknown or index units.
                if (EQUAL(pszWLUnits, "Unknown") || EQUAL(pszWLUnits, "Index"))
                    pszWLUnits = nullptr;
            }
            if (pszWLUnits && bSetDatasetLevelMetadata)
            {
                // Set wavelength units to dataset metadata.
                poDS->SetMetadataItem("wavelength_units", pszWLUnits);
            }
        }

        for (int i = 0; i < nBands; i++)
        {
            // First set up the wavelength names and units if available.
            std::string osWavelength;
            if (nWLCount > i)
            {
                osWavelength = aosWL[i];
                if (pszWLUnits)
                {
                    osWavelength += " ";
                    osWavelength += pszWLUnits;
                }
            }

            if (bSetBandName)
            {
                // Build the final name for this band.
                std::string osBandName;
                if (aosBandNames && CSLCount(aosBandNames) > i)
                {
                    osBandName = aosBandNames[i];
                    if (!osWavelength.empty())
                    {
                        osBandName += " (";
                        osBandName += osWavelength;
                        osBandName += ")";
                    }
                }
                else
                {
                    // WL but no band names.
                    osBandName = std::move(osWavelength);
                }

                // Description is for internal GDAL usage.
                poDS->GetRasterBand(i + 1)->SetDescription(osBandName.c_str());

                // Metadata field named Band_1, etc. Needed for ArcGIS integration.
                const std::string osBandId = CPLSPrintf("Band_%i", i + 1);
                if (bSetDatasetLevelMetadata)
                    poDS->SetMetadataItem(osBandId.c_str(), osBandName.c_str());
            }

            const auto ConvertWaveLength =
                [pszWLUnits](double dfVal) -> const char *
            {
                if (EQUAL(pszWLUnits, "Micrometers") || EQUAL(pszWLUnits, "um"))
                {
                    return CPLSPrintf("%.3f", dfVal);
                }
                else if (EQUAL(pszWLUnits, "Nanometers") ||
                         EQUAL(pszWLUnits, "nm"))
                {
                    return CPLSPrintf("%.3f", dfVal / 1000);
                }
                else if (EQUAL(pszWLUnits, "Millimeters") ||
                         EQUAL(pszWLUnits, "mm"))
                {
                    return CPLSPrintf("%.3f", dfVal * 1000);
                }
                else
                {
                    return nullptr;
                }
            };

            // Set wavelength metadata to band.
            if (nWLCount > i)
            {
                poDS->GetRasterBand(i + 1)->SetMetadataItem("wavelength",
                                                            aosWL[i]);

                if (pszWLUnits)
                {
                    poDS->GetRasterBand(i + 1)->SetMetadataItem(
                        "wavelength_units", pszWLUnits);

                    if (const char *pszVal =
                            ConvertWaveLength(CPLAtof(aosWL[i])))
                    {
                        poDS->GetRasterBand(i + 1)->SetMetadataItem(
                            "CENTRAL_WAVELENGTH_UM", pszVal, "IMAGERY");
                    }
                }
            }

            if (nFWHMCount > i && pszWLUnits)
            {
                if (const char *pszVal = ConvertWaveLength(CPLAtof(aosFWHM[i])))
                {
                    poDS->GetRasterBand(i + 1)->SetMetadataItem(
                        "FWHM_UM", pszVal, "IMAGERY");
                }
            }
        }
    }

    if (CPLTestBool(
            CSLFetchNameValueDef(papszOptions, "APPLY_DEFAULT_BANDS", "YES")))
    {
        // Apply "default bands" if we have it to set RGB color interpretation.
        const char *pszDefaultBands = aosHeaders["default_bands"];
        if (pszDefaultBands)
        {
            const CPLStringList aosDefaultBands(
                GDALENVISplitList(pszDefaultBands));
            if (aosDefaultBands.size() == 3)
            {
                const int nRBand = atoi(aosDefaultBands[0]);
                const int nGBand = atoi(aosDefaultBands[1]);
                const int nBBand = atoi(aosDefaultBands[2]);
                if (nRBand >= 1 && nRBand <= nBands && nGBand >= 1 &&
                    nGBand <= nBands && nBBand >= 1 && nBBand <= nBands &&
                    nRBand != nGBand && nRBand != nBBand && nGBand != nBBand)
                {
                    poDS->GetRasterBand(nRBand)->SetColorInterpretation(
                        GCI_RedBand);
                    poDS->GetRasterBand(nGBand)->SetColorInterpretation(
                        GCI_GreenBand);
                    poDS->GetRasterBand(nBBand)->SetColorInterpretation(
                        GCI_BlueBand);
                }
            }
            else if (aosDefaultBands.size() == 1)
            {
                const int nGrayBand = atoi(aosDefaultBands[0]);
                if (nGrayBand >= 1 && nGrayBand <= nBands)
                {
                    poDS->GetRasterBand(nGrayBand)->SetColorInterpretation(
                        GCI_GrayIndex);
                }
            }
        }
    }

    // Apply data offset values
    if (const char *pszDataOffsetValues = aosHeaders["data_offset_values"])
    {
        const CPLStringList aosValues(GDALENVISplitList(pszDataOffsetValues));
        if (aosValues.size() == nBands)
        {
            for (int i = 0; i < nBands; ++i)
                poDS->GetRasterBand(i + 1)->SetOffset(CPLAtof(aosValues[i]));
        }
    }

    // Apply data gain values
    if (const char *pszDataGainValues = aosHeaders["data_gain_values"])
    {
        const CPLStringList aosValues(GDALENVISplitList(pszDataGainValues));
        if (aosValues.size() == nBands)
        {
            for (int i = 0; i < nBands; ++i)
            {
                poDS->GetRasterBand(i + 1)->SetScale(CPLAtof(aosValues[i]));
            }
        }
    }

    // Apply class names if we have them.
    if (const char *pszClassNames = aosHeaders["class_names"])
    {
        poDS->GetRasterBand(1)->SetCategoryNames(
            GDALENVISplitList(pszClassNames).List());
    }

    if (const char *pszBBL = aosHeaders["bbl"])
    {
        const CPLStringList aosValues(GDALENVISplitList(pszBBL));
        if (aosValues.size() == nBands)
        {
            for (int i = 0; i < nBands; ++i)
            {
                poDS->GetRasterBand(i + 1)->SetMetadataItem(
                    "good_band",
                    strcmp(aosValues[i], "1") == 0 ? "true" : "false");
            }
        }
    }

    if (CPLTestBool(
            CSLFetchNameValueDef(papszOptions, "APPLY_CLASS_LOOKUP", "YES")))
    {
        // Apply colormap if we have one.
        const char *pszClassLookup = aosHeaders["class_lookup"];
        if (pszClassLookup != nullptr)
        {
            const CPLStringList aosClassColors(
                GDALENVISplitList(pszClassLookup));
            const int nColorValueCount = aosClassColors.size();
            GDALColorTable oCT;

            for (int i = 0; i * 3 + 2 < nColorValueCount; i++)
            {
                const GDALColorEntry sEntry = {
                    static_cast<short>(std::clamp(
                        atoi(aosClassColors[i * 3 + 0]), 0, 255)),  // Red
                    static_cast<short>(std::clamp(
                        atoi(aosClassColors[i * 3 + 1]), 0, 255)),  // Green
                    static_cast<short>(std::clamp(
                        atoi(aosClassColors[i * 3 + 2]), 0, 255)),  // Blue
                    255};
                oCT.SetColorEntry(i, &sEntry);
            }

            poDS->GetRasterBand(1)->SetColorTable(&oCT);
            poDS->GetRasterBand(1)->SetColorInterpretation(GCI_PaletteIndex);
        }
    }

    if (CPLTestBool(CSLFetchNameValueDef(papszOptions,
                                         "APPLY_DATA_IGNORE_VALUE", "YES")))
    {
        // Set the nodata value if it is present.
        const char *pszDataIgnoreValue = aosHeaders["data_ignore_value"];
        if (pszDataIgnoreValue != nullptr)
        {
            for (int i = 0; i < nBands; i++)
            {
                auto poBand =
                    dynamic_cast<RawRasterBand *>(poDS->GetRasterBand(i + 1));
                if (poBand)
                    poBand->SetNoDataValue(CPLAtof(pszDataIgnoreValue));
            }
        }
    }

    if (poPamDS)
        poPamDS->SetPamFlags(nPAMFlagsBackup);
}

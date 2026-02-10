/******************************************************************************
 *
 * Project:  MiraMonRaster driver
 * Purpose:  Implements MMRRel: provides access to the REL file, which
 *           holds all the necessary metadata to correctly interpret and
 *           access the associated raw data.
 * Author:   Abel Pau
 *
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "gdal_priv.h"
#include "cpl_string.h"
#include <set>

#include "miramon_rel.h"
#include "miramon_band.h"

#include "../miramon_common/mm_gdal_functions.h"  // For MMCheck_REL_FILE()

CPLString MMRRel::m_szImprobableRELChain = "@#&%$|``|$%&#@";

/************************************************************************/
/*                               MMRRel()                               */
/************************************************************************/
MMRRel::MMRRel(const CPLString &osRELFilenameIn, bool bIMGMustExist)
    : m_osRelFileName(osRELFilenameIn)
{
    CPLString osRelCandidate = osRELFilenameIn;

    // Getting the name of the REL
    const CPLString osMMRPrefix = "MiraMonRaster:";
    if (STARTS_WITH(osRelCandidate, osMMRPrefix))
    {
        // SUBDATASET case: gets the names of the bands in the subdataset
        size_t nPos = osRelCandidate.ifind(osMMRPrefix);
        if (nPos != 0)
            return;

        CPLString osSDSReL = osRelCandidate.substr(osMMRPrefix.size());

        // Getting the internal names of the bands
        const CPLStringList aosTokens(CSLTokenizeString2(osSDSReL, ",", 0));
        const int nTokens = CSLCount(aosTokens);

        if (nTokens < 1)
            return;

        osRelCandidate = aosTokens[0];
        osRelCandidate.replaceAll("\"", "");

        // Getting the list of bands in the subdataset
        for (int nIBand = 0; nIBand < nTokens - 1; nIBand++)
        {
            // Raw band name
            CPLString osBandName = aosTokens[nIBand + 1];
            osBandName.replaceAll("\"", "");
            m_papoSDSBands.emplace_back(osBandName);
        }
        m_bIsAMiraMonFile = true;
    }
    else
    {
        // Getting the metadata file name. If it's already a REL file,
        // then same name is returned.
        osRelCandidate = GetAssociatedMetadataFileName(m_osRelFileName.c_str());
        if (osRelCandidate.empty())
        {
            if (m_bIsAMiraMonFile)
            {
                CPLError(CE_Failure, CPLE_OpenFailed,
                         "Metadata file for %s should exist.",
                         m_osRelFileName.c_str());
            }
            if (!bIMGMustExist)
            {
                // Simulates that we have a MiraMon file
                // and we can ask things to this Rel file.
                UpdateRELNameChar(m_osRelFileName);
                m_bIsAMiraMonFile = true;
                if (!OpenRELFile("rb"))
                    return;
            }
            return;
        }
        else
        {
            // It's a REL and it's not empty, so it's a MiraMon file
            VSILFILE *pF = VSIFOpenL(osRelCandidate, "r");
            if (!pF)
            {
                CPLError(CE_Failure, CPLE_OpenFailed,
                         "Metadata file %s could not be opened.",
                         m_osRelFileName.c_str());
                return;
            }
            VSIFSeekL(pF, 0, SEEK_END);
            if (VSIFTellL(pF))
                m_bIsAMiraMonFile = true;
            else
            {
                CPLError(
                    CE_Failure, CPLE_OpenFailed,
                    "Metadata file for %s should have some information in.",
                    m_osRelFileName.c_str());

                VSIFCloseL(pF);
                return;
            }
            VSIFCloseL(pF);
        }
    }

    // If rel name was not a REL name, we update that
    // from the one found in the process of discovering it.
    UpdateRELNameChar(osRelCandidate);

    // We let it be opened
    if (!OpenRELFile("rb"))
        return;

    // Collect band information
    if (ParseBandInfo() != CE_None)
        return;

    // We have a valid object MMRREL.
    m_bIsValid = true;

    return;
}

/************************************************************************/
/*                              ~MMRRel()                               */
/************************************************************************/

MMRRel::~MMRRel()
{
    CloseRELFile();
}

/************************************************************************/
/*                      Getting section-key-value                       */
/************************************************************************/
// Used when the MMRREL is not yet constructed.
CPLString
MMRRel::GetValueFromSectionKeyPriorToREL(const CPLString &osPriorRelName,
                                         const CPLString &osSection,
                                         const CPLString &osKey)
{
    if (osPriorRelName.empty())
        return "";

    VSILFILE *pPriorRELFile = VSIFOpenL(osPriorRelName, "rb");
    if (!pPriorRELFile)
        return "";

    CPLString osValue = GetValueFromSectionKey(pPriorRELFile, osSection, osKey);
    VSIFCloseL(pPriorRELFile);
    return osValue;
}

// Used when the MMRREL is already constructed.
CPLString MMRRel::GetValueFromSectionKeyFromREL(const CPLString &osSection,
                                                const CPLString &osKey)
{
    if (!GetRELFile())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "REL file is not opened: \"%s\"",
                 m_osRelFileName.c_str());
        return "";
    }

    return GetValueFromSectionKey(GetRELFile(), osSection, osKey);
}

// This function is the C++ equivalent of MMReturnValueFromSectionINIFile().
// It improves upon the original by using CPLString instead of raw char pointers,
// and by operating on an already opened file pointer rather than reopening the file
// on each invocation.
// MMReturnValueFromSectionINIFile() is retained in miramon_common because it is
// widely used by existing, already OGR tested code (and in the common code itself).
// At least in C++ code the modern version is used
CPLString MMRRel::GetValueFromSectionKey(VSILFILE *pf,
                                         const CPLString &osSection,
                                         const CPLString &osKey)
{
    if (!pf)
        return "";

    CPLString osCurrentSection;
    CPLString osCurrentKey, osCurrentValue;
    bool bIAmInMySection = false;

    const char *pszLine;

    VSIFSeekL(pf, 0, SEEK_SET);
    while ((pszLine = CPLReadLine2L(pf, 10000, nullptr)) != nullptr)
    {
        CPLString rawLine = pszLine;

        rawLine.Recode(CPL_ENC_ISO8859_1, CPL_ENC_UTF8);
        rawLine.Trim();

        if (rawLine.empty() || rawLine[0] == ';' || rawLine[0] == '#')
            continue;

        if (rawLine[0] == '[' && rawLine[rawLine.size() - 1] == ']')
        {
            if (bIAmInMySection)
            {
                // This is the next section to mine, so nothing to find here.
                return m_szImprobableRELChain;
            }

            osCurrentSection = rawLine.substr(1, rawLine.size() - 2);
            osCurrentSection.Trim();

            if (!EQUAL(osCurrentSection, osSection))
                bIAmInMySection = false;
            else
                bIAmInMySection = true;

            continue;
        }

        if (!bIAmInMySection)
            continue;

        size_t equalPos = rawLine.find('=');
        if (equalPos != CPLString::npos)
        {
            osCurrentKey = rawLine.substr(0, equalPos);
            osCurrentValue = rawLine.substr(equalPos + 1);
            osCurrentKey.Trim();
            osCurrentValue.Trim();

            if (EQUAL(osCurrentKey, osKey))
                return osCurrentValue;
        }
    }

    return m_szImprobableRELChain;  // Key not found
}

/************************************************************************/
/*                           Other functions                            */
/************************************************************************/

// Converts FileNameI.rel to FileName
CPLString MMRRel::MMRGetFileNameWithOutI(const CPLString &osRELFile)
{
    if (osRELFile.empty())
        return "";

    CPLString osFile = CPLString(CPLResetExtensionSafe(osRELFile, "").c_str());

    if (osFile.length() < 2)
        return "";

    osFile.resize(osFile.size() - 2);  // I.

    return osFile;
}

// Converts FileNameI.rel to FileName.xxx (where xxx is an extension)
CPLString MMRRel::MMRGetFileNameFromRelName(const CPLString &osRELFile,
                                            const CPLString osExtension)
{
    if (osRELFile.empty())
        return "";

    // Extracts I.rel
    CPLString osFile =
        MMRGetFileNameWithOutI(CPLResetExtensionSafe(osRELFile, ""));

    if (!osExtension.empty())
    {
        // Adds extension (with the ".", ex: ".img")
        osFile += osExtension;
    }

    return osFile;
}

// Converts FileName.img to FileNameI.rel
CPLString MMRRel::MMRGetSimpleMetadataName(const CPLString &osLayerName)
{
    if (osLayerName.empty())
        return "";

    // Extract extension
    CPLString osRELFile =
        CPLString(CPLResetExtensionSafe(osLayerName, "").c_str());

    if (!osRELFile.length())
        return "";

    // Extract "."
    osRELFile.resize(osRELFile.size() - 1);
    // Add "I.rel"
    osRELFile += pszExtRasterREL;

    return osRELFile;
}

// Gets the value from a section-key accessing directly to the RELFile.
// It happens when MMRel is used to access a REL that is not an IMG sidecar
// or at the Identify() process, when we don't have already the MMRRel constructed.
bool MMRRel::GetAndExcludeMetadataValueDirectly(const CPLString &osRELFile,
                                                const CPLString &osSection,
                                                const CPLString &osKey,
                                                CPLString &osValue)
{
    addExcludedSectionKey(osSection, osKey);
    return GetMetadataValueDirectly(osRELFile, osSection, osKey, osValue);
}

bool MMRRel::GetMetadataValueDirectly(const CPLString &osRELFile,
                                      const CPLString &osSection,
                                      const CPLString &osKey,
                                      CPLString &osValue)
{
    osValue = GetValueFromSectionKeyPriorToREL(osRELFile, osSection, osKey);

    if (osValue != m_szImprobableRELChain)
        return true;  // Found

    osValue = "";
    return false;  // Key not found
}

bool MMRRel::SameFile(const CPLString &osFile1, const CPLString &osFile2)
{
    if (EQUAL(osFile1, osFile2))
        return true;

    // Just to be more sure:
    CPLString osLayerName1 = osFile1;
    osLayerName1.replaceAll("\\", "/");
    CPLString osLayerName2 = osFile2;
    osLayerName2.replaceAll("\\", "/");

    if (EQUAL(osLayerName1, osLayerName2))
        return true;

    return false;
}

// Gets the state (enum class MMRNomFitxerState) of NomFitxer in the
// specified section
// [pszSection]
// NomFitxer=Value
MMRNomFitxerState MMRRel::MMRStateOfNomFitxerInSection(
    const CPLString &osLayerName, const CPLString &osSection,
    const CPLString &osRELFile, bool bNomFitxerMustExist)
{
    CPLString osDocumentedLayerName;

    if (!GetAndExcludeMetadataValueDirectly(osRELFile, osSection, KEY_NomFitxer,
                                            osDocumentedLayerName) ||
        osDocumentedLayerName.empty())
    {
        CPLString osIIMGFromREL =
            MMRGetFileNameFromRelName(osRELFile, pszExtRaster);
        if (SameFile(osIIMGFromREL, osLayerName))
            return MMRNomFitxerState::NOMFITXER_VALUE_EXPECTED;

        if (bNomFitxerMustExist)
            return MMRNomFitxerState::NOMFITXER_VALUE_UNEXPECTED;
        else
            return MMRNomFitxerState::NOMFITXER_NOT_FOUND;
    }

    CPLString osFileAux = CPLFormFilenameSafe(CPLGetPathSafe(osRELFile).c_str(),
                                              osDocumentedLayerName, "");

    osDocumentedLayerName.Trim();
    if (*osDocumentedLayerName == '*' || *osDocumentedLayerName == '?')
        return MMRNomFitxerState::NOMFITXER_VALUE_UNEXPECTED;

    if (SameFile(osFileAux, osLayerName))
        return MMRNomFitxerState::NOMFITXER_VALUE_EXPECTED;

    return MMRNomFitxerState::NOMFITXER_VALUE_UNEXPECTED;
}

// Tries to find a reference to the IMG file 'pszLayerName'
// we are opening in the REL file 'pszRELFile'
CPLString MMRRel::MMRGetAReferenceToIMGFile(const CPLString &osLayerName,
                                            const CPLString &osRELFile)
{
    if (osRELFile.empty())
    {
        CPLError(CE_Failure, CPLE_OpenFailed, "Expected File name.");
        return "";
    }

    // [ATTRIBUTE_DATA]
    // NomFitxer=
    // It should be empty but if it's not, at least,
    // the value has to be osLayerName
    MMRNomFitxerState iState = MMRStateOfNomFitxerInSection(
        osLayerName, SECTION_ATTRIBUTE_DATA, osRELFile, false);

    if (iState == MMRNomFitxerState::NOMFITXER_VALUE_EXPECTED ||
        iState == MMRNomFitxerState::NOMFITXER_VALUE_EMPTY)
    {
        return osRELFile;
    }
    else if (iState == MMRNomFitxerState::NOMFITXER_VALUE_UNEXPECTED)
    {
        if (m_bIsAMiraMonFile)
        {
            CPLError(
                CE_Failure, CPLE_OpenFailed,
                "Unexpected value for SECTION_ATTRIBUTE_DATA [NomFitxer] in "
                "%s file.",
                osRELFile.c_str());
        }
        return "";
    }

    // Discarting not supported via SDE (some files
    // could have this option)
    CPLString osVia;
    if (GetAndExcludeMetadataValueDirectly(osRELFile, SECTION_ATTRIBUTE_DATA,
                                           KEY_via, osVia))
    {
        if (!osVia.empty() && !EQUAL(osVia, "SDE"))
        {
            if (m_bIsAMiraMonFile)
            {
                CPLError(CE_Failure, CPLE_OpenFailed,
                         "Unexpected Via in %s file", osRELFile.c_str());
            }
            return "";
        }
    }

    CPLString osFieldNames;

    if (!GetAndExcludeMetadataValueDirectly(osRELFile, SECTION_ATTRIBUTE_DATA,
                                            Key_IndexesNomsCamps,
                                            osFieldNames) ||
        osFieldNames.empty())
    {
        if (m_bIsAMiraMonFile)
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "IndexesNomsCamps not found in %s file",
                     osRELFile.c_str());
        }
        return "";
    }

    // Getting the internal names of the bands
    const CPLStringList aosTokens(CSLTokenizeString2(osFieldNames, ",", 0));
    const int nTokenBands = CSLCount(aosTokens);

    CPLString osBandSectionKey;
    CPLString osAttributeDataName;
    for (int nIBand = 0; nIBand < nTokenBands; nIBand++)
    {
        osBandSectionKey = KEY_NomCamp;
        osBandSectionKey.append("_");
        osBandSectionKey.append(aosTokens[nIBand]);

        CPLString osBandSectionValue;

        if (!GetAndExcludeMetadataValueDirectly(
                osRELFile, SECTION_ATTRIBUTE_DATA, osBandSectionKey,
                osBandSectionValue) ||
            osBandSectionValue.empty())
            continue;  // A band without name (·$· unexpected)

        // Example: [ATTRIBUTE_DATA:G1]
        osAttributeDataName = SECTION_ATTRIBUTE_DATA;
        osAttributeDataName.append(":");
        osAttributeDataName.append(osBandSectionValue.Trim());

        // Let's see if this band contains the expected name
        // or none (in monoband case)
        iState = MMRStateOfNomFitxerInSection(osLayerName, osAttributeDataName,
                                              osRELFile, true);
        if (iState == MMRNomFitxerState::NOMFITXER_VALUE_EXPECTED)
            return osRELFile;

        else if (iState == MMRNomFitxerState::NOMFITXER_VALUE_UNEXPECTED)
            continue;

        // If there is only one band is accepted NOMFITXER_NOT_FOUND/EMPTY iState result
        if (nTokenBands == 1)
            return osRELFile;
    }

    if (m_bIsAMiraMonFile)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "REL search failed for all bands in %s file",
                 osRELFile.c_str());
    }
    return "";
}

// Finds the metadata filename associated to osFileName (usually an IMG file)
CPLString MMRRel::GetAssociatedMetadataFileName(const CPLString &osFileName)
{
    if (osFileName.empty())
    {
        if (m_bIsAMiraMonFile)
            CPLError(CE_Failure, CPLE_OpenFailed, "Expected File name.");
        return "";
    }

    // If the string finishes in "I.rel" we consider it can be
    // the associated file to all bands that are documented in this file.
    if (cpl::ends_with(osFileName, pszExtRasterREL))
    {
        m_bIsAMiraMonFile = true;
        return osFileName;
    }

    // If the file is not a REL file, let's try to find the associated REL
    // It must be a IMG file.
    CPLString osExtension = CPLString(CPLGetExtensionSafe(osFileName).c_str());
    if (!EQUAL(osExtension, pszExtRaster + 1))
        return "";

    // Converting FileName.img to FileNameI.rel
    CPLString osRELFile = MMRGetSimpleMetadataName(osFileName);
    if (osRELFile.empty())
    {
        if (m_bIsAMiraMonFile)
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Failing in conversion from .img to I.rel for %s file",
                     osFileName.c_str());
        }
        return "";
    }

    // Checking if the file exists
    VSIStatBufL sStat;
    if (VSIStatExL(osRELFile.c_str(), &sStat, VSI_STAT_EXISTS_FLAG) == 0)
        return MMRGetAReferenceToIMGFile(osFileName, osRELFile);

    // If the file I.rel doesn't exist then it has to be found
    // in the same folder than the .img file.
    const CPLString osPath = CPLGetPathSafe(osFileName);
    const CPLStringList folder(VSIReadDir(osPath.c_str()));
    const int size = folder.size();

    for (int nIFile = 0; nIFile < size; nIFile++)
    {
        if (folder[nIFile][0] == '.' || !strstr(folder[nIFile], "I.rel"))
        {
            continue;
        }

        const CPLString osFilePath =
            CPLFormFilenameSafe(osPath, folder[nIFile], nullptr);

        osRELFile = MMRGetAReferenceToIMGFile(osFileName, osFilePath);
        if (!osRELFile.empty())
            return osRELFile;
    }

    if (m_bIsAMiraMonFile)
    {
        CPLError(CE_Failure, CPLE_OpenFailed, "REL search failed for %s file",
                 osFileName.c_str());
    }

    return "";
}

/************************************************************************/
/*                           CheckBandInRel()                           */
/************************************************************************/
CPLErr MMRRel::CheckBandInRel(const CPLString &osRELFileName,
                              const CPLString &osIMGFile)

{
    CPLString osFieldNames;
    if (!GetMetadataValueDirectly(osRELFileName, SECTION_ATTRIBUTE_DATA,
                                  Key_IndexesNomsCamps, osFieldNames) ||
        osFieldNames.empty())
        return CE_Failure;

    // Separator ,
    const CPLStringList aosTokens(CSLTokenizeString2(osFieldNames, ",", 0));
    const int nTokenCount = CSLCount(aosTokens);

    if (!nTokenCount)
        return CE_Failure;

    CPLString osBandSectionKey;
    CPLString osBandSectionValue;
    for (int nIBand = 0; nIBand < nTokenCount; nIBand++)
    {
        osBandSectionKey = KEY_NomCamp;
        osBandSectionKey.append("_");
        osBandSectionKey.append(aosTokens[nIBand]);

        if (!GetMetadataValueDirectly(osRELFileName, SECTION_ATTRIBUTE_DATA,
                                      osBandSectionKey, osBandSectionValue) ||
            osBandSectionValue.empty())
            return CE_Failure;

        CPLString osAttributeDataName;
        osAttributeDataName = SECTION_ATTRIBUTE_DATA;
        osAttributeDataName.append(":");
        osAttributeDataName.append(osBandSectionValue.Trim());

        CPLString osRawBandFileName;

        if (!GetMetadataValueDirectly(osRELFileName, osAttributeDataName,
                                      KEY_NomFitxer, osRawBandFileName) ||
            osRawBandFileName.empty())
        {
            CPLString osBandFileName =
                MMRGetFileNameFromRelName(osRELFileName, pszExtRaster);
            if (osBandFileName.empty())
                return CE_Failure;
        }
        else
        {
            if (!EQUAL(osRawBandFileName, osIMGFile))
                continue;
            break;  // Found
        }
    }

    return CE_None;
}

int MMRRel::IdentifySubdataSetFile(const CPLString &osFileName)
{
    const CPLString osMMRPrefix = "MiraMonRaster:";
    if (!STARTS_WITH(osFileName, osMMRPrefix))
        return FALSE;

    // SUBDATASETS
    size_t nPos = osFileName.ifind(osMMRPrefix);
    if (nPos != 0)
        return GDAL_IDENTIFY_FALSE;

    CPLString osRELAndBandName = osFileName.substr(osMMRPrefix.size());

    const CPLStringList aosTokens(CSLTokenizeString2(osRELAndBandName, ",", 0));
    const int nTokens = CSLCount(aosTokens);
    // Getting the REL associated to the bands
    // We need the REL and at least one band (index + name).
    if (nTokens < 2)
        return GDAL_IDENTIFY_FALSE;

    // Let's remove "\"" if existent.
    CPLString osRELName = aosTokens[0];
    osRELName.replaceAll("\"", "");

    // It must be a I.rel file.
    if (!cpl::ends_with(osRELName, pszExtRasterREL))
        return GDAL_IDENTIFY_FALSE;

    if (MMCheck_REL_FILE(osRELName))
        return GDAL_IDENTIFY_FALSE;

    // Let's see if the specified bands are in the REL file
    // Getting the index + internal names of the bands
    for (int nIBand = 1; nIBand < nTokens; nIBand++)
    {
        // Let's check that this band (papszTokens[nIBand]) is in the REL file.
        CPLString osBandName = aosTokens[nIBand];

        // Let's remove "\"" if existent.
        osBandName.replaceAll("\"", "");

        // If it's not an IMG file return FALSE
        CPLString osExtension =
            CPLString(CPLGetExtensionSafe(osBandName).c_str());
        if (!EQUAL(osExtension, pszExtRaster + 1))
            return GDAL_IDENTIFY_FALSE;

        if (CE_None != CheckBandInRel(osRELName, osBandName))
            return GDAL_IDENTIFY_FALSE;
    }
    return GDAL_IDENTIFY_TRUE;
}

int MMRRel::IdentifyFile(const GDALOpenInfo *poOpenInfo)
{
    // IMG files are shared for many drivers.
    // Identify will mark it as unknown.
    // Open function will try to open that, but as it has computation
    // cost is better avoid doing it here.
    if (poOpenInfo->IsExtensionEqualToCI("IMG"))
        return GDAL_IDENTIFY_UNKNOWN;

    if (!poOpenInfo->IsExtensionEqualToCI("REL"))
        return GDAL_IDENTIFY_FALSE;

    // In fact, the file has to end with I.rel (pszExtRasterREL)
    if (!cpl::ends_with(std::string_view(poOpenInfo->pszFilename),
                        pszExtRasterREL))
        return GDAL_IDENTIFY_FALSE;

    // Some versions of REL files are not allowed.
    if (MMCheck_REL_FILE(poOpenInfo->pszFilename))
        return GDAL_IDENTIFY_FALSE;

    return GDAL_IDENTIFY_TRUE;
}

/************************************************************************/
/*                          GetMetadataValue()                          */
/************************************************************************/
bool MMRRel::GetMetadataValue(const CPLString &osMainSection,
                              const CPLString &osSubSection,
                              const CPLString &osSubSubSection,
                              const CPLString &osKey, CPLString &osValue)
{
    CPLAssert(
        isAMiraMonFile());  // Trying to access metadata from the wrong way

    // Searches in [pszMainSection:pszSubSection]
    CPLString osAttributeDataName;
    osAttributeDataName = osMainSection;
    osAttributeDataName.append(":");
    osAttributeDataName.append(osSubSection);
    osAttributeDataName.append(":");
    osAttributeDataName.append(osSubSubSection);

    addExcludedSectionKey(osAttributeDataName, osKey);
    osValue = GetValueFromSectionKeyFromREL(osAttributeDataName, osKey);
    if (osValue != m_szImprobableRELChain)
        return true;  // Found

    // If the value is not found then searches in [pszMainSection]
    addExcludedSectionKey(osSubSubSection, osKey);
    osValue = GetValueFromSectionKeyFromREL(osSubSubSection, osKey);
    if (osValue == m_szImprobableRELChain)
    {
        osValue = "";
        return false;  // Key not found
    }
    return true;  // Found
}

bool MMRRel::GetMetadataValue(const CPLString &osMainSection,
                              const CPLString &osSubSection,
                              const CPLString &osKey, CPLString &osValue)
{
    CPLAssert(
        isAMiraMonFile());  // Trying to access metadata from the wrong way

    // Searches in [pszMainSection:pszSubSection]
    CPLString osAttributeDataName;
    osAttributeDataName = osMainSection;
    osAttributeDataName.append(":");
    osAttributeDataName.append(osSubSection);

    addExcludedSectionKey(osAttributeDataName, osKey);
    osValue = GetValueFromSectionKeyFromREL(osAttributeDataName, osKey);
    if (osValue != m_szImprobableRELChain)
        return true;  // Found

    // If the value is not found then searches in [pszMainSection]
    addExcludedSectionKey(osMainSection, osKey);
    osValue = GetValueFromSectionKeyFromREL(osMainSection, osKey);
    if (osValue == m_szImprobableRELChain)
    {
        osValue = "";
        return false;  // Key not found
    }
    return true;  // Found
}

bool MMRRel::GetMetadataValue(const CPLString &osSection,
                              const CPLString &osKey, CPLString &osValue)
{
    CPLAssert(
        isAMiraMonFile());  // Trying to access metadata from the wrong way

    addExcludedSectionKey(osSection, osKey);
    osValue = GetValueFromSectionKeyFromREL(osSection, osKey);
    if (osValue == m_szImprobableRELChain)
    {
        osValue = "";
        return false;  // Key not found
    }
    return true;  // Found
}

void MMRRel::UpdateRELNameChar(const CPLString &osRelFileNameIn)
{
    m_osRelFileName = osRelFileNameIn;
}

/************************************************************************/
/*                           ParseBandInfo()                            */
/************************************************************************/
CPLErr MMRRel::ParseBandInfo()
{
    m_nBands = 0;

    CPLString osFieldNames;
    if (!GetMetadataValue(SECTION_ATTRIBUTE_DATA, Key_IndexesNomsCamps,
                          osFieldNames) ||
        osFieldNames.empty())
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "%s-%s section-key should exist in %s.",
                 SECTION_ATTRIBUTE_DATA, Key_IndexesNomsCamps,
                 m_osRelFileName.c_str());
        return CE_Failure;
    }

    // Separator ,
    const CPLStringList aosTokens(CSLTokenizeString2(osFieldNames, ",", 0));
    const int nMaxBands = CSLCount(aosTokens);

    if (!nMaxBands)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed, "No bands in file %s.",
                 m_osRelFileName.c_str());
        return CE_Failure;
    }

    CPLString osBandSectionKey;
    CPLString osBandSectionValue;
    std::set<std::string> setProcessedTokens;

    int nNBand;
    if (m_papoSDSBands.size())
        nNBand = static_cast<int>(m_papoSDSBands.size());
    else
        nNBand = nMaxBands;

    m_oBands.reserve(nNBand);

    for (int nIBand = 0; nIBand < nMaxBands; nIBand++)
    {
        const std::string lowerCaseToken =
            CPLString(aosTokens[nIBand]).tolower();
        if (cpl::contains(setProcessedTokens, lowerCaseToken))
            continue;  // Repeated bands are ignored.

        setProcessedTokens.insert(lowerCaseToken);

        osBandSectionKey = KEY_NomCamp;
        osBandSectionKey.append("_");
        osBandSectionKey.append(aosTokens[nIBand]);

        if (!GetMetadataValue(SECTION_ATTRIBUTE_DATA, osBandSectionKey,
                              osBandSectionValue) ||
            osBandSectionValue.empty())
            continue;

        if (m_papoSDSBands.size())
        {
            CPLString osRawBandFileName;
            if (!GetMetadataValue(SECTION_ATTRIBUTE_DATA, osBandSectionValue,
                                  KEY_NomFitxer, osRawBandFileName) ||
                osRawBandFileName.empty())
                return CE_Failure;

            // I'm in a Subataset
            size_t nISDSBand;
            for (nISDSBand = 0; nISDSBand < m_papoSDSBands.size(); nISDSBand++)
            {
                if (m_papoSDSBands[nISDSBand] == osRawBandFileName)
                    break;
            }
            if (nISDSBand == m_papoSDSBands.size())
                continue;
        }

        if (m_nBands >= nNBand)
            break;

        // MMRBand constructor is called
        m_oBands.emplace_back(*this, osBandSectionValue.Trim());

        if (!m_oBands[m_nBands].IsValid())
        {
            // This band is not been completed
            return CE_Failure;
        }

        m_nBands++;
    }

    return CE_None;
}

int MMRRel::GetColumnsNumberFromREL()
{
    // Number of columns of the subdataset (if exist)
    // Section [OVERVIEW:ASPECTES_TECNICS] in rel file
    CPLString osValue;

    if (!GetMetadataValue(SECTION_OVVW_ASPECTES_TECNICS, "columns", osValue) ||
        osValue.empty())
        return 0;  // Default value

    int nValue;
    if (1 != sscanf(osValue, "%d", &nValue))
        return 0;  // Default value

    return nValue;
}

int MMRRel::GetRowsNumberFromREL()
{
    // Number of columns of the subdataset (if exist)
    // Section [OVERVIEW:ASPECTES_TECNICS] in rel file
    // Key raws
    CPLString osValue;

    if (!GetMetadataValue(SECTION_OVVW_ASPECTES_TECNICS, "rows", osValue) ||
        osValue.empty())
        return 0;  // Default value

    int nValue;
    if (1 != sscanf(osValue, "%d", &nValue))
        return 0;  // Default value

    return nValue;
}

/************************************************************************/
/*                         Preserving metadata                          */
/************************************************************************/
void MMRRel::RELToGDALMetadata(GDALDataset *poDS)
{
    if (!m_pRELFile)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "REL file cannot be opened: \"%s\"", m_osRelFileName.c_str());
        return;
    }

    CPLString osCurrentSection;
    CPLString osPendingKey, osPendingValue;

    auto isExcluded = [&](const CPLString &section, const CPLString &key)
    {
        return GetExcludedMetadata().count({section, key}) ||
               GetExcludedMetadata().count({section, ""});
    };

    const char *pszLine;

    VSIFSeekL(m_pRELFile, 0, SEEK_SET);
    while ((pszLine = CPLReadLine2L(m_pRELFile, 10000, nullptr)) != nullptr)
    {
        CPLString rawLine = pszLine;

        rawLine.Recode(CPL_ENC_ISO8859_1, CPL_ENC_UTF8);
        rawLine.Trim();

        if (rawLine.empty() || rawLine[0] == ';' || rawLine[0] == '#')
            continue;

        if (rawLine[0] == '[' && rawLine[rawLine.size() - 1] == ']')
        {
            // Saves last key
            if (!osPendingKey.empty())
            {
                if (!isExcluded(osCurrentSection, osPendingKey))
                {
                    CPLString fullKey =
                        osCurrentSection + m_SecKeySeparator + osPendingKey;

                    poDS->SetMetadataItem(fullKey.c_str(),
                                          osPendingValue.Trim().c_str(),
                                          m_kMetadataDomain);
                }
                osPendingKey.clear();
                osPendingValue.clear();
            }

            osCurrentSection = rawLine.substr(1, rawLine.size() - 2);
            osCurrentSection.Trim();
            continue;
        }

        size_t equalPos = rawLine.find('=');
        if (equalPos != CPLString::npos)
        {
            // Desa clau anterior
            if (!osPendingKey.empty())
            {
                if (!isExcluded(osCurrentSection, osPendingKey))
                {
                    CPLString fullKey =
                        osCurrentSection + m_SecKeySeparator + osPendingKey;

                    poDS->SetMetadataItem(fullKey.c_str(),
                                          osPendingValue.Trim().c_str(),
                                          m_kMetadataDomain);
                }
            }

            osPendingKey = rawLine.substr(0, equalPos);
            osPendingValue = rawLine.substr(equalPos + 1);
            osPendingKey.Trim();
            osPendingValue.Trim();
        }
        else if (!osPendingKey.empty())
        {
            osPendingValue += "\n" + rawLine;
        }
    }

    // Saves last key
    if (!osPendingKey.empty())
    {
        CPLString fullKey = osCurrentSection + m_SecKeySeparator + osPendingKey;
        if (!isExcluded(osCurrentSection, osPendingKey))
            poDS->SetMetadataItem(fullKey.c_str(),
                                  osPendingValue.Trim().c_str(),
                                  m_kMetadataDomain);
    }
}

CPLErr MMRRel::UpdateGDALColorEntryFromBand(CPLString m_osBandSection,
                                            GDALColorEntry &m_sConstantColorRGB)
{
    // Example: Color_Smb=(255,0,255)
    CPLString os_Color_Smb;
    if (!GetMetadataValue(SECTION_COLOR_TEXT, m_osBandSection, "Color_Smb",
                          os_Color_Smb))
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
                     "Invalid constant color: \"%s\"", GetRELNameChar());
            return CE_Failure;
        }

        int nIColor0;
        if (1 != sscanf(aosTokens[0], "%d", &nIColor0))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid constant color: \"%s\"", GetRELNameChar());
            return CE_Failure;
        }

        int nIColor1;
        if (1 != sscanf(aosTokens[1], "%d", &nIColor1))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid constant color: \"%s\"", GetRELNameChar());
            return CE_Failure;
        }

        int nIColor2;
        if (1 != sscanf(aosTokens[2], "%d", &nIColor2))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid constant color: \"%s\"", GetRELNameChar());
            return CE_Failure;
        }

        m_sConstantColorRGB.c1 = static_cast<short>(nIColor0);
        m_sConstantColorRGB.c2 = static_cast<short>(nIColor1);
        m_sConstantColorRGB.c3 = static_cast<short>(nIColor2);
    }
    return CE_None;
}

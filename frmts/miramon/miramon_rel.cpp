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
#include "cpl_time.h"
#include <set>

#include "miramon_rel.h"
#include "miramon_band.h"

#include "../miramon_common/mm_gdal_functions.h"  // For MMCheck_REL_FILE()

CPLString MMRRel::m_szImprobableRELChain = "@#&%$|``|$%&#@";

/************************************************************************/
/*                               MMRRel()                               */
/************************************************************************/
MMRRel::MMRRel(const CPLString &osRELFilenameIn, bool bIMGMustExist)
    : m_osRelFileName(osRELFilenameIn), m_szFileIdentifier("")
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

MMRRel::MMRRel(const CPLString &osRELFilenameIn, bool bNeedOfNomFitxer,
               const CPLString &osEPSG, int nWidth, int nHeight, double dfMinX,
               double dfMaxX, double dfMinY, double dfMaxY,
               std::vector<MMRBand> &&oBands)
    : m_osRelFileName(osRELFilenameIn), m_szFileIdentifier(""),
      m_oBands(std::move(oBands)), m_bNeedOfNomFitxer(bNeedOfNomFitxer),
      m_osEPSG(osEPSG), m_nWidth(nWidth), m_nHeight(nHeight), m_dfMinX(dfMinX),
      m_dfMaxX(dfMaxX), m_dfMinY(dfMinY), m_dfMaxY(dfMaxY)
{
    m_nBands = static_cast<int>(m_oBands.size());

    if (!m_nBands)
        return;

    // Getting the title of the rel
    m_osTitle = MMRGetFileNameWithOutI(CPLGetBasenameSafe(osRELFilenameIn));

    m_bIsAMiraMonFile = true;
    m_bIsValid = true;
}

MMRRel::MMRRel(const CPLString &osRELFilenameIn)
    : m_osRelFileName(osRELFilenameIn), m_osTitle(""), m_szFileIdentifier(""),
      m_bIsValid(true), m_bIsAMiraMonFile(true), m_nBands(0)
{
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
                                            const CPLString &osExtension)
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
    if (!m_pRELFile || !poDS)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "REL file cannot be opened: \"%s\"", m_osRelFileName.c_str());
        return;
    }

    CPLString osCurrentSection;
    CPLString osPendingKey, osPendingValue;

    auto isExcluded = [&](const CPLString &osSection, const CPLString &osKey)
    {
        return GetExcludedMetadata().count({osSection, osKey}) ||
               GetExcludedMetadata().count({osSection, ""});
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
                        osCurrentSection.replaceAll(":", IntraSecKeySeparator) +
                        SecKeySeparator + osPendingKey;

                    if (osPendingValue.Trim().empty())
                        osPendingValue = MMEmptyValue;
                    poDS->SetMetadataItem(fullKey.c_str(),
                                          osPendingValue.Trim().c_str(),
                                          MetadataDomain);
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
            // Saves last key
            if (!osPendingKey.empty())
            {
                if (!isExcluded(osCurrentSection, osPendingKey))
                {
                    CPLString fullKey =
                        osCurrentSection.replaceAll(":", IntraSecKeySeparator) +
                        SecKeySeparator + osPendingKey;

                    if (osPendingValue.Trim().empty())
                        osPendingValue = MMEmptyValue;
                    poDS->SetMetadataItem(fullKey.c_str(),
                                          osPendingValue.Trim().c_str(),
                                          MetadataDomain);
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
        CPLString fullKey =
            osCurrentSection.replaceAll(":", IntraSecKeySeparator) +
            SecKeySeparator + osPendingKey;
        if (!isExcluded(osCurrentSection, osPendingKey))
        {
            if (osPendingValue.Trim().empty())
                osPendingValue = MMEmptyValue;
            poDS->SetMetadataItem(
                fullKey.c_str(), osPendingValue.Trim().c_str(), MetadataDomain);
        }
    }
}

CPLErr MMRRel::UpdateGDALColorEntryFromBand(const CPLString &m_osBandSection,
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

/************************************************************************/
/*                             Writing part                             */
/************************************************************************/

void MMRRel::UpdateLineage(CSLConstList papszOptions, GDALDataset &oSrcDS)
{
    m_osInFile = oSrcDS.GetDescription();
    m_osOutFile = m_osRelFileName;

    m_aosOptions = papszOptions;
}

bool MMRRel::Write(GDALDataset &oSrcDS)
{
    // REL File creation
    if (!CreateRELFile())
        return false;

    AddRELVersion();

    // Writing METADADES section
    WriteMETADADES();  // It fills m_szFileIdentifier

    // Writing IDENTIFICATION section
    WriteIDENTIFICATION();

    // Writing OVERVIEW:ASPECTES_TECNICS
    WriteOVERVIEW_ASPECTES_TECNICS(oSrcDS);

    // Writing SPATIAL_REFERENCE_SYSTEM:HORIZONTAL
    WriteSPATIAL_REFERENCE_SYSTEM_HORIZONTAL();

    // Writing EXTENT section
    WriteEXTENT();

    // Writing OVERVIEW section
    WriteOVERVIEW();

    // Writing ATTRIBUTE_DATA section
    if (!WriteATTRIBUTE_DATA(oSrcDS))
    {
        CloseRELFile();
        return false;
    }

    // Writing visualization sections
    WriteCOLOR_TEXTSection();
    WriteVISU_LLEGENDASection();

    // Writing lineage
    WriteLINEAGE(oSrcDS);

    CloseRELFile();
    return true;
}

// Writes METADADES section
void MMRRel::WriteMETADADES()
{
    if (!GetRELFile())
        return;

    AddSectionStart(SECTION_METADADES);
    AddKeyValue(KEY_language, KEY_Value_eng);
    AddKeyValue(KEY_MDIdiom, KEY_Value_eng);

    char aMessage[MM_MESSAGE_LENGTH];
    CPLString osFileName = CPLGetBasenameSafe(m_osRelFileName);
    CPLStrlcpy(aMessage, osFileName, sizeof(aMessage));
    MMGenerateFileIdentifierFromMetadataFileName(aMessage, m_szFileIdentifier);

    AddKeyValue(KEY_FileIdentifier, m_szFileIdentifier);
    AddKeyValue(KEY_characterSet, KEY_Value_characterSet);
    AddSectionEnd();
}

// Writes IDENTIFICATION section
void MMRRel::WriteIDENTIFICATION()
{
    if (!GetRELFile())
        return;

    AddSectionStart(SECTION_IDENTIFICATION);
    AddKeyValue(KEY_code, m_szFileIdentifier);
    AddKey(KEY_codeSpace);

    if (!m_osTitle.empty())
        AddKeyValue(KEY_DatasetTitle, m_osTitle);
    AddSectionEnd();
}

// Writes OVERVIEW:ASPECTES_TECNICS section
void MMRRel::WriteOVERVIEW_ASPECTES_TECNICS(GDALDataset &oSrcDS)
{
    if (!GetRELFile())
        return;

    if (m_nWidth && m_nHeight)
    {
        AddSectionStart(SECTION_OVERVIEW, SECTION_ASPECTES_TECNICS);
        AddKeyValue("columns", m_nWidth);
        AddKeyValue("rows", m_nHeight);

        WriteMetadataInComments(oSrcDS);

        AddSectionEnd();
        m_bDimAlreadyWritten = TRUE;
    }
}

void MMRRel::WriteMetadataInComments(GDALDataset &oSrcDS)
{
    // Writing domain MIRAMON metadata to a section in REL that is used to
    // add comments. It's could be useful if some raster contains METADATA from
    // MiraMon and an expert user wants to recover and use it.
    const CSLConstList aosMiraMonMetaData(oSrcDS.GetMetadata(MetadataDomain));
    int nComment = 1;
    CPLString osValue;
    CPLString osCommentValue;
    size_t nPos, nPos2;
    CPLString osRecoveredMD;
    if (oSrcDS.GetDescription())
        osRecoveredMD =
            CPLSPrintf("Recovered MIRAMON domain metadata from '%s'",
                       oSrcDS.GetDescription());
    else
        osRecoveredMD = "Recovered MIRAMON domain metadata";

    CPLString osQUALITYLINEAGE = "QUALITY";
    osQUALITYLINEAGE.append(IntraSecKeySeparator);
    osQUALITYLINEAGE.append("LINEAGE");

    CPLString osCommenti;
    for (const auto &[pszKey, pszValue] :
         cpl::IterateNameValue(aosMiraMonMetaData))
    {
        osCommenti = CPLSPrintf("comment%d", nComment);

        CPLString osAux = pszKey;
        nPos = osAux.find(SecKeySeparator);
        if (nPos == std::string::npos)
            continue;

        CPLString osSection = osAux.substr(0, nPos);

        // Section lineage is written in another section
        nPos2 = osSection.find(osQUALITYLINEAGE);
        if (nPos2 != std::string::npos)
            continue;

        osSection.replaceAll(IntraSecKeySeparator, ":");
        osCommentValue = CPLSPrintf(
            "[%s]->%s=%s", osSection.c_str(),
            osAux.substr(nPos + strlen(SecKeySeparator)).c_str(), pszValue);

        if (!osRecoveredMD.empty())
        {
            AddKeyValue(osCommenti, osRecoveredMD);
            osCommenti = CPLSPrintf("comment%d", ++nComment);
            osRecoveredMD.clear();
        }

        AddKeyValue(osCommenti, osCommentValue);
        nComment++;
    }
}

// Writes SPATIAL_REFERENCE_SYSTEM:HORIZONTAL section
void MMRRel::WriteSPATIAL_REFERENCE_SYSTEM_HORIZONTAL()
{
    if (!GetRELFile())
        return;

    AddSectionStart(SECTION_SPATIAL_REFERENCE_SYSTEM, SECTION_HORIZONTAL);
    char aMMIDSRS[MM_MAX_ID_SNY];
    if (!ReturnMMIDSRSFromEPSGCodeSRS(m_osEPSG, aMMIDSRS) &&
        !MMIsEmptyString(aMMIDSRS))
    {
        AddKeyValue(KEY_HorizontalSystemIdentifier, aMMIDSRS);
    }
    else
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "The MiraMon driver cannot assign any HRS.");
        // Horizontal Reference System
        AddKeyValue(KEY_HorizontalSystemIdentifier, "plane");
        AddKeyValue(KEY_HorizontalSystemDefinition, "local");
    }
    AddSectionEnd();
}

// Writes EXTENT section
void MMRRel::WriteEXTENT()
{
    if (!GetRELFile())
        return;

    AddSectionStart(SECTION_EXTENT);
    if (m_dfMinX != MM_UNDEFINED_STATISTICAL_VALUE &&
        m_dfMaxX != -MM_UNDEFINED_STATISTICAL_VALUE &&
        m_dfMinY != MM_UNDEFINED_STATISTICAL_VALUE &&
        m_dfMaxY != -MM_UNDEFINED_STATISTICAL_VALUE)
    {
        AddKeyValue(KEY_MinX, m_dfMinX);
        AddKeyValue(KEY_MaxX, m_dfMaxX);
        AddKeyValue(KEY_MinY, m_dfMinY);
        AddKeyValue(KEY_MaxY, m_dfMaxY);
    }
    AddKeyValue(KEY_toler_env, 0);
    AddSectionEnd();
}

// Writes OVERVIEW section
void MMRRel::WriteOVERVIEW()
{
    if (!GetRELFile())
        return;

    AddSectionStart(SECTION_OVERVIEW);
    time_t currentTime = time(nullptr);
    struct tm ltime;
    VSILocalTime(&currentTime, &ltime);
    char aTimeString[200];
    snprintf(aTimeString, sizeof(aTimeString), "%04d%02d%02d %02d%02d%02d%02d",
             ltime.tm_year + 1900, ltime.tm_mon + 1, ltime.tm_mday,
             ltime.tm_hour, ltime.tm_min, ltime.tm_sec, 0);
    AddKeyValue(KEY_CreationDate, aTimeString);
    AddKeyValue(KEY_CoverageContentType, "001");  // Raster image
    AddSectionEnd();
}

// Writes ATTRIBUTE_DATA section
bool MMRRel::WriteATTRIBUTE_DATA(GDALDataset &oSrcDS)
{
    if (!GetRELFile())
        return false;

    if (!m_nBands)
        return false;

    AddSectionStart(SECTION_ATTRIBUTE_DATA);

    const CPLString osDSDataType = m_oBands[0].GetRELDataType();
    if (osDSDataType.empty())
        return false;

    AddKeyValue("TipusCompressio", osDSDataType);
    AddKeyValue("TipusRelacio", "RELACIO_1_1_DICC");

    // TractamentVariable by default
    m_osDefTractVariable =
        m_oBands[0].IsCategorical() ? "Categoric" : "QuantitatiuContinu";
    AddKeyValue(KEY_TractamentVariable, m_osDefTractVariable);

    // Units by default
    m_osDefUnits = m_oBands[0].GetUnits();
    if (m_osDefUnits.empty())
        AddKeyValue(KEY_MostrarUnitats, "0");
    else
        AddKeyValue(KEY_unitats, m_osDefUnits);

    // Key_IndexesNomsCamps
    CPLString osIndexsNomsCamps = "1";
    CPLString osIndex;
    for (int nIBand = 1; nIBand < m_nBands; nIBand++)
    {
        osIndex = CPLSPrintf(",%d", nIBand + 1);
        osIndexsNomsCamps.append(osIndex);
    }
    AddKeyValue(Key_IndexesNomsCamps, osIndexsNomsCamps);

    // Writing bands names
    CPLString osIndexKey, osIndexValue;
    for (int nIBand = 0; nIBand < m_nBands; nIBand++)
    {
        osIndexKey = CPLSPrintf("NomCamp_%d", nIBand + 1);
        osIndexValue = m_oBands[nIBand].GetBandSection();
        AddKeyValue(osIndexKey, osIndexValue);
    }
    AddSectionEnd();

    // Writing bands sections: particular band information
    for (int nIBand = 0; nIBand < m_nBands; nIBand++)
    {
        // Writing IMG binary file. This calculates min and max values of the band
        if (!m_oBands[nIBand].WriteBandFile(oSrcDS, m_nBands, nIBand))
            return false;

        // Adding band information to REL. This writes min max values of the band
        WriteBandSection(m_oBands[nIBand], osDSDataType);
    }

    return true;
}

void MMRRel::WriteBandSection(const MMRBand &osBand,
                              const CPLString &osDSDataType)
{
    if (osBand.GetBandSection().empty())
        return;  // It's not an error.

    CPLString osSection = "ATTRIBUTE_DATA";
    osSection.append(":");
    osSection.append(osBand.GetBandSection());

    AddSectionStart(osSection);
    CPLString osDataType = osBand.GetRELDataType();
    if (!EQUAL(osDSDataType, osDataType))
        AddKeyValue("TipusCompressio", osDataType);

    // TractamentVariable of the band (only written if different from default)
    CPLString osTractVariable =
        osBand.IsCategorical() ? "Categoric" : "QuantitatiuContinu";
    if (!EQUAL(m_osDefTractVariable, osTractVariable))
        AddKeyValue(KEY_TractamentVariable, osTractVariable);

    // Units of the band (only written if different from default)
    CPLString osUnits = osBand.GetUnits();
    if (!EQUAL(m_osDefUnits, osUnits))
    {
        if (osUnits.empty())
            AddKeyValue(KEY_MostrarUnitats, "0");
        else
        {
            AddKeyValue(KEY_MostrarUnitats, "1");
            AddKeyValue(KEY_unitats, osUnits);
        }
    }

    // If there is need of NomFitxer this is the place to wrote it.
    if (!osBand.GetRawBandFileName().empty() && m_bNeedOfNomFitxer)
        AddKeyValue(KEY_NomFitxer, osBand.GetRawBandFileName());

    // Description
    if (!osBand.GetFriendlyDescription().empty())
        AddKeyValue(KEY_descriptor, osBand.GetFriendlyDescription());

    if (osBand.BandHasNoData())
    {
        AddKeyValue("NODATA", osBand.GetNoDataValue());
        AddKeyValue("NODATADef", "NODATA");

        if (osBand.GetMin() == osBand.GetNoDataValue())
            AddKeyValue("min", osBand.GetMin() + 1);
        else
            AddKeyValue("min", osBand.GetMin());

        if (osBand.GetMax() == osBand.GetNoDataValue())
            AddKeyValue("max", osBand.GetMax() - 1);
        else
            AddKeyValue("max", osBand.GetMax());
    }
    else
    {
        AddKeyValue("min", osBand.GetMin());
        AddKeyValue("max", osBand.GetMax());
    }

    CPLString osIJT = "";
    if (!osBand.GetAttributeTableDBFNameFile().empty())
    {
        osIJT = osBand.GetBandSection();
        osIJT.append("_DBF");
        AddKeyValue("IndexsJoinTaula", osIJT);

        CPLString osJT = "JoinTaula_";
        osJT.append(osIJT);
        AddKeyValue(osJT, osIJT);
    }
    AddSectionEnd();

    if (!osIJT.empty())
    {
        CPLString osTAULA_SECTION = "TAULA_";
        osTAULA_SECTION.append(osIJT);
        AddSectionStart(osTAULA_SECTION);
        AddKeyValue(KEY_NomFitxer,
                    CPLGetFilename(osBand.GetAttributeTableRELNameFile()));
        AddSectionEnd();
    }
}

CPLString MMRRel::GetColor_TractamentVariable(int nIBand) const
{
    if (m_oBands[nIBand].IsCategorical())
        return "Categoric";
    else
        return "QuantitatiuContinu";
}

CPLString MMRRel::GetColor_Paleta(int nIBand) const
{
    if (!m_oBands[nIBand].GetColorTableNameFile().empty())
    {
        CPLString osRELPath = CPLGetPathSafe(m_osRelFileName);
        return CPLExtractRelativePath(
            osRELPath, m_oBands[nIBand].GetColorTableNameFile(), nullptr);
    }
    else
        return "<Automatic>";
}

void MMRRel::WriteCOLOR_TEXTSection()
{
    if (!GetRELFile())
        return;

    if (!m_nBands)
        return;

    AddSectionStart(SECTION_COLOR_TEXT);
    AddCOLOR_TEXTVersion();
    AddKeyValue("UnificVisCons", 1);
    AddKeyValue("visualitzable", 1);
    AddKeyValue("consultable", 1);
    AddKeyValue("EscalaMaxima", 0);
    AddKeyValue("EscalaMinima", 900000000);
    AddKeyValue("Color_Const", 0);

    // Setting the default values of "DefaultTractamentVariable"
    CPLString osDefaultColor_TractamentVariable =
        GetColor_TractamentVariable(0);
    AddKeyValue("Color_TractamentVariable", osDefaultColor_TractamentVariable);

    // Setting the default values of "Color_Paleta"
    CPLString os_DefaultColor_Paleta = GetColor_Paleta(0);
    AddKeyValue("Color_Paleta", os_DefaultColor_Paleta);

    AddKeyValue("Tooltips_Const", 1);
    AddSectionEnd();

    // Different key values from the first one in a new section
    // Band 0 has been already documented. Let's check other bands.
    for (int nIBand = 1; nIBand < m_nBands; nIBand++)
    {
        CPLString osSection = SECTION_COLOR_TEXT;
        osSection.append(":");
        osSection.append(m_oBands[nIBand].GetBandSection());
        bool bSectionStarted = false;

        CPLString osColor_TractamentVariable =
            GetColor_TractamentVariable(nIBand);
        if (!EQUAL(osDefaultColor_TractamentVariable,
                   osColor_TractamentVariable))
        {
            AddSectionStart(osSection);
            bSectionStarted = true;
            AddKeyValue("Color_TractamentVariable", osColor_TractamentVariable);
        }

        CPLString osColor_Paleta = GetColor_Paleta(nIBand);
        if (!EQUAL(os_DefaultColor_Paleta, osColor_Paleta))
        {
            if (!bSectionStarted)
                AddSectionStart(osSection);
            AddKeyValue("Color_Paleta", osColor_Paleta);
        }

        AddSectionEnd();
    }
}

void MMRRel::WriteVISU_LLEGENDASection()
{
    AddSectionStart(SECTION_VISU_LLEGENDA);

    AddVISU_LLEGENDAVersion();
    AddKeyValue("Color_VisibleALleg", 1);
    AddKeyValue("Color_TitolLlegenda", "");
    AddKeyValue("Color_CategAMostrar", "N");
    AddKeyValue("Color_InvertOrdPresentColorLleg", 0);
    AddKeyValue("Color_MostrarIndColorLleg", 0);
    AddKeyValue("Color_MostrarValColorLleg", 0);
    AddKeyValue("Color_MostrarCatColorLleg", 1);
    AddKeyValue("Color_MostrarNODATA", 0);
    AddKeyValue("Color_MostrarEntradesBuides", 0);
    AddKeyValue("Color_NovaColumnaLlegImpresa", 0);

    AddSectionEnd();
}

void MMRRel::WriteLINEAGE(GDALDataset &oSrcDS)
{
    ImportAndWriteLineageSection(oSrcDS);
    WriteCurrentProcess();
    EndProcessesSection();
}

void MMRRel::EndProcessesSection()
{
    if (!m_nNProcesses)
        return;
    AddSectionStart(SECTION_QUALITY_LINEAGE);
    AddKeyValue("processes", m_osListOfProcesses);
    AddSectionEnd();
}

void MMRRel::WriteCurrentProcess()
{
    // Checking the current processes. If it's too much,
    // then we cannot write lineage information.
    if (nILastProcess >= INT_MAX - 1)
        return;

    nILastProcess++;
    CPLString osCurrentProcess = CPLSPrintf("%d", nILastProcess);

    CPLString osSection = SECTION_QUALITY_LINEAGE;
    osSection.append(":PROCESS");
    osSection.append(osCurrentProcess);

    AddSectionStart(osSection);
    AddKeyValue("purpose", "GDAL process");

    struct tm ltime;
    char aTimeString[200];
    time_t currentTime = time(nullptr);

    VSILocalTime(&currentTime, &ltime);
    snprintf(aTimeString, sizeof(aTimeString), "%04d%02d%02d %02d%02d%02d%02d",
             ltime.tm_year + 1900, ltime.tm_mon + 1, ltime.tm_mday,
             ltime.tm_hour, ltime.tm_min, ltime.tm_sec, 0);

    AddKeyValue("date", aTimeString);
    AddKeyValue("NomFitxer", "gdal");
    AddSectionEnd();

    int nInOut = 1;
    if (!m_osInFile.empty())
    {
        WriteINOUTSection(osSection, nInOut, "InFile", "", "C", m_osInFile);
        nInOut++;
    }

    if (!m_osOutFile.empty())
    {
        WriteINOUTSection(osSection, nInOut, "OutFile", "1", "C", m_osOutFile);
        nInOut++;
    }

    for (const auto &[pszKey, pszValue] : cpl::IterateNameValue(m_aosOptions))
    {
        CPLString osIdentifierValue = "-co ";
        osIdentifierValue.append(pszKey);
        WriteINOUTSection(osSection, nInOut, osIdentifierValue, "", "C",
                          pszValue);
        nInOut++;
    }
    if (m_nNProcesses)
        m_osListOfProcesses.append(",");

    m_osListOfProcesses.append(osCurrentProcess);
    m_nNProcesses++;
}

void MMRRel::WriteINOUTSection(const CPLString &osSection, int nInOut,
                               const CPLString &osIdentifierValue,
                               const CPLString &osSentitValue,
                               const CPLString &osTypeValuesValue,
                               const CPLString &osResultValueValue)
{
    CPLString osSectionIn = osSection;
    osSectionIn.append(":INOUT");
    osSectionIn.append(CPLSPrintf("%d", nInOut));

    AddSectionStart(osSectionIn);
    if (!osIdentifierValue.empty())
        AddKeyValue("identifier", osIdentifierValue);
    if (!osSentitValue.empty())
        AddKeyValue("sentit", osSentitValue);
    if (!osTypeValuesValue.empty())
        AddKeyValue("TypeValues", osTypeValuesValue);
    if (!osResultValueValue.empty())
        AddKeyValue("ResultValue", osResultValueValue);
    AddKeyValue("ResultUnits", "");
    AddSectionEnd();
}

// This function imports lineage information from the source dataset
// and writes it in rel file (QUALITY:LINEAGE section).
void MMRRel::ImportAndWriteLineageSection(GDALDataset &oSrcDS)
{
    CPLStringList aosMiraMonMetaData(oSrcDS.GetMetadata(MetadataDomain));
    if (aosMiraMonMetaData.empty())
        return;

    m_nNProcesses = 0;

    CPLString osLineageProcessesKey = CPLSPrintf(
        "QUALITY%sLINEAGE%sprocesses", IntraSecKeySeparator, SecKeySeparator);
    CPLString osListOfProcesses =
        CSLFetchNameValueDef(aosMiraMonMetaData, osLineageProcessesKey, "");
    if (osListOfProcesses.empty())
        return;

    const CPLStringList aosTokens(
        CSLTokenizeString2(osListOfProcesses, ",", 0));
    const int nTokens = CSLCount(aosTokens);
    // No processes to import
    if (nTokens == 0)
        return;

    // Too much processes, it's not possible.
    if (nTokens >= INT_MAX)
        return;

    // Getting the list of metadata in MIRAMON domain and
    // converting to CPLStringList for sorting (necessary to write into the REL)
    CPLStringList aosMiraMonSortedMetaData(oSrcDS.GetMetadata(MetadataDomain));
    aosMiraMonSortedMetaData.Sort();

    int nIProcess;
    m_nNProcesses = 0;
    int nLastValidIndex = -1;
    for (nIProcess = 0; nIProcess < nTokens; nIProcess++)
    {
        CPLString osProcessSection =
            CPLSPrintf("QUALITY%sLINEAGE%sPROCESS%s", IntraSecKeySeparator,
                       IntraSecKeySeparator, aosTokens[nIProcess]);
        if (!ProcessProcessSection(aosMiraMonSortedMetaData, osProcessSection))
            break;  // If some section have a problem, we stop reading the lineage.

        nLastValidIndex = nIProcess;
        if (m_nNProcesses)
            m_osListOfProcesses.append(",");
        m_osListOfProcesses.append(CPLSPrintf("%s", aosTokens[nIProcess]));
        m_nNProcesses++;
    }

    if (nLastValidIndex >= 0)
    {
        if (1 != sscanf(aosTokens[nLastValidIndex], "%d", &nILastProcess))
            nILastProcess = 0;
    }
}

// This function processes a process section and its eventual subsections.
// It returns true if it has found the section and processed it, false otherwise.
bool MMRRel::ProcessProcessSection(
    const CPLStringList &aosMiraMonSortedMetaData,
    const CPLString &osProcessSection)
{
    CPLString osProcess = osProcessSection;
    osProcess.append(SecKeySeparator);
    osProcess.append("");

    CPLString osExpectedProcessKey;
    const size_t nStart = strlen(osProcessSection);
    const size_t nSecLen = strlen(SecKeySeparator);
    const size_t nIntraSecLen = strlen(IntraSecKeySeparator);

    // Main section
    bool bStartSectionDone = false;
    bool bSomethingInSection = false;
    for (const auto &[pszKey, pszValue] :
         cpl::IterateNameValue(aosMiraMonSortedMetaData))
    {
        if (pszKey && STARTS_WITH(pszKey, osProcessSection.c_str()))
        {
            CPLString osKey = pszKey;
            size_t nPos = osKey.find(SecKeySeparator);
            if (nPos == std::string::npos)
                continue;

            if (osKey.size() < nStart + nSecLen)
                continue;
            if (osKey.compare(nStart, nSecLen, SecKeySeparator) != 0)
                continue;

            // We are in the section we are looking for
            if (!bStartSectionDone)
            {
                bStartSectionDone = true;
                bSomethingInSection = true;
                CPLString osFinalSection = osKey.substr(0, nStart);
                osFinalSection.replaceAll(IntraSecKeySeparator, ":");
                AddSectionStart(osFinalSection);
            }
            AddKeyValue(osKey.substr(nStart + nSecLen), pszValue);
        }
    }
    if (!bSomethingInSection)
        return false;

    AddSectionEnd();

    // Subsections
    bool bCloseLastSubSection = false;
    CPLString osFinalSection = "";
    for (const auto &[pszKey, pszValue] :
         cpl::IterateNameValue(aosMiraMonSortedMetaData))
    {
        if (pszKey && STARTS_WITH(pszKey, osProcessSection.c_str()))
        {
            CPLString osKey = pszKey;
            size_t nPos = osKey.find(SecKeySeparator);
            if (nPos == std::string::npos)
                continue;

            if (osKey.size() < nStart + nIntraSecLen)
                continue;
            if (osKey.compare(nStart, nIntraSecLen, IntraSecKeySeparator) != 0)
                continue;

            // It cannot be the main section
            if (osKey.size() >= nStart + nSecLen &&
                osKey.compare(nStart, nSecLen, SecKeySeparator) == 0)
                continue;

            // We are in the subsection we are looking for
            if (!STARTS_WITH(osFinalSection.c_str(),
                             osKey.substr(0, nPos).c_str()))
            {
                if (bCloseLastSubSection)
                    AddSectionEnd();
                bCloseLastSubSection = true;
                CPLString osFinalSectionDecod = osKey.substr(0, nPos);
                osFinalSectionDecod.replaceAll(IntraSecKeySeparator, ":");
                AddSectionStart(osFinalSectionDecod);
            }
            AddKeyValue(osKey.substr(nPos + nSecLen), pszValue);

            osFinalSection = osKey.substr(0, nPos);
        }
    }
    if (bCloseLastSubSection)
        AddSectionEnd();

    return true;
}

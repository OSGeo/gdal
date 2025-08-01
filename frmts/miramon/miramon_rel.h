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

#ifndef MMR_REL_H_INCLUDED
#define MMR_REL_H_INCLUDED

#include "cpl_string.h"
#include "gdal_priv.h"
#include "miramon_band.h"  // For MMRBand

constexpr auto pszExtRaster = ".img";
constexpr auto pszExtRasterREL = "I.rel";
constexpr auto pszExtREL = ".rel";

class MMRBand;

/************************************************************************/
/*                               MMRRel                                */
/************************************************************************/

enum class MMRNomFitxerState
{
    NOMFITXER_NOT_FOUND,        // There is no NomFitxer key
    NOMFITXER_VALUE_EXPECTED,   // The NomFitxer value is the expected
    NOMFITXER_VALUE_EMPTY,      // The NomFitxer value is empty
    NOMFITXER_VALUE_UNEXPECTED  // The NomFitxer value is unexpected
};

using ExcludedEntry = std::pair<CPLString, CPLString>;

class MMRRel
{
  public:
    MMRRel(CPLString, bool);
    MMRRel(const MMRRel &) =
        delete;  // I don't want to construct a MMRDataset from another MMRDataset (effc++)
    MMRRel &operator=(const MMRRel &) =
        delete;  // I don't want to assing a MMRDataset to another MMRDataset (effc++)
    ~MMRRel();

    static CPLString
    GetValueFromSectionKeyPriorToREL(const CPLString osPriorRelName,
                                     const CPLString osSection,
                                     const CPLString osKey);
    CPLString GetValueFromSectionKeyFromREL(const CPLString osSection,
                                            const CPLString osKey);
    static CPLString GetValueFromSectionKey(VSILFILE *pf,
                                            const CPLString osSection,
                                            const CPLString osKey);
    bool GetMetadataValue(const CPLString osMainSection,
                          const CPLString osSubSection,
                          const CPLString osSubSubSection,
                          const CPLString osKey, CPLString &osValue);
    bool GetMetadataValue(const CPLString osMainSection,
                          const CPLString osSubSection, const CPLString osKey,
                          CPLString &osValue);
    bool GetMetadataValue(const CPLString osSection, const CPLString osKey,
                          CPLString &osValue);
    bool GetAndExcludeMetadataValueDirectly(const CPLString osRELFile,
                                            const CPLString osSection,
                                            const CPLString osKey,
                                            CPLString &osValue);
    static bool GetMetadataValueDirectly(const CPLString osRELFile,
                                         const CPLString osSection,
                                         const CPLString osKey,
                                         CPLString &osValue);
    void RELToGDALMetadata(GDALDataset *poDS);

    static CPLString MMRGetFileNameFromRelName(const CPLString osRELFile);
    int GetColumnsNumberFromREL();
    int GetRowsNumberFromREL();
    static int IdentifySubdataSetFile(const CPLString osFileName);
    static int IdentifyFile(GDALOpenInfo *poOpenInfo);

    bool IsValid() const
    {
        return bIsValid;
    }

    void SetIsValid(bool bIsValidIn)
    {
        bIsValid = bIsValidIn;
    }

    VSILFILE *GetRELFile() const
    {
        return pRELFile;
    }

    bool OpenRELFile()
    {
        if (osRelFileName.empty())
            return false;

        pRELFile = VSIFOpenL(osRelFileName, "rb");
        if (pRELFile)
            return true;
        return false;
    }

    void CloseRELFile()
    {
        if (!pRELFile)
            return;

        VSIFCloseL(pRELFile);
        pRELFile = nullptr;
    }

    const char *GetRELNameChar() const
    {
        return osRelFileName.c_str();
    }

    CPLString GetRELName() const
    {
        return osRelFileName;
    }

    int GetNBands() const
    {
        return nBands;
    }

    MMRBand **GetBands() const
    {
        return papoBand;
    }

    MMRBand *GetBand(int nIBand) const
    {
        if (nIBand < 0 || nIBand >= nBands)
            return nullptr;

        return papoBand[nIBand];
    }

    int isAMiraMonFile() const
    {
        return bIsAMiraMonFile;
    }

    void addExcludedSectionKey(const CPLString section, const CPLString key)
    {
        ExcludedSectionKey.emplace(section, key);
    }

    std::set<ExcludedEntry> GetExcludedMetadata() const
    {
        return ExcludedSectionKey;
    }

  private:
    static CPLErr CheckBandInRel(CPLString osRELFileName, CPLString osIMGFile);
    static CPLString MMRGetSimpleMetadataName(CPLString osLayerName);
    static bool SameFile(CPLString osFile1, CPLString osFile2);
    MMRNomFitxerState MMRStateOfNomFitxerInSection(CPLString osLayerName,
                                                   CPLString osSection,
                                                   CPLString osRELFile,
                                                   bool bNomFitxerMustExtist);
    CPLString MMRGetAReferenceToIMGFile(CPLString osLayerName,
                                        CPLString osRELFile);

    CPLString GetAssociatedMetadataFileName(CPLString osFileName);

    void UpdateRELNameChar(CPLString osRelFileNameIn);
    CPLErr ParseBandInfo();

    CPLString osRelFileName = "";
    VSILFILE *pRELFile = nullptr;
    static CPLString szImprobableRELChain;

    bool bIsValid = false;  // Determines if the created object is valid or not.
    bool bIsAMiraMonFile = false;

    // List of rawBandNames in a subdataset
    std::vector<CPLString> papoSDSBands{};

    int nBands = 0;
    MMRBand **papoBand = nullptr;

    // Preserving metadata

    // Domain
    const char *kMetadataDomain = "MIRAMON";

    // Used to join Section and Key in a single
    // name for SetMetadataItem(Name, Value)
    const char *SecKeySeparator = "[$$$]";

    // List of excluded pairs {Section, Key} to be added to metadata
    // Empty Key means all section
    std::set<ExcludedEntry> ExcludedSectionKey = {};
};

#endif /* ndef MMR_REL_H_INCLUDED */

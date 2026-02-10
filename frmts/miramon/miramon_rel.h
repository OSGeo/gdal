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

#include "../miramon_common/mm_gdal_driver_structs.h"  // For SECTION_VERSIO

constexpr auto pszExtRaster = ".img";
constexpr auto pszExtRasterREL = "I.rel";
constexpr auto pszExtREL = ".rel";

class MMRBand;

/************************************************************************/
/*                                MMRRel                                */
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
    MMRRel(const CPLString &, bool);
    MMRRel(const MMRRel &) =
        delete;  // I don't want to construct a MMRDataset from another MMRDataset (effc++)
    MMRRel &operator=(const MMRRel &) =
        delete;  // I don't want to assign a MMRDataset to another MMRDataset (effc++)
    ~MMRRel();

    static CPLString
    GetValueFromSectionKeyPriorToREL(const CPLString &osPriorRelName,
                                     const CPLString &osSection,
                                     const CPLString &osKey);
    CPLString GetValueFromSectionKeyFromREL(const CPLString &osSection,
                                            const CPLString &osKey);
    static CPLString GetValueFromSectionKey(VSILFILE *pf,
                                            const CPLString &osSection,
                                            const CPLString &osKey);
    bool GetMetadataValue(const CPLString &osMainSection,
                          const CPLString &osSubSection,
                          const CPLString &osSubSubSection,
                          const CPLString &osKey, CPLString &osValue);
    bool GetMetadataValue(const CPLString &osMainSection,
                          const CPLString &osSubSection, const CPLString &osKey,
                          CPLString &osValue);
    bool GetMetadataValue(const CPLString &osSection, const CPLString &osKey,
                          CPLString &osValue);
    bool GetAndExcludeMetadataValueDirectly(const CPLString &osRELFile,
                                            const CPLString &osSection,
                                            const CPLString &osKey,
                                            CPLString &osValue);
    static bool GetMetadataValueDirectly(const CPLString &osRELFile,
                                         const CPLString &osSection,
                                         const CPLString &osKey,
                                         CPLString &osValue);
    void RELToGDALMetadata(GDALDataset *poDS);

    static CPLString MMRGetFileNameWithOutI(const CPLString &osRELFile);
    static CPLString MMRGetFileNameFromRelName(const CPLString &osRELFile,
                                               const CPLString osExtension);
    int GetColumnsNumberFromREL();
    int GetRowsNumberFromREL();
    static int IdentifySubdataSetFile(const CPLString &osFileName);
    static int IdentifyFile(const GDALOpenInfo *poOpenInfo);
    CPLString GetColor_TractamentVariable(int nIBand);
    CPLString GetColor_Paleta(int nIBand);
    CPLErr UpdateGDALColorEntryFromBand(CPLString m_osBandSection,
                                        GDALColorEntry &m_sConstantColorRGB);

    bool IsValid() const
    {
        return m_bIsValid;
    }

    void SetIsValid(bool bIsValidIn)
    {
        m_bIsValid = bIsValidIn;
    }

    VSILFILE *GetRELFile() const
    {
        return m_pRELFile;
    }

    bool OpenRELFile(const char *pszAccess)
    {
        if (m_osRelFileName.empty())
            return false;

        m_pRELFile = VSIFOpenL(m_osRelFileName, pszAccess);
        if (m_pRELFile)
            return true;
        return false;
    }

    bool OpenRELFile()
    {
        if (m_osRelFileName.empty())
            return false;

        m_pRELFile = VSIFOpenL(m_osRelFileName, "rb");
        if (m_pRELFile)
            return true;
        return false;
    }

    void CloseRELFile()
    {
        if (!m_pRELFile)
            return;

        VSIFCloseL(m_pRELFile);
        m_pRELFile = nullptr;
    }

    const char *GetRELNameChar() const
    {
        return m_osRelFileName.c_str();
    }

    CPLString GetRELName() const
    {
        return m_osRelFileName;
    }

    int GetNBands() const
    {
        return m_nBands;
    }

    MMRBand *GetBand(int nIBand)
    {
        if (nIBand < 0 || nIBand >= m_nBands)
            return nullptr;

        return &m_oBands[nIBand];
    }

    int isAMiraMonFile() const
    {
        return m_bIsAMiraMonFile;
    }

    void addExcludedSectionKey(const CPLString &section, const CPLString &key)
    {
        m_ExcludedSectionKey.emplace(section, key);
    }

    std::set<ExcludedEntry> GetExcludedMetadata() const
    {
        return m_ExcludedSectionKey;
    }

  private:
    static CPLErr CheckBandInRel(const CPLString &osRELFileName,
                                 const CPLString &osIMGFile);
    static CPLString MMRGetSimpleMetadataName(const CPLString &osLayerName);
    static bool SameFile(const CPLString &osFile1, const CPLString &osFile2);
    MMRNomFitxerState MMRStateOfNomFitxerInSection(const CPLString &osLayerName,
                                                   const CPLString &osSection,
                                                   const CPLString &osRELFile,
                                                   bool bNomFitxerMustExist);
    CPLString MMRGetAReferenceToIMGFile(const CPLString &osLayerName,
                                        const CPLString &osRELFile);

    CPLString GetAssociatedMetadataFileName(const CPLString &osFileName);

    void UpdateRELNameChar(const CPLString &osRelFileNameIn);
    CPLErr ParseBandInfo();

    CPLString m_osRelFileName = "";
    VSILFILE *m_pRELFile = nullptr;
    static CPLString m_szImprobableRELChain;

    bool m_bIsValid =
        false;  // Determines if the created object is valid or not.
    bool m_bIsAMiraMonFile = false;

    // List of rawBandNames in a subdataset
    std::vector<CPLString> m_papoSDSBands{};

    int m_nBands = 0;
    std::vector<MMRBand> m_oBands{};

    // Preserving metadata

    // Domain
    static constexpr const char *m_kMetadataDomain = "MIRAMON";

    // Used to join Section and Key in a single
    // name for SetMetadataItem(Name, Value)
    static constexpr const char *m_SecKeySeparator = "[$$$]";

    // List of excluded pairs {Section, Key} to be added to metadata
    // Empty Key means all section
    std::set<ExcludedEntry> m_ExcludedSectionKey = {};
};

#endif /* ndef MMR_REL_H_INCLUDED */

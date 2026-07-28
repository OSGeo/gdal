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
constexpr auto LineReturn = "\r\n";

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
    MMRRel(const CPLString &, bool);  // Used in reading
    MMRRel(const CPLString &, bool bNeedOfNomFitxer, const CPLString &osEPSG,
           int nWidth, int nHeight, double dfMinX, double dfMaxX, double dfMinY,
           double dfMaxY,
           std::vector<MMRBand> &&oBands);  // Used in writing
    explicit MMRRel(const CPLString &);     // Used in writing. Simple version
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
                                               const CPLString &osExtension);
    int GetColumnsNumberFromREL();
    int GetRowsNumberFromREL();
    static int IdentifySubdataSetFile(const CPLString &osFileName);
    static int IdentifyFile(const GDALOpenInfo *poOpenInfo);
    CPLString GetColor_TractamentVariable(int nIBand) const;
    CPLString GetColor_Paleta(int nIBand) const;
    CPLErr UpdateGDALColorEntryFromBand(const CPLString &m_osBandSection,
                                        GDALColorEntry &m_sConstantColorRGB);

    void UpdateLineage(CSLConstList papszOptions, GDALDataset &oSrcDS);
    bool Write(GDALDataset &oSrcDS);
    void WriteMETADADES();
    void WriteIDENTIFICATION();
    void WriteOVERVIEW_ASPECTES_TECNICS(GDALDataset &oSrcDS);
    void WriteMetadataInComments(GDALDataset &oSrcDS);
    void WriteSPATIAL_REFERENCE_SYSTEM_HORIZONTAL();
    void WriteEXTENT();
    void WriteOVERVIEW();
    bool WriteATTRIBUTE_DATA(GDALDataset &oSrcDS);
    void WriteBandSection(const MMRBand &osBand, const CPLString &osDSDataType);
    void WriteCOLOR_TEXTSection();
    void WriteVISU_LLEGENDASection();
    void WriteLINEAGE(GDALDataset &oSrcDS);
    void WriteCurrentProcess();
    void WriteINOUTSection(const CPLString &osSection, int nInOut,
                           const CPLString &osIdentifierValue,
                           const CPLString &osSentitValue,
                           const CPLString &osTypeValuesValue,
                           const CPLString &osResultValueValue);
    void ImportAndWriteLineageSection(GDALDataset &oSrcDS);
    bool ProcessProcessSection(const CPLStringList &aosMiraMonSortedMetaData,
                               const CPLString &osProcessSection);
    void EndProcessesSection();

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

    bool CreateRELFile()
    {
        if (m_osRelFileName.empty())
            return false;

        m_pRELFile = VSIFOpenL(m_osRelFileName, "wb");
        if (m_pRELFile)
            return true;

        CPLError(CE_Failure, CPLE_FileIO, "Failed to create output file: %s",
                 m_osRelFileName.c_str());

        return false;
    }

    void AddRELVersion()
    {
        if (!m_pRELFile)
            return;

        // Writing MiraMon version section
        AddSectionStart(SECTION_VERSIO);
        AddKeyValue(KEY_VersMetaDades,
                    static_cast<unsigned>(MM_VERS_METADADES));
        AddKeyValue(KEY_SubVersMetaDades,
                    static_cast<unsigned>(MM_SUBVERS_METADADES));
        AddKeyValue(KEY_Vers, static_cast<unsigned>(MM_VERS));
        AddKeyValue(KEY_SubVers, static_cast<unsigned>(MM_SUBVERS));
        AddSectionEnd();
    }

    void AddCOLOR_TEXTVersion()
    {
        if (!m_pRELFile)
            return;

        // Writing COLOR_TEXT version keys
        AddKeyValue("Simb_Vers", 5);
        AddKeyValue("Simb_SubVers", 1);
    }

    void AddVISU_LLEGENDAVersion()
    {
        if (!m_pRELFile)
            return;

        // Writing VISU_LLEGENDA version keys
        AddKeyValue("LlegSimb_Vers", 4);
        AddKeyValue("LlegSimb_SubVers", 5);
    }

    void AddSectionStart(const CPLString &osSection)
    {
        if (!m_pRELFile)
            return;

        VSIFPrintfL(m_pRELFile, "[%s]%s", osSection.c_str(), LineReturn);
    }

    void AddSectionStart(const CPLString &osSectionP1,
                         const CPLString &osSectionP2)
    {
        if (!m_pRELFile)
            return;

        VSIFPrintfL(m_pRELFile, "[%s:%s]%s", osSectionP1.c_str(),
                    osSectionP2.c_str(), LineReturn);
    }

    void AddSectionEnd()
    {
        if (!m_pRELFile)
            return;

        VSIFPrintfL(m_pRELFile, LineReturn);
    }

    void AddKey(const CPLString &osKey)
    {
        if (!m_pRELFile)
            return;

        char *pzsKey = CPLRecode(osKey, CPL_ENC_UTF8, "CP1252");
        VSIFPrintfL(m_pRELFile, "%s=%s", pzsKey ? pzsKey : osKey.c_str(),
                    LineReturn);
        CPLFree(pzsKey);
    }

    void AddKeyValue(const CPLString &osKey, const CPLString &osValue)
    {
        if (!m_pRELFile)
            return;

        char *pzsKey = CPLRecode(osKey, CPL_ENC_UTF8, "CP1252");

        CPLString osValidValue = osValue;
        size_t nPos = osValue.find(MMEmptyValue);
        if (nPos != std::string::npos)
            osValidValue.replaceAll(MMEmptyValue, "");

        if (osValidValue.empty())
        {
            VSIFPrintfL(m_pRELFile, "%s=%s", pzsKey ? pzsKey : osKey.c_str(),
                        LineReturn);
        }
        else
        {
            char *pzsValue = CPLRecode(osValidValue, CPL_ENC_UTF8, "CP1252");
            VSIFPrintfL(m_pRELFile, "%s=%s%s", pzsKey ? pzsKey : osKey.c_str(),
                        pzsValue ? pzsValue : osValidValue.c_str(), LineReturn);
            CPLFree(pzsValue);
        }
        CPLFree(pzsKey);
    }

    void AddKeyValue(const CPLString &osKey, int nValue)
    {
        if (!m_pRELFile)
            return;

        char *pzsKey = CPLRecode(osKey, CPL_ENC_UTF8, "CP1252");
        VSIFPrintfL(m_pRELFile, "%s=%d%s", pzsKey ? pzsKey : osKey.c_str(),
                    nValue, LineReturn);

        CPLFree(pzsKey);
    }

    void AddKeyValue(const CPLString &osKey, unsigned nValue)
    {
        if (!m_pRELFile)
            return;

        char *pzsKey = CPLRecode(osKey, CPL_ENC_UTF8, "CP1252");
        VSIFPrintfL(m_pRELFile, "%s=%u%s", pzsKey ? pzsKey : osKey.c_str(),
                    nValue, LineReturn);

        CPLFree(pzsKey);
    }

    void AddKeyValue(const CPLString &osKey, double dfValue)
    {
        if (!m_pRELFile)
            return;

        char *pzsKey = CPLRecode(osKey, CPL_ENC_UTF8, "CP1252");
        CPLString osValue = CPLSPrintf("%.15g", dfValue);
        VSIFPrintfL(m_pRELFile, "%s=%s%s", pzsKey ? pzsKey : osKey.c_str(),
                    osValue.c_str(), LineReturn);

        CPLFree(pzsKey);
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

    const CPLString &GetRELName() const
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

    const CPLString &GetPattern() const
    {
        return osPattern;
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
    CPLString m_osTitle = "";
    VSILFILE *m_pRELFile = nullptr;
    static CPLString m_szImprobableRELChain;

    char m_szFileIdentifier[MM_MAX_LEN_LAYER_IDENTIFIER];

    bool m_bIsValid =
        false;  // Determines if the created object is valid or not.
    bool m_bIsAMiraMonFile = false;

    // List of rawBandNames in a subdataset
    std::vector<CPLString> m_papoSDSBands{};

    int m_nBands = 0;
    std::vector<MMRBand> m_oBands{};

    CPLString osPattern = "";  // A pattern used to create all band names

    // If there is only one band and the name of the rel is the same than the
    // name of the band (ex: band.img and bandI.rel) then NomFitxer
    // is not necessary to be written in the REL
    bool m_bNeedOfNomFitxer = true;

    // If a key-value pair is the same for all bands, it must be written
    // in a single general section. Otherwise, the most common key-value
    // (or any chosen one) can be written in the general section, and
    // all bands with a different value must write it in their
    // corresponding specific section.
    bool m_bDimAlreadyWritten = false;
    CPLString m_osDefTractVariable = "";
    CPLString m_osDefUnits = "";

    // Preserving metadata

    // List of excluded pairs {Section, Key} to be added to metadata
    // Empty Key means all section
    std::set<ExcludedEntry> m_ExcludedSectionKey = {};

    // For writing part
    // EPSG number
    CPLString m_osEPSG = "";

    // Global raster dimensions
    int m_nWidth = 0;
    int m_nHeight = 0;

    double m_dfMinX = MM_UNDEFINED_STATISTICAL_VALUE;
    double m_dfMaxX = -MM_UNDEFINED_STATISTICAL_VALUE;
    double m_dfMinY = MM_UNDEFINED_STATISTICAL_VALUE;
    double m_dfMaxY = -MM_UNDEFINED_STATISTICAL_VALUE;

    // Lineage
    CPLString m_osInFile = "";
    CPLString m_osOutFile = "";
    CPLStringList m_aosOptions{};

    // Number of processes in the lineage.
    // It is incremented each time a process is added
    // to the lineage, and it is used to set the "processes"
    // key in the QUALITY:LINEAGE section.
    int m_nNProcesses = 0;
    // It's possible to have a list of process like that:
    // processes=1,5,8 (nILastProcess=8)
    // So, current process would be 9 after the nILastProcess.
    int nILastProcess = 0;

    // List of processes
    CPLString m_osListOfProcesses = "";
};

#endif /* ndef MMR_REL_H_INCLUDED */

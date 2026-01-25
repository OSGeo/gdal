/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Driver for ASTM E57 3D file format (image part)
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "gdal_proxy.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "cpl_vsi_virtual.h"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <limits>
#include <set>

// Useful links:
// - https://paulbourke.net/dataformats/e57/
// - https://paulbourke.net/dataformats/e57/2011-huber-e57-v3.pdf
// - http://www.libe57.org/data.html
// - https://github.com/asmaloney/libE57Format
// - https://store.astm.org/e2807-11r19e01.html

namespace
{

constexpr const char *E57_PREFIX = "E57:";

/* EndOfPage size */
constexpr int E57_EOP_SIZE = 4;

/************************************************************************/
/*                            E57ImageDesc()                            */
/************************************************************************/

struct E57ImageDesc
{
    std::string osDriverName{};
    int nWidth = 0;
    int nHeight = 0;
    uint64_t nOffset = 0;
    uint64_t nLength = 0;
    uint64_t nMaskOffset = 0;
    uint64_t nMaskLength = 0;
    CPLStringList aosExtraMD{};
};

/************************************************************************/
/*             IsValidPhysicalOffsetForBeginningOfSection()             */
/************************************************************************/

static bool IsValidPhysicalOffsetForBeginningOfSection(uint64_t nOffset,
                                                       uint64_t pageSize)
{
    // The start of a section cannot be one of the last 3 bytes of a page
    return (nOffset % pageSize) < pageSize - (E57_EOP_SIZE - 1);
}

/************************************************************************/
/*                 ConvertE57LogicalOffsetToPhysical()                  */
/************************************************************************/

// Convert nLogicalOffset (measured from nBasePhysicalOffset) to physical offset
// E57 files are divided into physical pages. The last 4 bytes of every page
// are a CRC32 checksum. This function calculates the physical jump required
// to skip these checksums when moving through a logical stream of data.
static uint64_t ConvertE57LogicalOffsetToPhysical(uint64_t nBasePhysicalOffset,
                                                  uint64_t nLogicalOffset,
                                                  uint64_t nPhysicalPageSize)
{
    const auto nLogicalPageSize = nPhysicalPageSize - E57_EOP_SIZE;
    const auto numPagesCrossed =
        ((nBasePhysicalOffset % nPhysicalPageSize) + nLogicalOffset) /
        nLogicalPageSize;
    return nBasePhysicalOffset + nLogicalOffset +
           numPagesCrossed * E57_EOP_SIZE;
}

/************************************************************************/
/*                           GDAL_E57Dataset                            */
/************************************************************************/

class GDAL_E57RasterBand;

class GDAL_E57Dataset final : public GDALProxyDataset
{
  public:
    GDAL_E57Dataset(std::unique_ptr<GDALDataset> poUnderlyingDataset,
                    std::unique_ptr<GDALDataset> poMaskDS,
                    const E57ImageDesc &sE57ImageDesc, std::string osXML);

    GDALDriver *GetDriver() override;

    char **GetMetadataDomainList() override
    {
        char **papszList = GDALProxyDataset::GetMetadataDomainList();
        papszList = CSLAddString(papszList, "xml:E57");
        return papszList;
    }

    CSLConstList GetMetadata(const char *pszDomain) override
    {
        if (!pszDomain || pszDomain[0] == 0)
        {
            if (!m_bMDSet)
            {
                m_bMDSet = true;
                m_aosMD.Assign(
                    CSLDuplicate(GDALProxyDataset::GetMetadata(pszDomain)),
                    true);
                for (const auto &[pszKey, pszValue] :
                     cpl::IterateNameValue(m_sE57ImageDesc.aosExtraMD))
                    m_aosMD.SetNameValue(pszKey, pszValue);
            }
            return m_aosMD.List();
        }
        else if (EQUAL(pszDomain, "xml:E57"))
        {
            return const_cast<char **>(m_apszXMLE57.data());
        }

        return GDALProxyDataset::GetMetadata(pszDomain);
    }

    const char *GetMetadataItem(const char *pszName,
                                const char *pszDomain) override
    {
        return CSLFetchNameValue(GetMetadata(pszDomain), pszName);
    }

    static int Identify(GDALOpenInfo *poOpenInfo);
    static GDALDataset *Open(GDALOpenInfo *poOpenInfo);

  protected:
    GDALDataset *RefUnderlyingDataset() const override
    {
        return m_poUnderlyingDataset.get();
    }

  private:
    friend class GDAL_E57RasterBand;
    std::unique_ptr<GDALDataset> m_poUnderlyingDataset{};
    std::unique_ptr<GDALDataset> m_poMaskDS{};
    const E57ImageDesc m_sE57ImageDesc;
    CPLStringList m_aosMD{};
    const std::string m_osXML;
    std::array<const char *, 2> m_apszXMLE57{nullptr, nullptr};
    bool m_bMDSet = false;
};

GDALDriver *GDAL_E57Dataset::GetDriver()
{
    // Short-circuit proxying, so as not to get the PNG/JPEG driver
    return GDALDataset::GetDriver();
}

/************************************************************************/
/*                          GDAL_E57FileHandle                          */
/************************************************************************/

class GDAL_E57FileHandle final : public VSIVirtualHandle
{
  public:
    GDAL_E57FileHandle(VSIVirtualHandleUniquePtr poRawFP,
                       uint64_t nBasePhysicalOffset, uint64_t nLength,
                       uint64_t nPageSize, int nSectionHeaderSize)
        : m_poRawFP(std::move(poRawFP)),
          m_nBasePhysicalOffset(nBasePhysicalOffset), m_nLength(nLength),
          m_nPageSize(nPageSize), m_nSectionHeaderSize(nSectionHeaderSize)
    {
    }

    VSIVirtualHandleUniquePtr ReacquireRawFP()
    {
        VSIVirtualHandleUniquePtr ret;
        std::swap(ret, m_poRawFP);
        return ret;
    }

    int Seek(vsi_l_offset nOffset, int nWhence) override
    {
        m_bEOF = false;
        if (nWhence == SEEK_SET)
            m_nPos = nOffset;
        else if (nWhence == SEEK_CUR)
            m_nPos += nOffset;
        else
        {
            CPLAssert(nWhence == SEEK_END);
            CPLAssert(nOffset == 0);
            m_nPos = m_nLength;
        }
        return 0;
    }

    vsi_l_offset Tell() override;

    size_t Read(void *pBuffer, size_t nToReadTotal) override
    {
        if (m_bEOF || m_nPos > m_nLength ||
            m_nBasePhysicalOffset >
                std::numeric_limits<uint64_t>::max() - m_nPos ||
            m_nPos >
                std::numeric_limits<uint64_t>::max() - m_nSectionHeaderSize)
        {
            m_bEOF = true;
            return 0;
        }
        if (nToReadTotal == 0)
            return 0;

        // Align our raw file pointer to the physical location of the current
        // logical position, taking the E57 page/CRC overhead into account.
        const auto nPhysicalOffset = ConvertE57LogicalOffsetToPhysical(
            m_nBasePhysicalOffset, m_nPos + m_nSectionHeaderSize, m_nPageSize);

        if (m_poRawFP->Seek(nPhysicalOffset, SEEK_SET) != 0)
            return 0;

        GByte *pabyBuffer = static_cast<GByte *>(pBuffer);
        // Ingest number of requested bytes, by skipping the last 4 bytes of
        // each physical page.
        size_t nReadTotal = 0;
        while (nReadTotal < nToReadTotal)
        {
            const uint64_t nCurPos = m_poRawFP->Tell();
            const uint64_t nEndOfPagePhysicalOffset =
                cpl::div_round_up(nCurPos + 1, m_nPageSize) * m_nPageSize;
            CPLAssert(nEndOfPagePhysicalOffset - nCurPos >= E57_EOP_SIZE);

            const size_t nToReadChunk = static_cast<size_t>(
                std::min(static_cast<uint64_t>(nToReadTotal - nReadTotal),
                         nEndOfPagePhysicalOffset - nCurPos - E57_EOP_SIZE));
            const size_t nReadChunk =
                m_poRawFP->Read(pabyBuffer + nReadTotal, nToReadChunk);
            m_nPos += nReadChunk;
            nReadTotal += nReadChunk;
            if (m_nPos > m_nLength)
            {
                nReadTotal -= static_cast<size_t>(m_nPos - m_nLength);
                m_nPos = m_nLength;
                m_bEOF = true;
                break;
            }
            if (nReadChunk != nToReadChunk)
            {
                m_bEOF = true;
                break;
            }
            if (nReadTotal < nToReadTotal)
            {
                // Skip 4 bytes of CRC
                GByte abyCRC32[E57_EOP_SIZE];
                if (m_poRawFP->Read(abyCRC32, sizeof(abyCRC32)) !=
                    sizeof(abyCRC32))
                {
                    CPLDebug("E57", "Cannot read CRC");
                    break;
                }
            }
        }
        return nReadTotal;
    }

    int Eof() override
    {
        return m_bEOF;
    }

    int Error() override
    {
        return m_poRawFP->Error();
    }

    int Close() override
    {
        int nRet = 0;
        if (m_poRawFP)
            nRet = m_poRawFP->Close();
        m_poRawFP.reset();
        return nRet;
    }

    void ClearErr() override
    {
        m_poRawFP->ClearErr();
    }

    size_t Write(const void *, size_t) override
    {
        return 0;
    }

  private:
    VSIVirtualHandleUniquePtr m_poRawFP{};
    // physical offset of the start of the subfile
    const uint64_t m_nBasePhysicalOffset;
    const uint64_t m_nLength;  // logical length of the subfile
    const uint64_t m_nPageSize;
    const int m_nSectionHeaderSize;
    uint64_t m_nPos = 0;  // logical offset within the subfile
    bool m_bEOF = false;
};

// Offline definition to please -Wweak-vtables
vsi_l_offset GDAL_E57FileHandle::Tell()
{
    return m_nPos;
}

/************************************************************************/
/*                          GDAL_E57RasterBand                          */
/************************************************************************/

class GDAL_E57RasterBand final : public GDALProxyRasterBand
{
  public:
    explicit GDAL_E57RasterBand(GDALRasterBand *poUnderlyingBand)
        : m_poUnderlyingBand(poUnderlyingBand)
    {
        nRasterXSize = m_poUnderlyingBand->GetXSize();
        nRasterYSize = m_poUnderlyingBand->GetYSize();
        eDataType = m_poUnderlyingBand->GetRasterDataType();
        m_poUnderlyingBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
    }

    int GetMaskFlags() override
    {
        auto poGDS = cpl::down_cast<GDAL_E57Dataset *>(poDS);
        if (poGDS->m_poMaskDS)
            return GMF_PER_DATASET;
        return GDALProxyRasterBand::GetMaskFlags();
    }

    GDALRasterBand *GetMaskBand() override
    {
        auto poGDS = cpl::down_cast<GDAL_E57Dataset *>(poDS);
        if (poGDS->m_poMaskDS)
            return poGDS->m_poMaskDS->GetRasterBand(1);
        return GDALProxyRasterBand::GetMaskBand();
    }

  protected:
    GDALRasterBand *RefUnderlyingRasterBand(bool bForceOpen) const override;

  private:
    GDALRasterBand *const m_poUnderlyingBand;
    CPL_DISALLOW_COPY_ASSIGN(GDAL_E57RasterBand)
};

GDALRasterBand *GDAL_E57RasterBand::RefUnderlyingRasterBand(bool) const
{
    return m_poUnderlyingBand;
}

GDAL_E57Dataset::GDAL_E57Dataset(
    std::unique_ptr<GDALDataset> poUnderlyingDataset,
    std::unique_ptr<GDALDataset> poMaskDS, const E57ImageDesc &sE57ImageDesc,
    std::string osXML)
    : m_poUnderlyingDataset(std::move(poUnderlyingDataset)),
      m_poMaskDS(std::move(poMaskDS)), m_sE57ImageDesc(sE57ImageDesc),
      m_osXML(std::move(osXML))
{
    nRasterXSize = m_poUnderlyingDataset->GetRasterXSize();
    nRasterYSize = m_poUnderlyingDataset->GetRasterYSize();
    for (int i = 0; i < m_poUnderlyingDataset->GetRasterCount(); ++i)
    {
        SetBand(i + 1, std::make_unique<GDAL_E57RasterBand>(
                           m_poUnderlyingDataset->GetRasterBand(i + 1)));
    }
    m_apszXMLE57[0] = m_osXML.c_str();
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

/* static */
int GDAL_E57Dataset::Identify(GDALOpenInfo *poOpenInfo)
{
    return (poOpenInfo->nHeaderBytes >= 1024 &&
            memcmp(poOpenInfo->pabyHeader, "ASTM-E57", 8) == 0 &&
            poOpenInfo->fpL != nullptr) ||
           STARTS_WITH_CI(poOpenInfo->pszFilename, E57_PREFIX);
}

/************************************************************************/
/*                           asMDDescriptors                            */
/************************************************************************/

static const struct
{
    const char *pszXMLPath;
    const char *pszMDItem;
} asMDDescriptors[] = {
    {"name", "NAME"},
    {"description", "DESCRIPTION"},
    {"sensorVendor", "SENSOR_VENDOR"},
    {"sensorModel", "SENSOR_MODEL"},
    {"sensorSerialNumber", "SENSOR_SERIAL_NUMBER"},
    {"associatedData3DGuid", "ASSOCIATED_DATA_3D_GUID"},
    {"acquisitionDateTime.dateTimeValue", "ACQUISITION_DATE_TIME"},
    {"pose.rotation.w", "POSE_ROTATION_W"},
    {"pose.rotation.x", "POSE_ROTATION_X"},
    {"pose.rotation.y", "POSE_ROTATION_Y"},
    {"pose.rotation.z", "POSE_ROTATION_Z"},
    {"pose.translation.x", "POSE_TRANSLATION_X"},
    {"pose.translation.y", "POSE_TRANSLATION_Y"},
    {"pose.translation.z", "POSE_TRANSLATION_Z"},
    {"{rep}.pixelWidth", "PIXEL_WIDTH"},
    {"{rep}.pixelHeight", "PIXEL_HEIGHT"},
    {"{rep}.focalLength", "FOCAL_LENGTH"},
    {"{rep}.principalPointX", "PRINCIPAL_POINT_X"},
    {"{rep}.principalPointY", "PRINCIPAL_POINT_Y"},
    {"{rep}.radius", "RADIUS"},
};

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

/* static */
GDALDataset *GDAL_E57Dataset::Open(GDALOpenInfo *poOpenInfo)
{
    if (!Identify(poOpenInfo))
        return nullptr;
    if (poOpenInfo->eAccess == GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "E57 driver does not support updates");
        return nullptr;
    }

    std::string osSubDSName;
    std::unique_ptr<GDALOpenInfo> poOpenInfoSubDS;  // keep in that scope
    std::string osPhysicalFilename(poOpenInfo->pszFilename);
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, E57_PREFIX))
    {
        const CPLStringList aosTokens(
            CSLTokenizeString2(poOpenInfo->pszFilename + strlen(E57_PREFIX),
                               ":", CSLT_HONOURSTRINGS));
        if (aosTokens.size() != 2)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid E57 subdataset syntax");
            return nullptr;
        }

        osPhysicalFilename = aosTokens[0];
        poOpenInfoSubDS = std::make_unique<GDALOpenInfo>(
            osPhysicalFilename.c_str(), GA_ReadOnly);
        osSubDSName = aosTokens[1];
        poOpenInfo = poOpenInfoSubDS.get();
        // cppcheck-suppress knownConditionTrueFalse
        if (!Identify(poOpenInfo) || !poOpenInfo->fpL)
            return nullptr;
    }

    VSIVirtualHandleUniquePtr fp(poOpenInfo->fpL);
    poOpenInfo->fpL = nullptr;

    // Parse E57 header
    const uint32_t nMajorVersion = CPL_LSBUINT32PTR(poOpenInfo->pabyHeader + 8);
    const uint32_t nMinorVersion =
        CPL_LSBUINT32PTR(poOpenInfo->pabyHeader + 12);
    CPLDebug("E57", "E57 v%d.%d file", nMajorVersion, nMinorVersion);

    uint64_t filePhysicalLength;
    memcpy(&filePhysicalLength, poOpenInfo->pabyHeader + 16,
           sizeof(filePhysicalLength));
    CPL_LSBPTR64(&filePhysicalLength);
    CPLDebugOnly("E57", "filePhysicalLength = %" PRIu64, filePhysicalLength);

    uint64_t xmlPhysicalOffset;
    memcpy(&xmlPhysicalOffset, poOpenInfo->pabyHeader + 24,
           sizeof(xmlPhysicalOffset));
    CPL_LSBPTR64(&xmlPhysicalOffset);
    CPLDebugOnly("E57", "xmlPhysicalOffset = %" PRIu64, xmlPhysicalOffset);

    uint64_t xmlLogicalLength;
    memcpy(&xmlLogicalLength, poOpenInfo->pabyHeader + 32,
           sizeof(xmlLogicalLength));
    CPL_LSBPTR64(&xmlLogicalLength);
    CPLDebugOnly("E57", "xmlLogicalLength = %" PRIu64, xmlLogicalLength);

    uint64_t pageSize;
    memcpy(&pageSize, poOpenInfo->pabyHeader + 40, sizeof(pageSize));
    CPL_LSBPTR64(&pageSize);
    CPLDebugOnly("E57", "pageSize = %" PRIu64, pageSize);
    // The page size NEEDS to be strictly greater than E57_EOP_SIZE (4), and
    // the nominal page size is 1024 bytes
    constexpr uint64_t NOMINAL_PAGE_SIZE = 1024;
    constexpr uint64_t MAX_LARGE_PAGE_SIZE = 1024 * 1024;  // arbitrary
    if (pageSize < NOMINAL_PAGE_SIZE || pageSize > MAX_LARGE_PAGE_SIZE ||
        (pageSize % 4) != 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "E57: invalid page size: %" PRIu64, pageSize);
        return nullptr;
    }

    if (!IsValidPhysicalOffsetForBeginningOfSection(xmlPhysicalOffset,
                                                    pageSize))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "E57: invalid xmlPhysicalOffset: %" PRIu64, xmlPhysicalOffset);
        return nullptr;
    }

    constexpr size_t SIZEOF_E57_FILE_HEADER = 48;
    if (xmlLogicalLength > filePhysicalLength ||
        xmlLogicalLength > std::numeric_limits<size_t>::max() - 1 ||
        xmlPhysicalOffset < SIZEOF_E57_FILE_HEADER ||
        xmlPhysicalOffset > filePhysicalLength - xmlLogicalLength)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "E57: invalid "
                 "filePhysicalLength/xmlPhysicalOffset/xmlLogicalLength");
        return nullptr;
    }

    // Arbitrary threshold above which we check the consistency of the declared
    // size of the XML section w.r.t. whole file size
    constexpr size_t XML_THRESHOLD_SIZE = 100 * 1024 * 1024;
    if (xmlLogicalLength > XML_THRESHOLD_SIZE &&
        !(fp->Seek(0, SEEK_END) == 0 && fp->Tell() == filePhysicalLength &&
          fp->Seek(0, SEEK_SET) == 0))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "E57: file too short");
        return nullptr;
    }

    std::string osXML;
    try
    {
        osXML.resize(static_cast<size_t>(xmlLogicalLength));
    }
    catch (const std::exception &)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "E57: out of memory");
        return nullptr;
    }

    auto poE57XmlFile = std::make_unique<GDAL_E57FileHandle>(
        std::move(fp), xmlPhysicalOffset, xmlLogicalLength, pageSize, 0);

#ifdef DEBUG
    {
        // Quick actions just to increase test coverage
        CPLAssert(poE57XmlFile->Tell() == 0);
        char chDummy = 0;
        CPLAssert(poE57XmlFile->Read(&chDummy, 0) == 0);
        CPL_IGNORE_RET_VAL(chDummy);
        CPLAssert(!poE57XmlFile->Eof());
        CPLAssert(!poE57XmlFile->Error());
        poE57XmlFile->ClearErr();
        CPLAssert(poE57XmlFile->Write("", 0) == 0);
    }
#endif

    if (poE57XmlFile->Read(osXML.data(),
                           static_cast<size_t>(xmlLogicalLength)) !=
        static_cast<size_t>(xmlLogicalLength))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "E57: cannot read XML");
        return nullptr;
    }

#ifdef DEBUG
    if (EQUAL(CPLGetConfigOption("CPL_DEBUG", ""), "E57"))
    {
        fprintf(stderr, "XML: %s\n", osXML.c_str()); /*ok*/
    }
#endif

    CPLXMLTreeCloser poRoot(CPLParseXMLString(osXML.c_str()));
    const CPLXMLNode *psImages2D =
        CPLGetXMLNode(poRoot.get(), "=e57Root.images2D");
    std::vector<E57ImageDesc> asE57ImageDesc;
    std::set<std::string> aoSetNames;
    size_t iCounter = 0;
    // Iterate through images
    const CPLXMLNode *psIter = psImages2D;
    if (psIter)
        psIter = psIter->psChild;
    for (/* */; psIter; psIter = psIter->psNext)
    {
        if (psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, "vectorChild") == 0)
        {
            const CPLXMLNode *psRep = nullptr;
            for (const char *pszNodeName :
                 {"sphericalRepresentation", "pinholeRepresentation",
                  "cylindricalRepresentation", "visualReferenceRepresentation"})
            {
                psRep = CPLGetXMLNode(psIter, pszNodeName);
                if (psRep)
                    break;
            }
            if (psRep)
            {
                const CPLXMLNode *psImage = CPLGetXMLNode(psRep, "jpegImage");
                const bool bIsJPEG = psImage != nullptr;
                if (!psImage)
                    psImage = CPLGetXMLNode(psRep, "pngImage");
                if (psImage)
                {
                    const char *pszFileOffset =
                        CPLGetXMLValue(psImage, "fileOffset", nullptr);
                    const char *pszLength =
                        CPLGetXMLValue(psImage, "length", nullptr);
                    if (pszFileOffset && pszLength)
                    {
                        E57ImageDesc desc;
                        desc.osDriverName = bIsJPEG ? "JPEG" : "PNG";
                        desc.aosExtraMD.SetNameValue(
                            "REPRESENTATION_TYPE",
                            CPLString(psRep->pszValue)
                                .replaceAll("Representation", "")
                                .c_str());
                        for (const auto &sMDDescriptor : asMDDescriptors)
                        {
                            if (const char *pszValue = CPLGetXMLValue(
                                    psIter,
                                    CPLString(sMDDescriptor.pszXMLPath)
                                        .replaceAll("{rep}", psRep->pszValue)
                                        .c_str(),
                                    nullptr))
                                desc.aosExtraMD.SetNameValue(
                                    sMDDescriptor.pszMDItem, pszValue);
                        }
                        const char *pszName =
                            desc.aosExtraMD.FetchNameValue("NAME");
                        if (pszName)
                            aoSetNames.insert(pszName);
                        desc.nOffset =
                            std::strtoull(pszFileOffset, nullptr, 10);
                        desc.nLength = std::strtoull(pszLength, nullptr, 10);
                        desc.nWidth =
                            atoi(CPLGetXMLValue(psRep, "imageWidth", ""));
                        desc.nHeight =
                            atoi(CPLGetXMLValue(psRep, "imageHeight", ""));

                        const CPLXMLNode *psMask =
                            CPLGetXMLNode(psRep, "imageMask");
                        if (psMask)
                        {
                            const char *pszMaskFileOffset =
                                CPLGetXMLValue(psMask, "fileOffset", nullptr);
                            const char *pszMaskLength =
                                CPLGetXMLValue(psMask, "length", nullptr);
                            if (pszMaskFileOffset && pszMaskLength)
                            {
                                desc.nMaskOffset = std::strtoull(
                                    pszMaskFileOffset, nullptr, 10);
                                desc.nMaskLength =
                                    std::strtoull(pszMaskLength, nullptr, 10);
                            }
                        }

                        ++iCounter;
                        constexpr size_t MAX_IMAGES = 10000;
                        if (iCounter > MAX_IMAGES)
                        {
                            CPLError(CE_Failure, CPLE_NotSupported,
                                     "Too many images");
                            break;
                        }
                        if (osSubDSName.empty() ||
                            ((pszName && osSubDSName == pszName) ||
                             (osSubDSName == std::to_string(iCounter))))
                        {
                            asE57ImageDesc.push_back(std::move(desc));
                        }
                    }
                }
            }
        }
    }

    if (asE57ImageDesc.empty())
    {
        if (osSubDSName.empty())
        {
            CPLDebug("E57", "No image found");
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Subdataset %s not found",
                     osSubDSName.c_str());
        }
        return nullptr;
    }
    else if (asE57ImageDesc.size() == 1)
    {
        if (!IsValidPhysicalOffsetForBeginningOfSection(
                asE57ImageDesc[0].nOffset, pageSize))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "E57: invalid image offset: %" PRIu64,
                     asE57ImageDesc[0].nOffset);
            return nullptr;
        }

        CPLDebugOnly("E57", "Image physical offset: %" PRIu64,
                     asE57ImageDesc[0].nOffset);
        CPLDebugOnly("E57", "Image logical length: %" PRIu64,
                     asE57ImageDesc[0].nLength);
        constexpr int E57_SIZEOF_BINARY_SECTION_HEADER = 16;
        // Create a file handle that implements physical-to-logical translation
        // by skipping CRCs transparently.
        auto poE57File = std::make_unique<GDAL_E57FileHandle>(
            poE57XmlFile->ReacquireRawFP(), asE57ImageDesc[0].nOffset,
            asE57ImageDesc[0].nLength, pageSize,
            E57_SIZEOF_BINARY_SECTION_HEADER);

        GDALOpenInfo oOpenInfo(osPhysicalFilename.c_str(),
                               GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                               std::move(poE57File));
        const char *const apszAllowedDrivers[] = {
            asE57ImageDesc[0].osDriverName.c_str(), nullptr};
        CPLStringList aosOpenOptions;
        if (!osSubDSName.empty())
        {
            aosOpenOptions.SetNameValue("@PHYSICAL_FILENAME",
                                        osPhysicalFilename.c_str());
            aosOpenOptions.SetNameValue("@SUBDATASET_NAME",
                                        osSubDSName.c_str());
        }
        auto poImageDS =
            GDALDataset::Open(&oOpenInfo, apszAllowedDrivers, aosOpenOptions);
        std::unique_ptr<GDALDataset> poRetDS;
        if (poImageDS)
        {
            // Open the mask if present
            std::unique_ptr<GDALDataset> poMaskDS;
            if (asE57ImageDesc[0].nMaskLength &&
                asE57ImageDesc[0].nMaskOffset &&
                IsValidPhysicalOffsetForBeginningOfSection(
                    asE57ImageDesc[0].nMaskOffset, pageSize))
            {
                VSIVirtualHandleUniquePtr poNewRawFP(
                    VSIFOpenL(osPhysicalFilename.c_str(), "rb"));
                if (poNewRawFP)
                {
                    auto poE57MaskHandle = std::make_unique<GDAL_E57FileHandle>(
                        std::move(poNewRawFP), asE57ImageDesc[0].nMaskOffset,
                        asE57ImageDesc[0].nMaskLength, pageSize,
                        E57_SIZEOF_BINARY_SECTION_HEADER);
                    GDALOpenInfo oMaskOpenInfo(osPhysicalFilename.c_str(),
                                               GDAL_OF_RASTER |
                                                   GDAL_OF_INTERNAL,
                                               std::move(poE57MaskHandle));
                    const char *const apszAllowedDriversPNG[] = {"PNG",
                                                                 nullptr};
                    auto poMaskDSTmp = GDALDataset::Open(
                        &oMaskOpenInfo, apszAllowedDriversPNG, nullptr);
                    if (poMaskDSTmp &&
                        poMaskDSTmp->GetRasterXSize() ==
                            poImageDS->GetRasterXSize() &&
                        poMaskDSTmp->GetRasterYSize() ==
                            poImageDS->GetRasterYSize() &&
                        poMaskDSTmp->GetRasterCount() == 1)
                    {
                        poMaskDS = std::move(poMaskDSTmp);
                    }
                }
            }

            poRetDS = std::make_unique<GDAL_E57Dataset>(
                std::move(poImageDS), std::move(poMaskDS), asE57ImageDesc[0],
                std::move(osXML));
        }
        return poRetDS.release();
    }
    else
    {
        class GDAL_E57DatasetMultipleSDS final : public GDALDataset
        {
          public:
            GDAL_E57DatasetMultipleSDS()
            {
                nRasterXSize = 0;
                nRasterYSize = 0;
            }
        };

        const bool bUniqueNames = aoSetNames.size() == asE57ImageDesc.size();
        auto poDS = std::make_unique<GDAL_E57DatasetMultipleSDS>();
        CPLStringList aosSubDS;
        for (unsigned i = 0; i < asE57ImageDesc.size(); ++i)
        {
            const char *pszName =
                asE57ImageDesc[i].aosExtraMD.FetchNameValueDef("NAME", "");
            const CPLString osSubdatasetName = CPLString().Printf(
                "%s\"%s\":%s", E57_PREFIX, poOpenInfo->pszFilename,
                bUniqueNames ? pszName
                             : CPLString().Printf("%u", i + 1).c_str());
            aosSubDS.SetNameValue(
                CPLString().Printf("SUBDATASET_%u_NAME", i + 1),
                osSubdatasetName.c_str());
            CPLString osDesc;
            if (bUniqueNames)
            {
                osDesc = CPLString().Printf("Image %s (%dx%d)", pszName,
                                            asE57ImageDesc[i].nWidth,
                                            asE57ImageDesc[i].nHeight);
            }
            else if (pszName[0])
            {
                osDesc = CPLString().Printf("Image %u (%s) (%dx%d)", i + 1,
                                            pszName, asE57ImageDesc[i].nWidth,
                                            asE57ImageDesc[i].nHeight);
            }
            else
            {
                osDesc = CPLString().Printf("Image %u (%dx%d)", i + 1,
                                            asE57ImageDesc[i].nWidth,
                                            asE57ImageDesc[i].nHeight);
            }
            aosSubDS.SetNameValue(
                CPLString().Printf("SUBDATASET_%u_DESC", i + 1),
                osDesc.c_str());
        }
        poDS->SetMetadata(aosSubDS.List(), "SUBDATASETS");
        CPLStringList aosXMLE57;
        aosXMLE57.AddString(osXML.c_str());
        poDS->SetMetadata(aosXMLE57.List(), "xml:E57");
        return poDS.release();
    }
}

}  // namespace

/************************************************************************/
/*                          GDALRegister_E57()                          */
/************************************************************************/

void GDALRegister_E57()
{
    auto poDM = GetGDALDriverManager();
    if (poDM->GetDriverByName("E57") != nullptr)
        return;

    auto poDriver = std::make_unique<GDALDriver>();

    poDriver->SetDescription("E57");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "ASTM E57 3D file format (image part)");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "e57");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/e57.html");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = GDAL_E57Dataset::Open;
    poDriver->pfnIdentify = GDAL_E57Dataset::Identify;

    poDM->RegisterDriver(poDriver.release());
}

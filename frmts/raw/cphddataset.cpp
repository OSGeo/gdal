/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  CPHD driver multidimensional classes
 * Author:   Norman Barker <norman at analyticaspatial.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Norman Barker <norman at analyticaspatial.com>
 *
 ****************************************************************************/

#include <cmath>
#include <functional>
#include <iostream>
#include <limits>

#include "cpl_vsi_virtual.h"
#include "gdal_frmts.h"
#include "gdal_multidim.h"
#include "memmultidim.h"
#include "rawdataset.h"

static int CPHDDatasetIdentify(GDALOpenInfo *poOpenInfo);

constexpr const char *PVP_ARRAY_NAME = "PVP";

/************************************************************************/
/*                         ParseComplexDataType                         */
/************************************************************************/

static GDALDataType ParseComplexDataType(const char *pszFormat,
                                         const char *pszFileName)
{
    GDALDataType dt = GDT_Unknown;
    if EQUAL (pszFormat, "CI4")
        dt = GDT_CInt16;
    else if EQUAL (pszFormat, "CI8")
        dt = GDT_CInt32;
    else if (EQUAL(pszFormat, "CI2") || EQUAL(pszFormat, "CI16"))
        CPLError(CE_Failure, CPLE_AppDefined, "Format %s not supported : %s.",
                 pszFormat, pszFileName);
    else if EQUAL (pszFormat, "CF8")
        dt = GDT_CFloat32;
    else if EQUAL (pszFormat, "CF16")
        dt = GDT_CFloat64;
    else
        CPLError(CE_Failure, CPLE_AppDefined, "Format %s not recognized : %s.",
                 pszFormat, pszFileName);

    return dt;
}

/************************************************************************/
/*                           ParsePVPDataType                           */
/************************************************************************/

static GDALExtendedDataType ParsePVPDataType(const CPLXMLNode *psPvpXML,
                                             const char *pszFileName)
{
    std::vector<std::unique_ptr<GDALEDTComponent>> comps;
    bool bSort = false;
    size_t nSize = 0;
    int nLastOffset = 0;
    auto *psPvpChild = psPvpXML->psChild;

    std::function<bool(const CPLXMLNode *, const char *)> parseChildPVPDataType;
    parseChildPVPDataType =
        [&parseChildPVPDataType, &bSort, &comps, &nSize, &nLastOffset,
         &pszFileName](const CPLXMLNode *psNode, const char *pszPrefix)
    {
        if (psNode->eType != CXT_Element)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unexpected XML error parsing : %s", pszFileName);
            return false;
        }
        auto osElementName =
            (pszPrefix == nullptr)
                ? std::string(psNode->pszValue)
                : std::string(pszPrefix) + std::string(psNode->pszValue);
        if ((osElementName == "TxAntenna") || (osElementName == "RcvAntenna"))
        {
            auto *psChild = psNode->psChild;
            for (; psChild != nullptr; psChild = psChild->psNext)
            {
                if (!parseChildPVPDataType(
                        psChild, (std::string(psNode->pszValue) + ".").c_str()))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Expected child elements for %s : %s",
                             osElementName.c_str(), pszFileName);
                    return false;
                }
            }
            return true;
        }
        else if (osElementName == "AddedPVP")
        {
            osElementName = CPLGetXMLValue(psNode, "Name", "");
        }

        const auto pszFormat = CPLGetXMLValue(psNode, "Format", nullptr);
        const auto pszOffset = CPLGetXMLValue(psNode, "Offset", nullptr);

        if (pszFormat && pszOffset)
        {
            // all values are multiples of 8 bytes
            auto nOffset = atoi(pszOffset);
            if (nOffset < 0 || nOffset > (std::numeric_limits<int>::max() / 8))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid offset %s for %s in %s.", pszOffset,
                         osElementName.c_str(), pszFileName);
                return false;
            }
            nOffset *= 8;

            if (nOffset < nLastOffset)
                bSort = true;
            else
                nLastOffset = nOffset;

            if EQUAL (pszFormat, "X=F8;Y=F8;Z=F8;")
            {
                std::vector<std::unique_ptr<GDALEDTComponent>> xyzComps;
                xyzComps.emplace_back(
                    std::unique_ptr<GDALEDTComponent>(new GDALEDTComponent(
                        "X", 0, GDALExtendedDataType::Create(GDT_Float64))));
                xyzComps.emplace_back(
                    std::unique_ptr<GDALEDTComponent>(new GDALEDTComponent(
                        "Y", 8, GDALExtendedDataType::Create(GDT_Float64))));
                xyzComps.emplace_back(
                    std::unique_ptr<GDALEDTComponent>(new GDALEDTComponent(
                        "Z", 16, GDALExtendedDataType::Create(GDT_Float64))));
                auto dtXYZ(GDALExtendedDataType::Create("XYZ", 24,
                                                        std::move(xyzComps)));
                comps.emplace_back(std::unique_ptr<GDALEDTComponent>(
                    new GDALEDTComponent(osElementName, nOffset, dtXYZ)));
                nSize += 24;
            }
            else if EQUAL (pszFormat, "DCX=F8;DCY=F8;")
            {
                std::vector<std::unique_ptr<GDALEDTComponent>> xyComps;
                xyComps.emplace_back(
                    std::unique_ptr<GDALEDTComponent>(new GDALEDTComponent(
                        "DCX", 0, GDALExtendedDataType::Create(GDT_Float64))));
                xyComps.emplace_back(
                    std::unique_ptr<GDALEDTComponent>(new GDALEDTComponent(
                        "DCY", 8, GDALExtendedDataType::Create(GDT_Float64))));
                auto dtXY(GDALExtendedDataType::Create("DCXDCY", 16,
                                                       std::move(xyComps)));
                comps.emplace_back(std::unique_ptr<GDALEDTComponent>(
                    new GDALEDTComponent(osElementName, nOffset, dtXY)));
                nSize += 16;
            }
            else if EQUAL (pszFormat, "F8")
            {
                comps.emplace_back(
                    std::unique_ptr<GDALEDTComponent>(new GDALEDTComponent(
                        osElementName, nOffset,
                        GDALExtendedDataType::Create(GDT_Float64))));
                nSize += 8;
            }
            else if EQUAL (pszFormat, "I8")
            {
                comps.emplace_back(
                    std::unique_ptr<GDALEDTComponent>(new GDALEDTComponent(
                        osElementName, nOffset,
                        GDALExtendedDataType::Create(GDT_Int64))));
                nSize += 8;
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Unrecognized format %s for %s : %s.", pszFormat,
                         osElementName.c_str(), pszFileName);
                return false;
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Expected offset or format for %s : %s",
                     osElementName.c_str(), pszFileName);
            return false;
        }
        return true;
    };

    for (; psPvpChild != nullptr; psPvpChild = psPvpChild->psNext)
    {
        if (!parseChildPVPDataType(psPvpChild, nullptr))
            return GDALExtendedDataType::Create(GDT_Unknown);
    }

    if (bSort)
    {
        sort(comps.begin(), comps.end(),
             [](const std::unique_ptr<GDALEDTComponent> &comp1,
                const std::unique_ptr<GDALEDTComponent> &comp2)
             { return comp1->GetOffset() < comp2->GetOffset(); });
    }

    return GDALExtendedDataType::Create("PVPDataType", nSize, std::move(comps));
}

/************************************************************************/
/*                          CPHDSharedResource                          */
/************************************************************************/

struct CPHDSharedResources
{
    VSIVirtualHandleUniquePtr m_fp;
    std::string m_osFilename;
    CPLXMLTreeCloser m_poXMLTree;

    GIntBig nXmlBlockSize = 0;
    vsi_l_offset nXmlBlockByteOffset = 0;
    GIntBig nSupportBlockSize = 0;
    vsi_l_offset nSupportBlockByteOffset = 0;
    GIntBig nPVPBlockSize = 0;
    vsi_l_offset nPVPBlockByteOffset = 0;
    vsi_l_offset nPVPArrayByteOffset = 0;
    GIntBig nSignalBlockSize = 0;
    vsi_l_offset nSignalBlockByteOffset = 0;

    CPHDSharedResources(const std::string &osFilename, VSILFILE *fp)
        : m_fp(fp), m_osFilename(osFilename), m_poXMLTree(nullptr)
    {
    }
};

/************************************************************************/
/*                         CPHDInternalDataset                          */
/************************************************************************/

class CPHDInternalDataset final : public RawDataset
{
    friend class CPHDGroup;

  protected:
    CPLErr Close(GDALProgressFunc = nullptr, void * = nullptr) override;
};

/************************************************************************/
/*                                Close                                 */
/************************************************************************/

CPLErr CPHDInternalDataset::Close(GDALProgressFunc, void *)

{
    return CE_None;
}

/************************************************************************/
/*                           CPHDInternalBand                           */
/************************************************************************/

class CPHDInternalBand final : public RawRasterBand
{
  public:
    CPHDInternalBand(CPHDInternalDataset *poDSIn, int nBandIn, VSILFILE *fpRaw,
                     vsi_l_offset nImgOffsetIn, int nPixelOffsetIn,
                     int nLineOffsetIn, GDALDataType eDataTypeIn)
        : RawRasterBand(poDSIn, nBandIn, fpRaw, nImgOffsetIn, nPixelOffsetIn,
                        nLineOffsetIn, eDataTypeIn,
                        RawRasterBand::ByteOrder::ORDER_BIG_ENDIAN,
                        RawRasterBand::OwnFP::NO)
    {
    }

    ~CPHDInternalBand() override;
};

/************************************************************************/
/*                          ~CPHDInternalBand                           */
/************************************************************************/

CPHDInternalBand::~CPHDInternalBand()

{
}

/************************************************************************/
/*                             CPHDMDArray                              */
/************************************************************************/

class CPHDMDArray final : public GDALMDArray
{
    CPL_DISALLOW_COPY_ASSIGN(CPHDMDArray)

    friend class CPHDGroup;

    std::unique_ptr<CPHDInternalDataset> m_poDS;
    GDALExtendedDataType m_dt;
    std::vector<std::shared_ptr<GDALDimension>> m_dims{};
    mutable std::vector<std::shared_ptr<GDALAttribute>> m_apoAttributes{};
    std::string m_osFilename{};

  protected:
    CPHDMDArray(std::unique_ptr<CPHDInternalDataset> poDS,
                const std::string &osParentName, const std::string &osName,
                const GDALExtendedDataType &oEDT)
        : GDALAbstractMDArray(osParentName, osName),
          GDALMDArray(osParentName, osName), m_poDS(std::move(poDS)),
          m_dt(oEDT), m_osFilename(m_poDS->GetDescription())
    {
        const int nXSize = m_poDS->GetRasterXSize();
        const int nYSize = m_poDS->GetRasterYSize();

        std::string osArrayPath = osParentName.empty()
                                      ? "/" + osName
                                      : "/" + osParentName + "/" + osName;

        m_dims = {std::make_shared<GDALDimensionWeakIndexingVar>(
                      osArrayPath, "Y", "", "", nYSize),
                  std::make_shared<GDALDimensionWeakIndexingVar>(
                      osArrayPath, "X", "", "", nXSize)};
    }

    bool IRead(const GUInt64 *arrayStartIdx, const size_t *count,
               const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
               const GDALExtendedDataType &bufferDataType,
               void *pDstBuffer) const override
    {
        const size_t iDimX = 1;
        const size_t iDimY = 0;
        return GDALMDRasterIOFromBand(m_poDS->GetRasterBand(1), GF_Read, iDimX,
                                      iDimY, arrayStartIdx, count, arrayStep,
                                      bufferStride, bufferDataType, pDstBuffer);
    }

  public:
    static std::shared_ptr<CPHDMDArray>
    Create(std::unique_ptr<CPHDInternalDataset> poDS,
           const std::string &osParentName, const std::string &osName,
           const GDALExtendedDataType &oEDT)
    {
        auto array(std::shared_ptr<CPHDMDArray>(
            new CPHDMDArray(std::move(poDS), osParentName, osName, oEDT)));
        array->SetSelf(array);
        return array;
    }

    bool IsWritable() const override
    {
        return false;
    }

    const std::string &GetFilename() const override
    {
        return m_osFilename;
    }

    const std::vector<std::shared_ptr<GDALDimension>> &
    GetDimensions() const override
    {
        return m_dims;
    }

    const GDALExtendedDataType &GetDataType() const override
    {
        return m_dt;
    }

    std::vector<GUInt64> GetBlockSize() const override
    {
        int nBlockXSize = 0;
        int nBlockYSize = 0;
        m_poDS->GetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);
        return std::vector<GUInt64>{static_cast<GUInt64>(nBlockYSize),
                                    static_cast<GUInt64>(nBlockXSize)};
    }

    std::vector<std::shared_ptr<GDALAttribute>>
    GetAttributes(CSLConstList) const override
    {
        return m_apoAttributes;
    }

    ~CPHDMDArray() override;
};

/************************************************************************/
/*                             ~CPHDMDArray                             */
/************************************************************************/

CPHDMDArray::~CPHDMDArray()

{
}

/************************************************************************/
/*                              CPHDGroup                               */
/************************************************************************/

class CPHDGroup final : public GDALGroup
{
    friend class CPHDDataset;
    std::shared_ptr<CPHDSharedResources> m_poShared{};
    mutable std::vector<std::shared_ptr<GDALMDArray>> m_apoArrays{};
    mutable std::vector<std::shared_ptr<CPHDGroup>> m_apoGroups{};
    mutable std::vector<std::shared_ptr<GDALAttribute>> m_apoAttributes{};
    mutable std::vector<GByte> m_abyPVPData{};
    bool Init();
    std::shared_ptr<CPHDGroup> AddChannel(const CPLXMLNode *psChannel);
    bool AddSupportArray(const CPLXMLNode *psSupportArray);

  public:
    CPHDGroup(const std::string &osParentName, const std::string &osName,
              const std::shared_ptr<CPHDSharedResources> &poShared)
        : GDALGroup(osParentName, osName), m_poShared(poShared)
    {
    }

    std::vector<std::string>
    GetMDArrayNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALMDArray>
    OpenMDArray(const std::string &osName,
                CSLConstList papszOptions) const override;

    std::vector<std::string>
    GetGroupNames(CPL_UNUSED CSLConstList papszOptions) const override;

    std::shared_ptr<GDALGroup>
    OpenGroup(const std::string &osName,
              CSLConstList papszOptions = nullptr) const override;

    std::vector<std::shared_ptr<GDALAttribute>>
    GetAttributes(CSLConstList) const override
    {
        return m_apoAttributes;
    }
};

/************************************************************************/
/* ==================================================================== */
/*                        CPHDDataset                                   */
/* ==================================================================== */
/************************************************************************/

class CPHDDataset final : public RawDataset
{
    CPL_DISALLOW_COPY_ASSIGN(CPHDDataset)
    std::shared_ptr<GDALGroup> m_poRootGroup{};

  protected:
    CPLErr Close(GDALProgressFunc, void *) override;

  public:
    CPHDDataset()
    {
    }

    static GDALDataset *OpenMultiDim(GDALOpenInfo *poOpenInfo);

    std::shared_ptr<GDALGroup> GetRootGroup() const override
    {
        return m_poRootGroup;
    }

    static GDALDataset *Open(GDALOpenInfo *);
};

/************************************************************************/
/*                                Close                                 */
/************************************************************************/

CPLErr CPHDDataset::Close(GDALProgressFunc, void *)

{
    return CE_None;
}

/************************************************************************/
/*                            OpenMultiDim()                            */
/************************************************************************/

GDALDataset *CPHDDataset::OpenMultiDim(GDALOpenInfo *poOpenInfo)

{
    auto poDS = std::make_unique<CPHDDataset>();
    auto poShared = std::make_shared<CPHDSharedResources>(
        poOpenInfo->pszFilename, poOpenInfo->fpL);
    poOpenInfo->fpL = nullptr;
    auto poRootGroup =
        std::make_shared<CPHDGroup>(std::string(), "/", poShared);

    poShared->m_fp->Seek(0, SEEK_SET);
    const char *pszLine = nullptr;

    while ((pszLine = CPLReadLineL(poShared->m_fp.get())) != nullptr)
    {
        // header section terminator (required)
        if EQUAL (pszLine, "\f")
            break;

        const CPLStringList aosTokens(CSLTokenizeString2(pszLine, " := /", 0));

        if (aosTokens.size() == 2)
        {
            if EQUAL (aosTokens[0], "CPHD")
                poRootGroup->m_apoAttributes.emplace_back(
                    std::make_shared<GDALAttributeString>("/", "cphd_version",
                                                          aosTokens[1]));
            else if EQUAL (aosTokens[0], "RELEASE_INFO")
                poRootGroup->m_apoAttributes.emplace_back(
                    std::make_shared<GDALAttributeString>("/", "release_info",
                                                          aosTokens[1]));
            else if EQUAL (aosTokens[0], "CLASSIFICATION")
                poRootGroup->m_apoAttributes.emplace_back(
                    std::make_shared<GDALAttributeString>("/", "classification",
                                                          aosTokens[1]));
            else if EQUAL (aosTokens[0], "XML_BLOCK_SIZE")
                poShared->nXmlBlockSize = CPLAtoGIntBig(aosTokens[1]);
            else if EQUAL (aosTokens[0], "XML_BLOCK_BYTE_OFFSET")
                poShared->nXmlBlockByteOffset =
                    static_cast<GUIntBig>(CPLAtoGIntBig(aosTokens[1]));
            else if EQUAL (aosTokens[0], "SUPPORT_BLOCK_SIZE")
                poShared->nSupportBlockSize = CPLAtoGIntBig(aosTokens[1]);
            else if EQUAL (aosTokens[0], "SUPPORT_BLOCK_BYTE_OFFSET")
                poShared->nSupportBlockByteOffset =
                    static_cast<GUIntBig>(CPLAtoGIntBig(aosTokens[1]));
            else if EQUAL (aosTokens[0], "PVP_BLOCK_SIZE")
                poShared->nPVPBlockSize = CPLAtoGIntBig(aosTokens[1]);
            else if EQUAL (aosTokens[0], "PVP_BLOCK_BYTE_OFFSET")
                poShared->nPVPBlockByteOffset =
                    static_cast<GUIntBig>(CPLAtoGIntBig(aosTokens[1]));
            else if EQUAL (aosTokens[0], "SIGNAL_BLOCK_SIZE")
                poShared->nSignalBlockSize = CPLAtoGIntBig(aosTokens[1]);
            else if EQUAL (aosTokens[0], "SIGNAL_BLOCK_BYTE_OFFSET")
                poShared->nSignalBlockByteOffset =
                    static_cast<GUIntBig>(CPLAtoGIntBig(aosTokens[1]));
            else
                poRootGroup->m_apoAttributes.emplace_back(
                    std::make_shared<GDALAttributeString>(
                        "/", CPLString(aosTokens[0]).tolower(), aosTokens[1]));
        }
    }

    // read XML block
    if (poShared->nXmlBlockByteOffset && poShared->nXmlBlockSize)
    {
        std::map<CPLString, CPLString> oAttrs{
            {"collector_name", "CollectionId.CollectorName"},
            {"core_name", "CollectionId.CoreName"},
            {"collect_type", "CollectionId.CollectType"},
            {"radar_mode", "CollectionId.RadarMode.ModeType"}};

        poShared->m_fp->Seek(poShared->nXmlBlockByteOffset, SEEK_SET);
        CPLString osBuffer;
        if (poShared->nXmlBlockSize > std::numeric_limits<int>::max())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "XML block size is too large for file %s",
                     poOpenInfo->pszFilename);
            return nullptr;
        }
        try
        {
            if (poShared->nXmlBlockSize > 100 * 1024 * 1024)
            {
                VSIStatBufL sStat;
                if (VSIStatL(poOpenInfo->pszFilename, &sStat) == 0)
                {
                    GIntBig nFileSize = static_cast<GIntBig>(sStat.st_size);
                    if (poShared->nXmlBlockSize > nFileSize)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "XML block size is too large for file %s",
                                 poOpenInfo->pszFilename);
                        return nullptr;
                    }
                }
            }

            osBuffer.resize(static_cast<size_t>(poShared->nXmlBlockSize));
        }
        catch (const std::exception &e)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
            return nullptr;
        }

        poShared->m_fp->Read(&osBuffer[0],
                             static_cast<size_t>(poShared->nXmlBlockSize), 1);

        poShared->m_poXMLTree.reset(CPLParseXMLString(osBuffer));

        for (auto const &[osName, osPath] : oAttrs)
        {
            auto pszValue =
                CPLGetXMLValue(poShared->m_poXMLTree.get(), osPath, nullptr);
            if (pszValue != nullptr)
                poRootGroup->m_apoAttributes.emplace_back(
                    std::make_shared<GDALAttributeString>("/", osName,
                                                          pszValue));
        }

        // make the entire metadata xml tree available at the root level
        if (CPLTestBool(CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                                             "INCLUDE_XML", "YES")))
        {
            poRootGroup->m_apoAttributes.emplace_back(
                std::make_shared<GDALAttributeString>("/", "xml", osBuffer));
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "XML Offset and/or Size not found in CPHD header: %s.",
                 poOpenInfo->pszFilename);
        return nullptr;
    }

    if (!poShared->m_poXMLTree)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "XML block not parsed: %s.",
                 poOpenInfo->pszFilename);
        return nullptr;
    }

    if (poRootGroup->Init())
    {
        poDS->m_poRootGroup = std::move(poRootGroup);
        poDS->SetDescription(poOpenInfo->pszFilename);

        // Setup/check for pam .aux.xml.
        poDS->TryLoadXML();

        return poDS.release();
    }
    else
        return nullptr;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *CPHDDataset::Open(GDALOpenInfo *poOpenInfo)

{
    const auto eIdentify = CPHDDatasetIdentify(poOpenInfo);
    if (eIdentify == GDAL_IDENTIFY_FALSE)
        return nullptr;

    return CPHDDataset::OpenMultiDim(poOpenInfo);
}

/************************************************************************/
/* ==================================================================== */
/*                            CPHDGroup                                 */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             AddChannel()                             */
/************************************************************************/
std::shared_ptr<CPHDGroup> CPHDGroup::AddChannel(const CPLXMLNode *psChannel)
{
    // assign datasource and arrays for this group as there are not many channels
    const auto pszIdentifier = CPLGetXMLValue(psChannel, "Identifier", "");

    // these are required by the XML schema
    const auto pszSignalBlockFormat = CPLGetXMLValue(
        m_poShared->m_poXMLTree.get(), "Data.SignalArrayFormat", nullptr);
    const auto pszSignalArrayByteOffset =
        CPLGetXMLValue(psChannel, "SignalArrayByteOffset", nullptr);
    const auto pszSignalArrayWidth =
        CPLGetXMLValue(psChannel, "NumSamples", nullptr);
    const auto pszSignalArrayHeight =
        CPLGetXMLValue(psChannel, "NumVectors", nullptr);
    const auto pszNumBytesPVP = CPLGetXMLValue(m_poShared->m_poXMLTree.get(),
                                               "Data.NumBytesPVP", nullptr);
    const auto pszPVPArrayByteOffset =
        CPLGetXMLValue(psChannel, "PVPArrayByteOffset", nullptr);

    if ((pszSignalBlockFormat == nullptr) ||
        (pszSignalArrayByteOffset == nullptr) ||
        (pszSignalArrayWidth == nullptr) || (pszSignalArrayHeight == nullptr))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Required signal block array offsets and "
                 "format/height/width not found in XML description : %s.",
                 m_poShared->m_osFilename.c_str());
        return nullptr;
    }

    if ((pszPVPArrayByteOffset == nullptr) || (pszNumBytesPVP == nullptr))
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Required PVP array offsets and "
            "number of bytes (NumBytesPVP) not found in XML description : %s.",
            m_poShared->m_osFilename.c_str());
        return nullptr;
    }

    const auto nXSize = atoi(pszSignalArrayWidth);
    const auto nYSize = atoi(pszSignalArrayHeight);

    if (nXSize < 0 || nYSize < 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Signal block dimensions are incorrect "
                 "Width %i Height %i : %s.",
                 nXSize, nYSize, m_poShared->m_osFilename.c_str());
        return nullptr;
    }

    const auto poSubGroup =
        std::make_shared<CPHDGroup>("/", pszIdentifier, m_poShared);

    const auto osSignalBlockName("SignalBlock");
    const GDALDataType dt = ParseComplexDataType(
        pszSignalBlockFormat, m_poShared->m_osFilename.c_str());

    if (dt == GDT_Unknown)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unrecognized complex data type %s : %s.",
                 pszSignalBlockFormat, m_poShared->m_osFilename.c_str());
        return nullptr;
    }

    // add signal block
    auto poSignalDS = std::make_unique<CPHDInternalDataset>();
    poSignalDS->nRasterXSize = nXSize;
    poSignalDS->nRasterYSize = nYSize;

    auto poBand = std::make_unique<CPHDInternalBand>(
        poSignalDS.get(), 1, m_poShared->m_fp.get(),
        m_poShared->nSignalBlockByteOffset + atoi(pszSignalArrayByteOffset),
        GDALGetDataTypeSizeBytes(dt), GDALGetDataTypeSizeBytes(dt) * nXSize,
        dt);

    poSignalDS->SetBand(1, std::move(poBand));
    auto poSignalArray = CPHDMDArray::Create(
        std::move(poSignalDS), std::string(pszIdentifier), osSignalBlockName,
        GDALExtendedDataType::Create(dt));

    poSubGroup->m_apoArrays.emplace_back(std::move(poSignalArray));

    // add PVPs
    m_poShared->nPVPArrayByteOffset = atoi(pszPVPArrayByteOffset);

    const auto oEDTPvp =
        ParsePVPDataType(CPLGetXMLNode(m_poShared->m_poXMLTree.get(), "PVP"),
                         m_poShared->m_osFilename.c_str());
    if (oEDTPvp.GetClass() == GEDTC_NUMERIC &&
        oEDTPvp.GetNumericDataType() == GDT_Unknown)
        return nullptr;

    auto poVectorDim = std::make_shared<GDALDimensionWeakIndexingVar>(
        "/" + std::string(pszIdentifier), "Vector", GDAL_DIM_TYPE_VERTICAL,
        "AZIMUTH", nYSize);

    auto poPvpArray = MEMMDArray::Create(
        std::string(pszIdentifier), PVP_ARRAY_NAME, {poVectorDim}, oEDTPvp);
    poSubGroup->m_apoArrays.emplace_back(std::move(poPvpArray));

    // add group to root
    m_apoGroups.emplace_back(std::move(poSubGroup));
    return m_apoGroups.back();
}

/************************************************************************/
/*                          AddSupportArray()                           */
/************************************************************************/
bool CPHDGroup::AddSupportArray(const CPLXMLNode *psDataSupportArray)
{
    // get support array section
    auto psSupportXml =
        CPLGetXMLNode(m_poShared->m_poXMLTree.get(), "SupportArray");

    if (psSupportXml == nullptr)
        return false;

    auto *psSupportChild = psSupportXml->psChild;
    const auto pszArrayName =
        CPLGetXMLValue(psDataSupportArray, "Identifier", "");

    // process only known support arrays for now
    for (; psSupportChild != nullptr; psSupportChild = psSupportChild->psNext)
    {
        if EQUAL (pszArrayName,
                  CPLGetXMLValue(psSupportChild, "Identifier", ""))
        {
            const auto pszElementName = psSupportChild->pszValue;
            std::unique_ptr<CPHDInternalBand> poBand;
            const auto pszSupportArrayWidth =
                CPLGetXMLValue(psDataSupportArray, "NumCols", nullptr);
            const auto pszSupportArrayHeight =
                CPLGetXMLValue(psDataSupportArray, "NumRows", nullptr);
            const auto pszArrayByteOffset =
                CPLGetXMLValue(psDataSupportArray, "ArrayByteOffset", nullptr);

            if (!pszSupportArrayWidth || !pszSupportArrayHeight ||
                !pszArrayByteOffset)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Support Array height/width/offset not found in XML "
                         "description : %s.",
                         m_poShared->m_osFilename.c_str());
                return false;
            }

            int nWidth = atoi(pszSupportArrayWidth);
            int nHeight = atoi(pszSupportArrayHeight);

            if (nWidth < 0 || nHeight < 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Support array width/height is incorrect "
                         "width %i height %i for %s",
                         nWidth, nHeight, m_poShared->m_osFilename.c_str());
                return false;
            }

            // add support array
            auto poSupportDS = std::make_unique<CPHDInternalDataset>();
            poSupportDS->nRasterXSize = nWidth;
            poSupportDS->nRasterYSize = nHeight;

            if (EQUAL(pszElementName, "AntGainPhase") ||
                EQUAL(pszElementName, "DwellTimeArray"))
                // DataType: F4;F4
                poBand = std::make_unique<CPHDInternalBand>(
                    poSupportDS.get(), 1, m_poShared->m_fp.get(),
                    m_poShared->nSupportBlockByteOffset +
                        atoi(pszArrayByteOffset),
                    GDALGetDataTypeSizeBytes(GDT_CFloat64),
                    GDALGetDataTypeSizeBytes(GDT_CFloat64) *
                        poSupportDS->nRasterXSize,
                    GDT_CFloat64);
            else if EQUAL (pszElementName, "IAZArray")
                // DataType: IAZ=F4
                poBand = std::make_unique<CPHDInternalBand>(
                    poSupportDS.get(), 1, m_poShared->m_fp.get(),
                    m_poShared->nSupportBlockByteOffset +
                        atoi(pszArrayByteOffset),
                    GDALGetDataTypeSizeBytes(GDT_Float32),
                    GDALGetDataTypeSizeBytes(GDT_Float32) *
                        poSupportDS->nRasterXSize,
                    GDT_Float32);
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Unsupported Support Array %s : %s.", pszArrayName,
                         m_poShared->m_osFilename.c_str());
                return false;
            }

            CPLAssert(poBand);

            const auto dt = poBand->GetRasterDataType();
            poSupportDS->SetBand(1, poBand.release());
            auto poSupportArray = CPHDMDArray::Create(
                std::move(poSupportDS), "", std::string(pszArrayName),
                GDALExtendedDataType::Create(dt));

            poSupportArray->m_apoAttributes.emplace_back(
                std::make_shared<GDALAttributeString>(
                    "/" + std::string(pszArrayName), "element_format",
                    CPLGetXMLValue(psSupportChild, "ElementFormat", "")));
            poSupportArray->m_apoAttributes.emplace_back(
                std::make_shared<GDALAttributeNumeric>(
                    "/" + std::string(pszArrayName), "x_0",
                    CPLAtof(CPLGetXMLValue(psSupportChild, "X0", "0."))));
            poSupportArray->m_apoAttributes.emplace_back(
                std::make_shared<GDALAttributeNumeric>(
                    "/" + std::string(pszArrayName), "y_0",
                    CPLAtof(CPLGetXMLValue(psSupportChild, "Y0", "0."))));
            poSupportArray->m_apoAttributes.emplace_back(
                std::make_shared<GDALAttributeNumeric>(
                    "/" + std::string(pszArrayName), "xss",
                    CPLAtof(CPLGetXMLValue(psSupportChild, "XSS", "0."))));
            poSupportArray->m_apoAttributes.emplace_back(
                std::make_shared<GDALAttributeNumeric>(
                    "/" + std::string(pszArrayName), "yss",
                    CPLAtof(CPLGetXMLValue(psSupportChild, "YSS", "0."))));

            m_apoArrays.emplace_back(std::move(poSupportArray));

            break;
        }
    }
    return true;
}

/************************************************************************/
/*                                Init()                                */
/************************************************************************/
bool CPHDGroup::Init()
{
    if ((m_osName != "/") || (!m_apoArrays.empty()))
        return false;

    auto psDataXml = CPLGetXMLNode(m_poShared->m_poXMLTree.get(), "Data");

    if (psDataXml == nullptr)
        return false;

    CPLXMLNode *psChild = psDataXml->psChild;

    for (; psChild != nullptr; psChild = psChild->psNext)
    {
        if EQUAL (psChild->pszValue, "Channel")
        {
            std::shared_ptr<CPHDGroup> poSubGroup = AddChannel(psChild);
            if (!poSubGroup)
                return false;
        }
        else if EQUAL (psChild->pszValue, "SupportArray")
        {
            if (!AddSupportArray(psChild))
                return false;
        }
    }
    return true;
}

/************************************************************************/
/*                          GetMDArrayNames()                           */
/************************************************************************/

std::vector<std::string> CPHDGroup::GetMDArrayNames(CSLConstList) const
{
    std::vector<std::string> aosArrayNames;

    if (!CheckValidAndErrorOutIfNot())
        return aosArrayNames;

    for (auto const &poArray : m_apoArrays)
        aosArrayNames.emplace_back(poArray->GetName());

    return aosArrayNames;
}

/************************************************************************/
/*                            OpenMDArray()                             */
/************************************************************************/

std::shared_ptr<GDALMDArray> CPHDGroup::OpenMDArray(const std::string &osName,
                                                    CSLConstList) const
{
    if (!CheckValidAndErrorOutIfNot())
        return nullptr;

    for (const auto &poArray : m_apoArrays)
    {
        if (poArray->GetName() == osName)
        {
            if (osName == PVP_ARRAY_NAME)
            {
                const auto poPVPArray =
                    std::dynamic_pointer_cast<MEMMDArray>(poArray);

                if (!poPVPArray)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Error opening PVP array : %s",
                             m_poShared->m_osFilename.c_str());
                    return nullptr;
                }

                if (poPVPArray->IsWritable())
                {
                    // read PVP array
                    if (m_poShared->nPVPArrayByteOffset <
                        std::numeric_limits<uint64_t>::max() -
                            m_poShared->nPVPBlockByteOffset)
                    {
                        m_poShared->m_fp->Seek(
                            m_poShared->nPVPBlockByteOffset +
                                m_poShared->nPVPArrayByteOffset,
                            SEEK_SET);
                    }
                    else
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Unable to read PVPs for %s, one or both of "
                                 "PVP_BLOCK_SIZE and PVP_BLOCK_BYTE_OFFSET are "
                                 "incorrect.",
                                 m_poShared->m_osFilename.c_str());
                        return nullptr;
                    }

                    if constexpr (sizeof(size_t) <
                                  sizeof(m_poShared->nPVPBlockSize))
                    {
                        if (m_poShared->nPVPBlockSize >
                            std::numeric_limits<int>::max())
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "PVP block size is too large for file %s",
                                     m_poShared->m_osFilename.c_str());
                            return nullptr;
                        }
                    }

                    try
                    {
                        if (m_poShared->nPVPBlockSize > 100 * 1024 * 1024)
                        {
                            // possible to have a very large CPHD file so check file size to continue
                            // as PVPs should be relatively small
                            VSIStatBufL sStat;
                            if (VSIStatL(m_poShared->m_osFilename.c_str(),
                                         &sStat) == 0)
                            {
                                GIntBig nFileSize =
                                    static_cast<GIntBig>(sStat.st_size);
                                if (m_poShared->nPVPBlockSize > nFileSize)
                                {
                                    CPLError(CE_Failure, CPLE_AppDefined,
                                             "PVP block size is too large for "
                                             "file %s",
                                             m_poShared->m_osFilename.c_str());
                                    return nullptr;
                                }
                            }
                            else
                            {
                                CPLError(CE_Failure, CPLE_AppDefined,
                                         "Unable to determine file size for %s",
                                         m_poShared->m_osFilename.c_str());
                                return nullptr;
                            }
                        }
                        m_abyPVPData.resize(
                            static_cast<size_t>(m_poShared->nPVPBlockSize));
                    }
                    catch (const std::exception &)
                    {
                        CPLError(CE_Failure, CPLE_OutOfMemory,
                                 "Out of memory allocating PVP buffer");
                        return nullptr;
                    }
                    if (m_poShared->m_fp->Read(
                            m_abyPVPData.data(),
                            static_cast<size_t>(m_poShared->nPVPBlockSize)) !=
                        static_cast<size_t>(m_poShared->nPVPBlockSize))
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Unable to read PVPs from %s",
                                 m_poShared->m_osFilename.c_str());
                        return nullptr;
                    }

                    size_t nVectors = static_cast<size_t>(
                        poPVPArray->GetDimensions()[0]->GetSize());

                    const auto &oEDT = poPVPArray->GetDataType();
#if CPL_IS_LSB
                    // swap all numeric types as CPHD binary data is big endian
                    auto ptr = m_abyPVPData.data();

                    auto swapPtr = [](uint8_t *p, GDALDataType dt)
                    {
                        switch (dt)
                        {
                            case GDT_Int16:
                            case GDT_UInt16:
                            case GDT_Float16:
                                CPL_SWAP16PTR(p);
                                break;
                            case GDT_Int32:
                            case GDT_UInt32:
                            case GDT_Float32:
                                CPL_SWAP32PTR(p);
                                break;
                            case GDT_Int64:
                            case GDT_UInt64:
                            case GDT_Float64:
                                CPL_SWAP64PTR(p);
                                break;
                            // complex
                            case GDT_CInt16:
                            case GDT_CFloat16:
                                CPL_SWAP16PTR(p);
                                CPL_SWAP16PTR(p + 2);
                                break;
                            case GDT_CInt32:
                            case GDT_CFloat32:
                                CPL_SWAP32PTR(p);
                                CPL_SWAP32PTR(p + 4);
                                break;
                            case GDT_CFloat64:
                                CPL_SWAP64PTR(p);
                                CPL_SWAP64PTR(p + 8);
                                break;
                            default:
                                break;
                        }

                        return GDALGetDataTypeSizeBytes(dt);
                    };

                    // loop over all vectors
                    for (size_t i = 0; i < nVectors; i++)
                    {
                        for (const auto &comp : oEDT.GetComponents())
                        {
                            if (comp->GetType().GetClass() == GEDTC_COMPOUND)
                            {
                                // swap XYZ and similar one level down types
                                for (const auto &subComp :
                                     comp->GetType().GetComponents())
                                {
                                    ptr +=
                                        swapPtr(ptr, subComp->GetType()
                                                         .GetNumericDataType());
                                }
                            }
                            else
                            {
                                ptr += swapPtr(
                                    ptr, comp->GetType().GetNumericDataType());
                            }
                        }
                    }
#endif
                    if (!poPVPArray->Init(m_abyPVPData.data()))
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Unable to read PVP data.");
                        return nullptr;
                    }

                    poPVPArray->SetWritable(false);
                }
            }
            return poArray;
        }
    }
    return nullptr;
}

/************************************************************************/
/*                           GetGroupNames()                            */
/************************************************************************/
std::vector<std::string>
CPHDGroup::GetGroupNames(CPL_UNUSED CSLConstList papszOptions) const

{
    std::vector<std::string> aosGroupNames;

    for (auto const &poGroup : m_apoGroups)
        aosGroupNames.emplace_back(poGroup->GetName());

    return aosGroupNames;
}

/************************************************************************/
/*                             OpenGroup()                              */
/************************************************************************/
std::shared_ptr<GDALGroup> CPHDGroup::OpenGroup(const std::string &osName,
                                                CSLConstList) const

{
    for (const auto &poGroup : m_apoGroups)
        if (poGroup->GetName() == osName)
            return poGroup;

    return nullptr;
}

/************************************************************************/
/*                        CPHDDatasetIdentify()                         */
/************************************************************************/

static int CPHDDatasetIdentify(GDALOpenInfo *poOpenInfo)
{
    return poOpenInfo->IsExtensionEqualToCI("cphd") && poOpenInfo->bStatOK &&
           (poOpenInfo->eAccess == GA_ReadOnly) &&
           (poOpenInfo->nOpenFlags & GDAL_OF_MULTIDIM_RASTER);
}

/************************************************************************/
/*                         GDALRegister_CPHD()                          */
/************************************************************************/

void GDALRegister_CPHD()

{
    if (GDALGetDriverByName("CPHD") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("CPHD");
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIDIM_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "Compensated Phase History Data Reader");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/cphd.html");
    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "<Option name='INCLUDE_XML' type='boolean' "
        "description='Whether to include the XML string as a group attribute' "
        "default='YES'/>"
        "</OpenOptionList>");
    poDriver->pfnOpen = CPHDDataset::Open;
    poDriver->pfnIdentify = CPHDDatasetIdentify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}

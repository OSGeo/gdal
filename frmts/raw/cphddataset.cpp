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
#include <iostream>

#include "cpl_vsi_virtual.h"
#include "gdal_frmts.h"
#include "memmultidim.h"
#include "rawdataset.h"

#include "cphddataset.h"

static int CPHDDatasetIdentify(GDALOpenInfo *poOpenInfo);

constexpr const char* PVP_ARRAY_NAME = "PVP";

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
    auto *psPvpChild = psPvpXML->psChild;
    std::string osPrefix{};
    size_t nSize = 0;

    for (; psPvpChild != nullptr; psPvpChild = psPvpChild->psNext)
    {
        if (psPvpChild->eType == CXT_Element)
        {
            auto osElementName = osPrefix + std::string(psPvpChild->pszValue);

            if (osElementName == "AddedPVP")
                osElementName = CPLGetXMLValue(psPvpChild, "Name", "");

            const auto pszFormat = CPLGetXMLValue(psPvpChild, "Format", nullptr);
            const auto pszOffset = CPLGetXMLValue(psPvpChild, "Offset", nullptr);

            if (pszFormat && pszOffset)
            {
                // all values are multiples of 8 bytes
                auto nOffset = atoi(pszOffset) * 8;

                if EQUAL (pszFormat, "X=F8;Y=F8;Z=F8;")
                {
                    std::vector<std::unique_ptr<GDALEDTComponent>> xyzComps;
                    xyzComps.emplace_back(
                        std::unique_ptr<GDALEDTComponent>(new GDALEDTComponent(
                            "X", 0,
                            GDALExtendedDataType::Create(GDT_Float64))));
                    xyzComps.emplace_back(
                        std::unique_ptr<GDALEDTComponent>(new GDALEDTComponent(
                            "Y", 8,
                            GDALExtendedDataType::Create(GDT_Float64))));
                    xyzComps.emplace_back(
                        std::unique_ptr<GDALEDTComponent>(new GDALEDTComponent(
                            "Z", 16,
                            GDALExtendedDataType::Create(GDT_Float64))));
                    auto dtXYZ(GDALExtendedDataType::Create(
                        "XYZ", 24, std::move(xyzComps)));
                    comps.emplace_back(std::unique_ptr<GDALEDTComponent>(
                        new GDALEDTComponent(osElementName, nOffset, dtXYZ)));
                    nSize += 24;
                }
                else if EQUAL (pszFormat, "DCX=F8;DCY=F8;")
                {
                    std::vector<std::unique_ptr<GDALEDTComponent>> xyComps;
                    xyComps.emplace_back(
                        std::unique_ptr<GDALEDTComponent>(new GDALEDTComponent(
                            "DCX", 0,
                            GDALExtendedDataType::Create(GDT_Float64))));
                    xyComps.emplace_back(
                        std::unique_ptr<GDALEDTComponent>(new GDALEDTComponent(
                            "DCY", 8,
                            GDALExtendedDataType::Create(GDT_Float64))));
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
                    return GDALExtendedDataType::Create(GDT_Unknown);
                }

                osPrefix = "";
            }
            else
            {
                if (osElementName == "TxAntenna")
                    osPrefix = "TxAntenna.";
                else if (osElementName == "RcvAntenna")
                    osPrefix = "RcvAntenna.";
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Format, offset not found for %s : %s.",
                             osElementName.c_str(), pszFileName);
                    return GDALExtendedDataType::Create(GDT_Unknown);
                }
            }
        }
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
    std::shared_ptr<CPLXMLNode> m_poXMLTree{};

    size_t nXmlBlockSize = 0;
    size_t nXmlBlockByteOffset = 0;
    size_t nSupportBlockSize = 0;
    size_t nSupportBlockByteOffset = 0;
    size_t nPVPBlockSize = 0;
    size_t nPVPBlockByteOffset = 0;
    size_t nPVPArrayByteOffset = 0;
    size_t nSignalBlockSize = 0;
    size_t nSignalBlockByteOffset = 0;

    CPHDSharedResources(const std::string &osFilename, VSILFILE *fp)
        : m_fp(fp), m_osFilename(osFilename)
    {
    }
};

/************************************************************************/
/*                         CPHDInternalDataset                          */
/************************************************************************/

class CPHDInternalBand;

class CPHDInternalDataset final : public RawDataset
{
    friend class CPHDGroup;

  public:
    CPHDInternalDataset()
    {
    }

    CPHDInternalDataset(int nXSize, int nYSize, const std::string &osName);
};

CPHDInternalDataset::CPHDInternalDataset(int nXSize, int nYSize,
                                         const std::string &osName)
    : RawDataset()
{
    nRasterXSize = nXSize;
    nRasterYSize = nYSize;
    eAccess = GA_ReadOnly;
    sDescription = osName.c_str();
    bShared = true;
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
};

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
    CPHDMDArray(CPHDInternalDataset *poDS, const std::string &osParentName,
                const std::string &osName, const GDALExtendedDataType &oEDT)
        : GDALAbstractMDArray(osParentName, osName),
          GDALMDArray(osParentName, osName), m_poDS(poDS), m_dt(oEDT),
          m_osFilename(poDS->GetDescription())
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
    static std::shared_ptr<CPHDMDArray> Create(CPHDInternalDataset *poDS,
                                               const std::string &osParentName,
                                               const std::string &osName,
                                               const GDALExtendedDataType &oEDT)
    {
        auto array(std::shared_ptr<CPHDMDArray>(
            new CPHDMDArray(poDS, osParentName, osName, oEDT)));
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
};

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

        char **papszTokens = CSLTokenizeString2(pszLine, " := /", 0);

        if (CSLCount(papszTokens) == 2)
        {
            if EQUAL (papszTokens[0], "CPHD")
                poRootGroup->m_apoAttributes.emplace_back(
                    std::make_shared<GDALAttributeString>("/", "cphd_version",
                                                          papszTokens[1]));
            else if EQUAL (papszTokens[0], "RELEASE_INFO")
                poRootGroup->m_apoAttributes.emplace_back(
                    std::make_shared<GDALAttributeString>("/", "release_info",
                                                          papszTokens[1]));
            else if EQUAL (papszTokens[0], "CLASSIFICATION")
                poRootGroup->m_apoAttributes.emplace_back(
                    std::make_shared<GDALAttributeString>("/", "classification",
                                                          papszTokens[1]));
            else if EQUAL (papszTokens[0], "XML_BLOCK_SIZE")
                poShared->nXmlBlockSize = CPLAtoGIntBig(papszTokens[1]);
            else if EQUAL (papszTokens[0], "XML_BLOCK_BYTE_OFFSET")
                poShared->nXmlBlockByteOffset = CPLAtoGIntBig(papszTokens[1]);
            else if EQUAL (papszTokens[0], "SUPPORT_BLOCK_SIZE")
                poShared->nSupportBlockSize = CPLAtoGIntBig(papszTokens[1]);
            else if EQUAL (papszTokens[0], "SUPPORT_BLOCK_BYTE_OFFSET")
                poShared->nSupportBlockByteOffset =
                    CPLAtoGIntBig(papszTokens[1]);
            else if EQUAL (papszTokens[0], "PVP_BLOCK_SIZE")
                poShared->nPVPBlockSize = CPLAtoGIntBig(papszTokens[1]);
            else if EQUAL (papszTokens[0], "PVP_BLOCK_BYTE_OFFSET")
                poShared->nPVPBlockByteOffset = CPLAtoGIntBig(papszTokens[1]);
            else if EQUAL (papszTokens[0], "SIGNAL_BLOCK_SIZE")
                poShared->nSignalBlockSize = CPLAtoGIntBig(papszTokens[1]);
            else if EQUAL (papszTokens[0], "SIGNAL_BLOCK_BYTE_OFFSET")
                poShared->nSignalBlockByteOffset =
                    CPLAtoGIntBig(papszTokens[1]);
        }

        CSLDestroy(papszTokens);
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
        osBuffer.resize(poShared->nXmlBlockSize);
        CPLPushErrorHandler(CPLQuietErrorHandler);
        poShared->m_fp->Read(&osBuffer[0], poShared->nXmlBlockSize, 1);

        poShared->m_poXMLTree =
            std::make_shared<CPLXMLNode>(*CPLParseXMLString(osBuffer));

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

        CPLPopErrorHandler();
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
    auto pszIdentifier = CPLGetXMLValue(psChannel, "Identifier", "");

    // these are required by the XML schema
    auto pszSignalBlockFormat = CPLGetXMLValue(
        m_poShared->m_poXMLTree.get(), "Data.SignalArrayFormat", nullptr);
    auto pszSignalArrayByteOffset =
        CPLGetXMLValue(psChannel, "SignalArrayByteOffset", nullptr);
    auto pszSignalArrayWidth = CPLGetXMLValue(psChannel, "NumSamples", nullptr);
    auto pszSignalArrayHeight =
        CPLGetXMLValue(psChannel, "NumVectors", nullptr);
    auto pszNumBytesPVP = CPLGetXMLValue(m_poShared->m_poXMLTree.get(),
                                         "Data.NumBytesPVP", nullptr);
    auto pszPVPArrayByteOffset =
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

    auto poSubGroup =
        std::make_shared<CPHDGroup>("/", pszIdentifier, m_poShared);

    auto nXSize = atoi(pszSignalArrayWidth);
    auto nYSize = atoi(pszSignalArrayHeight);
    auto osSignalBlockName = "SignalBlock";
    GDALDataType dt = ParseComplexDataType(pszSignalBlockFormat,
                                           m_poShared->m_osFilename.c_str());

    // add signal block
    auto poSignalDS =
        new CPHDInternalDataset(nXSize, nYSize, osSignalBlockName);

    auto poBand = new CPHDInternalBand(
        poSignalDS, 1, m_poShared->m_fp.get(),
        m_poShared->nSignalBlockByteOffset + atoi(pszSignalArrayByteOffset),
        GDALGetDataTypeSizeBytes(dt), GDALGetDataTypeSizeBytes(dt) * nXSize,
        dt);

    poSignalDS->SetBand(1, poBand);
    auto poSignalArray = CPHDMDArray::Create(
        poSignalDS, std::string(pszIdentifier), osSignalBlockName,
        GDALExtendedDataType::Create(dt));

    poSubGroup->m_apoArrays.emplace_back(std::move(poSignalArray));

    // add PVPs
    m_poShared->nPVPArrayByteOffset = atoi(pszPVPArrayByteOffset);

    auto oEDTPvp =
        ParsePVPDataType(CPLGetXMLNode(m_poShared->m_poXMLTree.get(), "PVP"),
                         m_poShared->m_osFilename.c_str());

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
    auto *psSupportChild = psSupportXml->psChild;
    const auto pszArrayName = CPLGetXMLValue(psDataSupportArray, "Identifier", "");

    // process only known support arrays for now
    for (; psSupportChild != nullptr; psSupportChild = psSupportChild->psNext)
    {
        if EQUAL (pszArrayName,
                  CPLGetXMLValue(psSupportChild, "Identifier", ""))
        {
            auto pszElementName = psSupportChild->pszValue;
            std::unique_ptr<CPHDInternalBand> poBand = nullptr;
            auto pszSupportArrayWidth =
                CPLGetXMLValue(psDataSupportArray, "NumCols", nullptr);
            auto pszSupportArrayHeight =
                CPLGetXMLValue(psDataSupportArray, "NumRows", nullptr);
            auto pszArrayByteOffset =
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

            // add support array
            auto poSupportDS = std::make_unique<CPHDInternalDataset>(
                atoi(pszSupportArrayWidth), atoi(pszSupportArrayHeight),
                std::string(pszArrayName));

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

            if (poBand)
            {
                auto dt = poBand->GetRasterDataType();
                poSupportDS->SetBand(1, poBand.release());
                auto poSupportArray = CPHDMDArray::Create(
                    poSupportDS.release(), "", std::string(pszArrayName),
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
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Unable to create support array %s : %s.",
                         pszArrayName, m_poShared->m_osFilename.c_str());
                return false;
            }
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
                if (poPVPArray->IsWritable())
                {
                    // read PVP array
                    m_poShared->m_fp->Seek(m_poShared->nPVPBlockByteOffset +
                                               m_poShared->nPVPArrayByteOffset,
                                           SEEK_SET);
                    m_abyPVPData.resize(m_poShared->nPVPBlockSize);
                    m_poShared->m_fp->Read(m_abyPVPData.data(),
                                           m_poShared->nPVPBlockSize);

                    size_t nVectors = static_cast<size_t>(
                        poPVPArray->GetDimensions()[0]->GetSize());

                    auto oEDT = poPVPArray->GetDataType();
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
    poDriver->pfnOpen = CPHDDataset::Open;
    poDriver->pfnIdentify = CPHDDatasetIdentify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}

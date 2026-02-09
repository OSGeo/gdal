/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  Read S104 datasets.
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2023-2025, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "hdf5dataset.h"
#include "hdf5drivercore.h"
#include "gh5_convenience.h"
#include "rat.h"
#include "s100.h"

#include "gdal_frmts.h"
#include "gdal_priv.h"
#include "gdal_proxy.h"
#include "gdal_rat.h"

#include "cpl_time.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <variant>

/************************************************************************/
/*                             S104Dataset                              */
/************************************************************************/

class S104Dataset final : public S100BaseDataset
{
  public:
    explicit S104Dataset(const std::string &osFilename)
        : S100BaseDataset(osFilename)
    {
    }

    ~S104Dataset() override;

    static GDALDataset *Open(GDALOpenInfo *);
    static GDALDataset *CreateCopy(const char *pszFilename,
                                   GDALDataset *poSrcDS, int bStrict,
                                   CSLConstList papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData);
};

S104Dataset::~S104Dataset() = default;

/************************************************************************/
/*                            S104RasterBand                            */
/************************************************************************/

class S104RasterBand final : public GDALProxyRasterBand
{
    friend class S104Dataset;
    std::unique_ptr<GDALDataset> m_poDS{};
    GDALRasterBand *m_poUnderlyingBand = nullptr;
    std::string m_osUnitType{};
    std::unique_ptr<GDALRasterAttributeTable> m_poRAT{};

    CPL_DISALLOW_COPY_ASSIGN(S104RasterBand)

  public:
    explicit S104RasterBand(std::unique_ptr<GDALDataset> &&poDSIn)
        : m_poDS(std::move(poDSIn)),
          m_poUnderlyingBand(m_poDS->GetRasterBand(1))
    {
        eDataType = m_poUnderlyingBand->GetRasterDataType();
        m_poUnderlyingBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
    }

    GDALRasterBand *
    RefUnderlyingRasterBand(bool /*bForceOpen*/ = true) const override;

    const char *GetUnitType() override
    {
        return m_osUnitType.c_str();
    }

    GDALRasterAttributeTable *GetDefaultRAT() override
    {
        return m_poRAT.get();
    }
};

GDALRasterBand *
S104RasterBand::RefUnderlyingRasterBand(bool /*bForceOpen*/) const
{
    return m_poUnderlyingBand;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *S104Dataset::Open(GDALOpenInfo *poOpenInfo)

{
    // Confirm that this appears to be a S104 file.
    if (!S104DatasetIdentify(poOpenInfo))
        return nullptr;

    HDF5_GLOBAL_LOCK();

    if (poOpenInfo->nOpenFlags & GDAL_OF_MULTIDIM_RASTER)
    {
        return HDF5Dataset::OpenMultiDim(poOpenInfo);
    }

    // Confirm the requested access is supported.
    if (poOpenInfo->eAccess == GA_Update)
    {
        ReportUpdateNotSupportedByDriver("S104");
        return nullptr;
    }

    std::string osFilename(poOpenInfo->pszFilename);
    std::string osFeatureInstance = "WaterLevel.01";
    std::string osGroup;
    if (STARTS_WITH(poOpenInfo->pszFilename, "S104:"))
    {
        const CPLStringList aosTokens(
            CSLTokenizeString2(poOpenInfo->pszFilename, ":",
                               CSLT_HONOURSTRINGS | CSLT_PRESERVEESCAPES));

        if (aosTokens.size() == 2)
        {
            osFilename = aosTokens[1];
        }
        else if (aosTokens.size() == 3)
        {
            osFilename = aosTokens[1];
            osGroup = aosTokens[2];
        }
        else if (aosTokens.size() == 4)
        {
            osFilename = aosTokens[1];
            osFeatureInstance = aosTokens[2];
            osGroup = aosTokens[3];
        }
        else
        {
            return nullptr;
        }
    }

    auto poDS = std::make_unique<S104Dataset>(osFilename);
    if (!poDS->Init())
        return nullptr;

    const auto &poRootGroup = poDS->m_poRootGroup;

    auto poVerticalCS = poRootGroup->GetAttribute("verticalCS");
    if (poVerticalCS && poVerticalCS->GetDataType().GetClass() == GEDTC_NUMERIC)
    {
        const int nVerticalCS = poVerticalCS->ReadAsInt();
        if (nVerticalCS == 6498)
            poDS->GDALDataset::SetMetadataItem(
                "VERTICAL_CS_DEFINITION", "depth, meters, orientation down");
        else if (nVerticalCS == 6499)
            poDS->GDALDataset::SetMetadataItem(
                "VERTICAL_CS_DEFINITION", "height, meters, orientation up");

        poDS->GDALDataset::SetMetadataItem("verticalCS",
                                           std::to_string(nVerticalCS).c_str());
    }

    auto poWaterLevel = poRootGroup->OpenGroup("WaterLevel");
    if (!poWaterLevel)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find /WaterLevel group");
        return nullptr;
    }

    auto poDataCodingFormat = poWaterLevel->GetAttribute("dataCodingFormat");
    if (!poDataCodingFormat ||
        poDataCodingFormat->GetDataType().GetClass() != GEDTC_NUMERIC)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find /WaterLevel/dataCodingFormat attribute");
        return nullptr;
    }
    const int nDataCodingFormat = poDataCodingFormat->ReadAsInt();
    if (nDataCodingFormat != 2)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "dataCodingFormat=%d is not supported by the S104 driver",
                 nDataCodingFormat);
        return nullptr;
    }

    // Read additional metadata
    for (const char *pszAttrName :
         {"methodWaterLevelProduct", "minDatasetHeight", "maxDatasetHeight",
          "horizontalPositionUncertainty", "verticalUncertainty",
          "timeUncertainty", "commonPointRule", "interpolationType"})
    {
        auto poAttr = poWaterLevel->GetAttribute(pszAttrName);
        if (poAttr)
        {
            const char *pszVal = poAttr->ReadAsString();
            if (pszVal)
            {
                poDS->GDALDataset::SetMetadataItem(pszAttrName, pszVal);
            }
        }
    }

    if (auto poCommonPointRule = poWaterLevel->GetAttribute("commonPointRule"))
    {
        poDS->SetMetadataForCommonPointRule(poCommonPointRule.get());
    }

    if (auto poInterpolationType =
            poWaterLevel->GetAttribute("interpolationType"))
    {
        poDS->SetMetadataForInterpolationType(poInterpolationType.get());
    }

    int nNumInstances = 1;
    if (osGroup.empty())
    {
        auto poNumInstances = poWaterLevel->GetAttribute("numInstances");
        if (poNumInstances &&
            poNumInstances->GetDataType().GetClass() == GEDTC_NUMERIC)
        {
            nNumInstances = poNumInstances->ReadAsInt();
        }
    }
    if (nNumInstances != 1)
    {
        CPLStringList aosSubDSList;
        int iSubDS = 0;
        for (const std::string &featureInstanceName :
             poWaterLevel->GetGroupNames())
        {
            auto poFeatureInstance =
                poWaterLevel->OpenGroup(featureInstanceName);
            if (poFeatureInstance)
            {
                GDALMajorObject mo;
                // Read first vertical datum from root group and let the
                // coverage override it.
                S100ReadVerticalDatum(&mo, poRootGroup.get());
                S100ReadVerticalDatum(&mo, poFeatureInstance.get());

                const auto aosGroupNames = poFeatureInstance->GetGroupNames();
                for (const auto &osSubGroup : aosGroupNames)
                {
                    if (auto poSubGroup =
                            poFeatureInstance->OpenGroup(osSubGroup))
                    {
                        ++iSubDS;
                        aosSubDSList.SetNameValue(
                            CPLSPrintf("SUBDATASET_%d_NAME", iSubDS),
                            CPLSPrintf("S104:\"%s\":%s:%s", osFilename.c_str(),
                                       featureInstanceName.c_str(),
                                       osSubGroup.c_str()));

                        std::string verticalDatum;
                        const char *pszValue =
                            mo.GetMetadataItem(S100_VERTICAL_DATUM_NAME);
                        if (pszValue)
                        {
                            verticalDatum = ", vertical datum ";
                            verticalDatum += pszValue;
                            pszValue =
                                mo.GetMetadataItem(S100_VERTICAL_DATUM_ABBREV);
                            if (pszValue)
                            {
                                verticalDatum += " (";
                                verticalDatum += pszValue;
                                verticalDatum += ')';
                            }
                        }

                        std::string osSubDSDesc;
                        const auto poTimePoint =
                            poSubGroup->GetAttribute("timePoint");
                        if (poTimePoint)
                        {
                            const char *pszVal = poTimePoint->ReadAsString();
                            if (pszVal)
                            {
                                osSubDSDesc = "Values for feature instance ";
                                osSubDSDesc += featureInstanceName;
                                osSubDSDesc += verticalDatum;
                                osSubDSDesc += " at timestamp ";
                                osSubDSDesc += pszVal;
                            }
                        }
                        if (osSubDSDesc.empty())
                        {
                            osSubDSDesc = "Values for feature instance ";
                            osSubDSDesc += featureInstanceName;
                            osSubDSDesc += verticalDatum;
                            osSubDSDesc += " and group ";
                            osSubDSDesc += osSubGroup;
                        }

                        aosSubDSList.SetNameValue(
                            CPLSPrintf("SUBDATASET_%d_DESC", iSubDS),
                            osSubDSDesc.c_str());
                    }
                }
            }
        }

        poDS->GDALDataset::SetMetadata(aosSubDSList.List(), "SUBDATASETS");

        // Setup/check for pam .aux.xml.
        poDS->SetDescription(osFilename.c_str());
        poDS->TryLoadXML();

        // Setup overviews.
        poDS->oOvManager.Initialize(poDS.get(), osFilename.c_str());

        return poDS.release();
    }

    auto poFeatureInstance = poWaterLevel->OpenGroup(osFeatureInstance);
    if (!poFeatureInstance)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find /WaterLevel/%s group", osFeatureInstance.c_str());
        return nullptr;
    }

    // Read additional metadata
    for (const char *pszAttrName :
         {"timeRecordInterval", "dateTimeOfFirstRecord", "dateTimeOfLastRecord",
          "numberOfTimes", "dataDynamicity"})
    {
        auto poAttr = poFeatureInstance->GetAttribute(pszAttrName);
        if (poAttr)
        {
            const char *pszVal = poAttr->ReadAsString();
            if (pszVal)
            {
                poDS->GDALDataset::SetMetadataItem(pszAttrName, pszVal);
            }
        }
    }

    if (auto poDataDynamicity =
            poFeatureInstance->GetAttribute("dataDynamicity"))
    {
        poDS->SetMetadataForDataDynamicity(poDataDynamicity.get());
    }

    if (auto poStartSequence = poFeatureInstance->GetAttribute("startSequence"))
    {
        const char *pszStartSequence = poStartSequence->ReadAsString();
        if (pszStartSequence && !EQUAL(pszStartSequence, "0,0"))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "startSequence (=%s) != 0,0 is not supported",
                     pszStartSequence);
            return nullptr;
        }
    }

    if (!S100GetNumPointsLongitudinalLatitudinal(
            poFeatureInstance.get(), poDS->nRasterXSize, poDS->nRasterYSize))
    {
        return nullptr;
    }

    // Potentially override vertical datum
    S100ReadVerticalDatum(poDS.get(), poFeatureInstance.get());

    const bool bNorthUp = CPLTestBool(
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "NORTH_UP", "YES"));

    // Compute geotransform
    poDS->m_bHasGT =
        S100GetGeoTransform(poFeatureInstance.get(), poDS->m_gt, bNorthUp);

    if (osGroup.empty())
    {
        const auto aosGroupNames = poFeatureInstance->GetGroupNames();
        int iSubDS = 1;
        for (const auto &osSubGroup : aosGroupNames)
        {
            if (auto poSubGroup = poFeatureInstance->OpenGroup(osSubGroup))
            {
                poDS->GDALDataset::SetMetadataItem(
                    CPLSPrintf("SUBDATASET_%d_NAME", iSubDS),
                    CPLSPrintf("S104:\"%s\":%s", osFilename.c_str(),
                               osSubGroup.c_str()),
                    "SUBDATASETS");
                std::string osSubDSDesc = "Values for group ";
                osSubDSDesc += osSubGroup;
                const auto poTimePoint = poSubGroup->GetAttribute("timePoint");
                if (poTimePoint)
                {
                    const char *pszVal = poTimePoint->ReadAsString();
                    if (pszVal)
                    {
                        osSubDSDesc = "Values at timestamp ";
                        osSubDSDesc += pszVal;
                    }
                }
                poDS->GDALDataset::SetMetadataItem(
                    CPLSPrintf("SUBDATASET_%d_DESC", iSubDS),
                    osSubDSDesc.c_str(), "SUBDATASETS");
                ++iSubDS;
            }
        }
    }
    else
    {
        auto poGroup = poFeatureInstance->OpenGroup(osGroup);
        if (!poGroup)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find /WaterLevel/%s/%s group",
                     osFeatureInstance.c_str(), osGroup.c_str());
            return nullptr;
        }

        // Read additional metadata
        for (const char *pszAttrName :
             {"timePoint", "waterLevelTrendThreshold", "trendInterval"})
        {
            auto poAttr = poGroup->GetAttribute(pszAttrName);
            if (poAttr)
            {
                const char *pszVal = poAttr->ReadAsString();
                if (pszVal)
                {
                    poDS->GDALDataset::SetMetadataItem(pszAttrName, pszVal);
                }
            }
        }

        auto poValuesArray = poGroup->OpenMDArray("values");
        if (!poValuesArray)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find /WaterLevel/%s/%s/values array",
                     osFeatureInstance.c_str(), osGroup.c_str());
            return nullptr;
        }

        if (poValuesArray->GetDimensionCount() != 2)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Wrong dimension count for %s",
                     poValuesArray->GetFullName().c_str());
            return nullptr;
        }

        const auto &oType = poValuesArray->GetDataType();
        if (oType.GetClass() != GEDTC_COMPOUND)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Wrong data type for %s",
                     poValuesArray->GetFullName().c_str());
            return nullptr;
        }

        const auto &oComponents = oType.GetComponents();
        if ((oComponents.size() != 2 && oComponents.size() != 3) ||
            oComponents[0]->GetName() != "waterLevelHeight" ||
            oComponents[0]->GetType().GetNumericDataType() != GDT_Float32 ||
            oComponents[1]->GetName() != "waterLevelTrend" ||
            (oComponents[1]->GetType().GetNumericDataType() != GDT_UInt8 &&
             // In theory should be Byte, but 104US00_ches_dcf2_20190606T12Z.h5 uses Int32
             oComponents[1]->GetType().GetNumericDataType() != GDT_Int32) ||
            (oComponents.size() == 3 &&
             (oComponents[2]->GetName() != "uncertainty" ||
              oComponents[2]->GetType().GetNumericDataType() != GDT_Float32)))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Wrong data type for %s",
                     poValuesArray->GetFullName().c_str());
            return nullptr;
        }

        const auto &apoDims = poValuesArray->GetDimensions();
        if (apoDims[0]->GetSize() != static_cast<unsigned>(poDS->nRasterYSize))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "numPointsLatitudinal(=%d) doesn't match first dimension "
                     "size of %s (=%d)",
                     poDS->nRasterYSize, poValuesArray->GetFullName().c_str(),
                     static_cast<int>(apoDims[0]->GetSize()));
            return nullptr;
        }
        if (apoDims[1]->GetSize() != static_cast<unsigned>(poDS->nRasterXSize))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "numPointsLongitudinal(=%d) doesn't match second "
                     "dimension size of %s (=%d)",
                     poDS->nRasterXSize, poValuesArray->GetFullName().c_str(),
                     static_cast<int>(apoDims[1]->GetSize()));
            return nullptr;
        }

        if (bNorthUp)
            poValuesArray = poValuesArray->GetView("[::-1,...]");

        // Create waterLevelHeight band
        auto poWaterLevelHeight =
            poValuesArray->GetView("[\"waterLevelHeight\"]");
        auto poWaterLevelHeightDS = std::unique_ptr<GDALDataset>(
            poWaterLevelHeight->AsClassicDataset(1, 0));
        auto poWaterLevelHeightBand =
            std::make_unique<S104RasterBand>(std::move(poWaterLevelHeightDS));
        poWaterLevelHeightBand->SetDescription("waterLevelHeight");
        poWaterLevelHeightBand->m_osUnitType = "metre";
        poDS->SetBand(1, poWaterLevelHeightBand.release());

        // Create waterLevelTrend band
        auto poWaterLevelTrend =
            poValuesArray->GetView("[\"waterLevelTrend\"]");
        auto poWaterLevelTrendDS = std::unique_ptr<GDALDataset>(
            poWaterLevelTrend->AsClassicDataset(1, 0));
        auto poWaterLevelTrendBand =
            std::make_unique<S104RasterBand>(std::move(poWaterLevelTrendDS));
        poWaterLevelTrendBand->SetDescription("waterLevelTrend");

        // From D-5.3 Water Level Trend of S-101 v1.1 spec
        auto poRAT = std::make_unique<GDALDefaultRasterAttributeTable>();
        poRAT->CreateColumn("code", GFT_Integer, GFU_MinMax);
        poRAT->CreateColumn("label", GFT_String, GFU_Generic);
        poRAT->CreateColumn("definition", GFT_String, GFU_Generic);

        const struct
        {
            int nCode;
            const char *pszLabel;
            const char *pszDefinition;
        } aoRatValues[] = {
            {0, "Nodata", "No data"},
            {1, "Decreasing", "Becoming smaller in magnitude"},
            {2, "Increasing", "Becoming larger in magnitude"},
            {3, "Steady", "Constant"},
        };

        int iRow = 0;
        for (const auto &oRecord : aoRatValues)
        {
            int iCol = 0;
            poRAT->SetValue(iRow, iCol++, oRecord.nCode);
            poRAT->SetValue(iRow, iCol++, oRecord.pszLabel);
            poRAT->SetValue(iRow, iCol++, oRecord.pszDefinition);
            ++iRow;
        }

        poWaterLevelTrendBand->m_poRAT = std::move(poRAT);

        poDS->SetBand(2, poWaterLevelTrendBand.release());

        if (oComponents.size() == 3)
        {
            // Create uncertainty band
            auto poUncertaintyArray =
                poValuesArray->GetView("[\"uncertainty\"]");
            auto poUncertaintyDS = std::unique_ptr<GDALDataset>(
                poUncertaintyArray->AsClassicDataset(1, 0));
            auto poUncertaintyBand =
                std::make_unique<S104RasterBand>(std::move(poUncertaintyDS));
            poUncertaintyBand->SetDescription("uncertainty");
            poUncertaintyBand->m_osUnitType = "metre";
            poDS->SetBand(3, poUncertaintyBand.release());
        }

        auto poUncertaintyDataset =
            poFeatureInstance->OpenMDArray("uncertainty");
        if (poUncertaintyDataset)
        {
            const auto &apoUncertaintyDims =
                poUncertaintyDataset->GetDimensions();
            const auto &oUncertaintyType = poUncertaintyDataset->GetDataType();
            if (apoUncertaintyDims.size() == 1 &&
                apoUncertaintyDims[0]->GetSize() == 1 &&
                oUncertaintyType.GetClass() == GEDTC_COMPOUND)
            {
                const auto &oUncertaintyComponents =
                    oUncertaintyType.GetComponents();
                if (oUncertaintyComponents.size() == 2 &&
                    oUncertaintyComponents[1]->GetType().GetClass() ==
                        GEDTC_NUMERIC)
                {
                    auto poView = poUncertaintyDataset->GetView(
                        std::string("[\"")
                            .append(oUncertaintyComponents[1]->GetName())
                            .append("\"]"));
                    double dfVal = 0;
                    const GUInt64 arrayStartIdx[] = {0};
                    const size_t count[] = {1};
                    const GInt64 arrayStep[] = {0};
                    const GPtrDiff_t bufferStride[] = {0};
                    if (poView &&
                        poView->Read(
                            arrayStartIdx, count, arrayStep, bufferStride,
                            GDALExtendedDataType::Create(GDT_Float64), &dfVal))
                    {
                        poDS->GDALDataset::SetMetadataItem(
                            "uncertainty", CPLSPrintf("%f", dfVal));
                    }
                }
            }
        }
    }

    poDS->GDALDataset::SetMetadataItem(GDALMD_AREA_OR_POINT, GDALMD_AOP_POINT);

    // Setup/check for pam .aux.xml.
    if (osFilename != poOpenInfo->pszFilename)
    {
        poDS->SetSubdatasetName((osFeatureInstance + "/" + osGroup).c_str());
        poDS->SetPhysicalFilename(osFilename.c_str());
    }
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->TryLoadXML();

    // Setup overviews.
    poDS->oOvManager.Initialize(poDS.get(), osFilename.c_str());

    return poDS.release();
}

/************************************************************************/
/*                             S104Creator                              */
/************************************************************************/

class S104Creator final : public S100BaseWriter
{
  public:
    S104Creator(const char *pszDestFilename, GDALDataset *poSrcDS,
                CSLConstList papszOptions)
        : S100BaseWriter(pszDestFilename, poSrcDS, papszOptions)
    {
    }

    ~S104Creator() override;

    bool Create(GDALProgressFunc pfnProgress, void *pProgressData);

    static constexpr const char *FEATURE_TYPE = "WaterLevel";

  protected:
    bool Close() override
    {
        return BaseClose();
    }

  private:
    bool WriteFeatureGroupAttributes();
    bool WriteUncertaintyDataset();
    bool FillFeatureInstanceGroup(
        const std::map<std::string, std::variant<GDALDataset *, std::string>>
            &oMapTimestampToDS,
        GDALProgressFunc pfnProgress, void *pProgressData);
    bool CopyValues(GDALDataset *poSrcDS, GDALProgressFunc pfnProgress,
                    void *pProgressData);
    bool CreateGroupF();
};

/************************************************************************/
/*                     S104Creator::~S104Creator()                      */
/************************************************************************/

S104Creator::~S104Creator()
{
    S104Creator::Close();
}

/************************************************************************/
/*                        S104Creator::Create()                         */
/************************************************************************/

bool S104Creator::Create(GDALProgressFunc pfnProgress, void *pProgressData)
{
    CPLStringList aosDatasets(
        CSLTokenizeString2(m_aosOptions.FetchNameValue("DATASETS"), ",", 0));
    if (m_poSrcDS->GetRasterCount() == 0 && aosDatasets.empty())
    {
        // Deal with S104 -> S104 translation;
        CSLConstList papszSubdatasets = m_poSrcDS->GetMetadata("SUBDATASETS");
        if (papszSubdatasets)
        {
            int iSubDS = 0;
            std::string osFirstDataset;
            std::string osDatasets;
            for (const auto &[pszItem, pszValue] :
                 cpl::IterateNameValue(papszSubdatasets))
            {
                if (STARTS_WITH(pszItem, "SUBDATASET_") &&
                    cpl::ends_with(std::string_view(pszItem), "_NAME") &&
                    STARTS_WITH(pszValue, "S104:"))
                {
                    if (strstr(pszValue, ":WaterLevel."))
                    {
                        auto poTmpDS =
                            std::unique_ptr<GDALDataset>(GDALDataset::Open(
                                pszValue,
                                GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR));
                        if (!poTmpDS)
                            return false;
                        CPLStringList aosOptions(m_aosOptions);
                        if (iSubDS > 0)
                            aosOptions.SetNameValue("APPEND_SUBDATASET", "YES");
                        S104Creator oAuxCreator(m_osDestFilename.c_str(),
                                                poTmpDS.get(),
                                                aosOptions.List());
                        const int nSubDSCount =
                            ((CSLCount(papszSubdatasets) + 1) / 2);
                        std::unique_ptr<void,
                                        decltype(&GDALDestroyScaledProgress)>
                            pScaledProgressData(
                                GDALCreateScaledProgress(
                                    static_cast<double>(iSubDS) / nSubDSCount,
                                    static_cast<double>(iSubDS + 1) /
                                        nSubDSCount,
                                    pfnProgress, pProgressData),
                                GDALDestroyScaledProgress);
                        ++iSubDS;
                        if (!oAuxCreator.Create(GDALScaledProgress,
                                                pScaledProgressData.get()))
                            return false;
                    }
                    else
                    {
                        if (osFirstDataset.empty())
                            osFirstDataset = pszValue;
                        if (!osDatasets.empty())
                            osDatasets += ',';
                        osDatasets += pszValue;
                    }
                }
            }
            if (iSubDS > 0)
            {
                return true;
            }
            else if (!osDatasets.empty())
            {
                auto poTmpDS = std::unique_ptr<GDALDataset>(
                    GDALDataset::Open(osFirstDataset.c_str(),
                                      GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR));
                if (!poTmpDS)
                    return false;
                CPLStringList aosOptions(m_aosOptions);
                aosOptions.SetNameValue("DATASETS", osDatasets.c_str());
                S104Creator oAuxCreator(m_osDestFilename.c_str(), poTmpDS.get(),
                                        aosOptions.List());
                return oAuxCreator.Create(pfnProgress, pProgressData);
            }
        }
    }

    if (m_poSrcDS->GetRasterCount() != 2 && m_poSrcDS->GetRasterCount() != 3)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Source dataset %s must have two or three bands",
                 m_poSrcDS->GetDescription());
        return false;
    }

    if (!BaseChecks("S104", /* crsMustBeEPSG = */ false,
                    /* verticalDatumRequired = */ true))
        return false;

    std::map<std::string, std::variant<GDALDataset *, std::string>>
        oMapTimestampToDS;
    CPLStringList aosDatasetsTimePoint(CSLTokenizeString2(
        m_aosOptions.FetchNameValue("DATASETS_TIME_POINT"), ",", 0));
    if (!aosDatasets.empty())
    {
        if (!aosDatasetsTimePoint.empty() &&
            aosDatasetsTimePoint.size() != aosDatasets.size())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "DATASETS_TIME_POINT does not have the same number of "
                     "values as DATASETS");
            return false;
        }
        int i = 0;
        for (const char *pszDataset : aosDatasets)
        {
            auto poDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
                pszDataset, GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR));
            if (!poDS)
                return false;
            if (poDS->GetRasterXSize() != m_poSrcDS->GetRasterXSize() ||
                poDS->GetRasterYSize() != m_poSrcDS->GetRasterYSize())
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Dataset %s does not have the same dimensions as %s",
                         poDS->GetDescription(), m_poSrcDS->GetDescription());
                return false;
            }
            if (poDS->GetRasterCount() != m_poSrcDS->GetRasterCount())
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Dataset %s must have %d bands",
                         poDS->GetDescription(), m_poSrcDS->GetRasterCount());
                return false;
            }
            auto poSRS = poDS->GetSpatialRef();
            if (!poSRS || !poSRS->IsSame(m_poSRS))
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Dataset %s does not have the same CRS as %s",
                         poDS->GetDescription(), m_poSrcDS->GetDescription());
                return false;
            }
            GDALGeoTransform gt;
            if (poDS->GetGeoTransform(gt) != CE_None || gt != m_gt)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Dataset %s does not have the same geotransform as %s",
                         poDS->GetDescription(), m_poSrcDS->GetDescription());
                return false;
            }
            const char *pszVerticalDatum =
                poDS->GetMetadataItem("VERTICAL_DATUM");
            if (pszVerticalDatum)
            {
                const int nVerticalDatum =
                    S100GetVerticalDatumCodeFromNameOrAbbrev(pszVerticalDatum);
                if (nVerticalDatum != m_nVerticalDatum)
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Dataset %s does not have the same vertical datum "
                             "as %s",
                             poDS->GetDescription(),
                             m_poSrcDS->GetDescription());
                    return false;
                }
            }
            const char *pszTimePoint = poDS->GetMetadataItem("timePoint");
            if (!pszTimePoint && !aosDatasetsTimePoint.empty())
                pszTimePoint = aosDatasetsTimePoint[i];
            if (!pszTimePoint)
            {
                CPLError(
                    CE_Failure, CPLE_NotSupported,
                    "Dataset %s does not have a timePoint metadata item, and "
                    "the DATASETS_TIME_POINT creation option is not set",
                    poDS->GetDescription());
                return false;
            }
            if (strlen(pszTimePoint) != strlen("YYYYMMDDTHHMMSSZ") ||
                pszTimePoint[8] != 'T' || pszTimePoint[15] != 'Z')
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "timePoint value for dataset %s is %s, but does not "
                         "conform to a YYYYMMDDTHHMMSSZ datetime value.",
                         poDS->GetDescription(), pszTimePoint);
                return false;
            }
            if (cpl::contains(oMapTimestampToDS, pszTimePoint))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Several datasets are at timePoint %s.", pszTimePoint);
                return false;
            }
            oMapTimestampToDS[pszTimePoint] = pszDataset;
            ++i;
        }
    }

    {
        const char *pszTimePoint = m_aosOptions.FetchNameValueDef(
            "TIME_POINT", m_poSrcDS->GetMetadataItem("timePoint"));
        if (!pszTimePoint)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "TIME_POINT creation option value must "
                     "be set, or source dataset must have a timePoint metadata "
                     "item.");
            return false;
        }
        if (strlen(pszTimePoint) != strlen("YYYYMMDDTHHMMSSZ") ||
            pszTimePoint[8] != 'T' || pszTimePoint[15] != 'Z')
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "TIME_POINT creation option value must "
                     "be set to a YYYYMMDDTHHMMSSZ datetime value.");
            return false;
        }

        if (oMapTimestampToDS.empty())
        {
            oMapTimestampToDS[pszTimePoint] = m_poSrcDS;
        }
        else
        {
            const auto oIter = oMapTimestampToDS.find(pszTimePoint);
            if (oIter != oMapTimestampToDS.end() &&
                CPLString(std::get<std::string>(oIter->second))
                        .replaceAll('\\', '/') !=
                    CPLString(m_poSrcDS->GetDescription())
                        .replaceAll('\\', '/'))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Several datasets are at timePoint %s (%s vs %s).",
                         pszTimePoint,
                         std::get<std::string>(oIter->second).c_str(),
                         m_poSrcDS->GetDescription());
                return false;
            }
        }
    }
    if (oMapTimestampToDS.size() > 999)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Only up to 999 datasets are supported for a same vertical datum");
        return false;
    }

    if (m_poSRS->IsVertical() || m_poSRS->IsCompound() || m_poSRS->IsLocal() ||
        m_poSRS->GetAxesCount() != 2)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The CRS must be a geographic 2D or projected 2D CRS");
        return false;
    }

    const bool bAppendSubdataset =
        CPLTestBool(m_aosOptions.FetchNameValueDef("APPEND_SUBDATASET", "NO"));
    if (bAppendSubdataset)
    {
        GDALOpenInfo oOpenInfo(m_osDestFilename.c_str(), GA_ReadOnly);
        auto poOriDS =
            std::unique_ptr<GDALDataset>(S104Dataset::Open(&oOpenInfo));
        if (!poOriDS)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s is not a valid existing S104 dataset",
                     m_osDestFilename.c_str());
            return false;
        }
        const auto poOriSRS = poOriDS->GetSpatialRef();
        if (!poOriSRS)
        {
            // shouldn't happen
            return false;
        }
        if (!poOriSRS->IsSame(m_poSRS))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "CRS of %s is not the same as the one of %s",
                     m_osDestFilename.c_str(), m_poSrcDS->GetDescription());
            return false;
        }
        poOriDS.reset();

        OGREnvelope sExtent;
        if (m_poSrcDS->GetExtentWGS84LongLat(&sExtent) != OGRERR_NONE)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot get dataset extent in WGS84 longitude/latitude");
            return false;
        }

        bool ret = OpenFileUpdateMode();
        if (ret)
        {
            m_featureGroup.reset(H5_CHECK(H5Gopen(m_hdf5, "WaterLevel")));
        }

        ret = ret && m_featureGroup;
        double dfNumInstances = 0;
        ret = ret && GH5_FetchAttribute(m_featureGroup, "numInstances",
                                        dfNumInstances, true);
        if (ret && !(dfNumInstances >= 1 && dfNumInstances <= 99 &&
                     std::round(dfNumInstances) == dfNumInstances))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid value for numInstances");
            ret = false;
        }
        else if (ret && dfNumInstances == 99)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Too many existing feature instances");
            ret = false;
        }
        else
        {
            double dfMainVerticalDatum = 0;
            ret = ret && GH5_FetchAttribute(m_hdf5, "verticalDatum",
                                            dfMainVerticalDatum, true);

            const int newNumInstances = static_cast<int>(dfNumInstances) + 1;
            ret = ret && GH5_WriteAttribute(m_featureGroup, "numInstances",
                                            newNumInstances);
            ret = ret && CreateFeatureInstanceGroup(
                             CPLSPrintf("WaterLevel.%02d", newNumInstances));
            ret = ret && FillFeatureInstanceGroup(oMapTimestampToDS,
                                                  pfnProgress, pProgressData);
            if (dfMainVerticalDatum != m_nVerticalDatum)
            {
                ret = ret && WriteVerticalDatumReference(
                                 m_featureInstanceGroup,
                                 m_nVerticalDatum <= 1024 ? 1 : 2);
                ret =
                    ret && WriteVerticalDatum(m_featureInstanceGroup,
                                              H5T_STD_I32LE, m_nVerticalDatum);
            }
        }

        return Close() && ret;
    }
    else
    {
        bool ret = CreateFile();
        ret = ret && WriteProductSpecification("INT.IHO.S-104.2.0");
        ret = ret && WriteIssueDate();
        ret = ret && WriteIssueTime(/* bAutogenerateFromCurrent = */ true);
        ret = ret && WriteHorizontalCRS();
        ret = ret && WriteTopLevelBoundingBox();

        const char *pszGeographicIdentifier = m_aosOptions.FetchNameValueDef(
            "GEOGRAPHIC_IDENTIFIER",
            m_poSrcDS->GetMetadataItem("geographicIdentifier"));
        if (pszGeographicIdentifier)
        {
            ret =
                ret && WriteVarLengthStringValue(m_hdf5, "geographicIdentifier",
                                                 pszGeographicIdentifier);
        }

        const char *pszVerticalCS = m_aosOptions.FetchNameValueDef(
            "VERTICAL_CS", m_poSrcDS->GetMetadataItem("verticalCS"));
        if (!pszVerticalCS)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "VERTICAL_CS creation option must be specified");
            return false;
        }
        const int nVerticalCS = EQUAL(pszVerticalCS, "DEPTH") ? 6498
                                : EQUAL(pszVerticalCS, "HEIGHT")
                                    ? 6499
                                    : atoi(pszVerticalCS);
        if (nVerticalCS != 6498 && nVerticalCS != 6499)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "VERTICAL_CS creation option must be set either to 6498 "
                     "(depth/down, metre), or 6499 (height/up, metre)");
            return false;
        }

        ret = ret && WriteVerticalCS(nVerticalCS);
        ret = ret && WriteVerticalCoordinateBase(2);  // verticalDatum
        // 1=s100VerticalDatum, 2=EPSG
        ret = ret && WriteVerticalDatumReference(
                         m_hdf5, m_nVerticalDatum <= 1024 ? 1 : 2);
        ret =
            ret && WriteVerticalDatum(m_hdf5, H5T_STD_I32LE, m_nVerticalDatum);

        const char *pszWaterLevelTrendThreshold =
            m_aosOptions.FetchNameValueDef(
                "WATER_LEVEL_TREND_THRESHOLD",
                m_poSrcDS->GetMetadataItem("waterLevelTrendThreshold"));
        if (!pszWaterLevelTrendThreshold)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "WATER_LEVEL_TREND_THRESHOLD creation option must be "
                     "specified.");
            return false;
        }
        if (CPLGetValueType(pszWaterLevelTrendThreshold) == CPL_VALUE_STRING)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "WATER_LEVEL_TREND_THRESHOLD creation option value must "
                     "be a numeric value.");
            return false;
        }
        ret = ret && WriteFloat32Value(m_hdf5, "waterLevelTrendThreshold",
                                       CPLAtof(pszWaterLevelTrendThreshold));

        const char *pszDatasetDeliveryInterval = m_aosOptions.FetchNameValueDef(
            "DATASET_DELIVERY_INTERVAL",
            m_poSrcDS->GetMetadataItem("datasetDeliveryInterval"));
        if (pszDatasetDeliveryInterval)
        {
            ret = ret &&
                  WriteVarLengthStringValue(m_hdf5, "datasetDeliveryInterval",
                                            pszDatasetDeliveryInterval);
        }

        const char *pszTrendInterval = m_aosOptions.FetchNameValueDef(
            "TREND_INTERVAL", m_poSrcDS->GetMetadataItem("trendInterval"));
        if (pszTrendInterval)
        {
            if (CPLGetValueType(pszTrendInterval) != CPL_VALUE_INTEGER)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "TREND_INTERVAL creation option value must "
                         "be an integer value.");
                return false;
            }
            ret = ret && WriteUInt32Value(m_hdf5, "trendInterval",
                                          atoi(pszTrendInterval));
        }

        // WaterLevel
        ret = ret && CreateFeatureGroup(FEATURE_TYPE);
        ret = ret && WriteFeatureGroupAttributes();
        ret = ret && WriteAxisNames(m_featureGroup);

        ret = ret && CreateFeatureInstanceGroup("WaterLevel.01");
        ret = ret && FillFeatureInstanceGroup(oMapTimestampToDS, pfnProgress,
                                              pProgressData);

        ret = ret && CreateGroupF();

        return Close() && ret;
    }
}

/************************************************************************/
/*              S104Creator::WriteFeatureGroupAttributes()              */
/************************************************************************/

bool S104Creator::WriteFeatureGroupAttributes()
{
    CPLAssert(m_featureGroup);

    // 4 = all (recommended)
    const char *pszCommonPointRule = m_aosOptions.FetchNameValueDef(
        "COMMON_POINT_RULE", m_poSrcDS->GetMetadataItem("commonPointRule"));
    if (!pszCommonPointRule)
        pszCommonPointRule = "4";  // all (recommended)
    const int nCommonPointRule = atoi(pszCommonPointRule);
    bool ret = WriteCommonPointRule(m_featureGroup, nCommonPointRule);
    ret = ret && WriteDataCodingFormat(m_featureGroup, 2);  // Regular grid
    ret = ret && WriteDataOffsetCode(m_featureGroup, 5);    // Center of cell
    ret = ret && WriteDimension(m_featureGroup, 2);
    const char *pszHorizontalPositionUncertainty =
        m_aosOptions.FetchNameValueDef(
            "HORIZONTAL_POSITION_UNCERTAINTY",
            m_poSrcDS->GetMetadataItem("horizontalPositionUncertainty"));
    ret =
        ret &&
        WriteHorizontalPositionUncertainty(
            m_featureGroup,
            pszHorizontalPositionUncertainty &&
                    pszHorizontalPositionUncertainty[0]
                ? static_cast<float>(CPLAtof(pszHorizontalPositionUncertainty))
                : -1.0f);
    const char *pszVerticalUncertainty = m_aosOptions.FetchNameValueDef(
        "VERTICAL_UNCERTAINTY",
        m_poSrcDS->GetMetadataItem("verticalUncertainty"));
    ret = ret && WriteVerticalUncertainty(
                     m_featureGroup,
                     pszVerticalUncertainty && pszVerticalUncertainty[0]
                         ? static_cast<float>(CPLAtof(pszVerticalUncertainty))
                         : -1.0f);
    const char *pszTimeUncertainty = m_aosOptions.FetchNameValueDef(
        "TIME_UNCERTAINTY", m_poSrcDS->GetMetadataItem("timeUncertainty"));
    if (pszTimeUncertainty)
        WriteFloat32Value(m_featureGroup, "timeUncertainty",
                          CPLAtof(pszTimeUncertainty));
    const char *pszMethodWaterLevelProduct = m_aosOptions.FetchNameValueDef(
        "METHOD_WATER_LEVEL_PRODUCT",
        m_poSrcDS->GetMetadataItem("methodWaterLevelProduct"));
    if (pszMethodWaterLevelProduct)
        WriteVarLengthStringValue(m_featureGroup, "methodWaterLevelProduct",
                                  pszMethodWaterLevelProduct);
    ret = ret && WriteInterpolationType(m_featureGroup, 1);  // Nearest neighbor
    ret = ret && WriteNumInstances(m_featureGroup, H5T_STD_U32LE, 1);
    ret = ret && WriteSequencingRuleScanDirection(m_featureGroup,
                                                  m_poSRS->IsProjected()
                                                      ? "Easting, Northing"
                                                      : "Longitude, Latitude");
    ret = ret && WriteSequencingRuleType(m_featureGroup, 1);  // Linear
    return ret;
}

/************************************************************************/
/*                S104Creator::WriteUncertaintyDataset()                */
/************************************************************************/

bool S104Creator::WriteUncertaintyDataset()
{
    CPLAssert(m_featureInstanceGroup);

    GH5_HIDTypeHolder hDataType(
        H5_CHECK(H5Tcreate(H5T_COMPOUND, sizeof(char *) + sizeof(float))));
    GH5_HIDTypeHolder hVarLengthStringDataType(H5_CHECK(H5Tcopy(H5T_C_S1)));
    bool bRet =
        hVarLengthStringDataType &&
        H5_CHECK(H5Tset_size(hVarLengthStringDataType, H5T_VARIABLE)) >= 0;
    bRet = bRet && hVarLengthStringDataType &&
           H5_CHECK(
               H5Tset_strpad(hVarLengthStringDataType, H5T_STR_NULLTERM)) >= 0;
    bRet = bRet && hDataType &&
           H5_CHECK(H5Tinsert(hDataType, "name", 0,
                              hVarLengthStringDataType)) >= 0 &&
           H5_CHECK(H5Tinsert(hDataType, "value", sizeof(char *),
                              H5T_IEEE_F32LE)) >= 0;
    hsize_t dims[] = {1};
    GH5_HIDSpaceHolder hDataSpace(H5_CHECK(H5Screate_simple(1, dims, nullptr)));
    GH5_HIDDatasetHolder hDatasetID;
    GH5_HIDParametersHolder hParams(H5_CHECK(H5Pcreate(H5P_DATASET_CREATE)));
    bRet = bRet && hParams;
    if (bRet)
    {
        hDatasetID.reset(
            H5_CHECK(H5Dcreate(m_featureInstanceGroup, "uncertainty", hDataType,
                               hDataSpace, hParams)));
        bRet = hDatasetID;
    }

    GH5_HIDSpaceHolder hFileSpace;
    if (bRet)
    {
        hFileSpace.reset(H5_CHECK(H5Dget_space(hDatasetID)));
        bRet = hFileSpace;
    }
    H5OFFSET_TYPE offset[] = {0};
    hsize_t count[1] = {1};
    const char *pszName = "uncertainty";
    GByte abyValues[sizeof(char *) + sizeof(float)];
    memcpy(abyValues, &pszName, sizeof(char *));
    const char *pszUncertainty = m_aosOptions.FetchNameValueDef(
        "UNCERTAINTY", m_poSrcDS->GetMetadataItem("uncertainty"));
    float fVal =
        pszUncertainty ? static_cast<float>(CPLAtof(pszUncertainty)) : -1.0f;
    CPL_LSBPTR32(&fVal);
    memcpy(abyValues + sizeof(char *), &fVal, sizeof(fVal));
    bRet = bRet &&
           H5_CHECK(H5Sselect_hyperslab(hFileSpace, H5S_SELECT_SET, offset,
                                        nullptr, count, nullptr)) >= 0 &&
           H5_CHECK(H5Dwrite(hDatasetID, hDataType, hDataSpace, hFileSpace,
                             H5P_DEFAULT, abyValues)) >= 0;
    return bRet;
}

/************************************************************************/
/*               S104Creator::FillFeatureInstanceGroup()                */
/************************************************************************/

bool S104Creator::FillFeatureInstanceGroup(
    const std::map<std::string, std::variant<GDALDataset *, std::string>>
        &oMapTimestampToDS,
    GDALProgressFunc pfnProgress, void *pProgressData)
{
    bool ret = WriteFIGGridRelatedParameters(m_featureInstanceGroup);

    const int numInstances = static_cast<int>(oMapTimestampToDS.size());

    ret =
        ret && WriteNumGRP(m_featureInstanceGroup, H5T_STD_U32LE, numInstances);
    ret = ret && WriteUInt32Value(m_featureInstanceGroup, "numberOfTimes",
                                  numInstances);

    // Check if value groups are spaced at a regular time interval
    GIntBig nLastInterval = 0;
    GIntBig nLastTS = 0;
    for (const auto &[key, value] : oMapTimestampToDS)
    {
        CPL_IGNORE_RET_VAL(value);
        int nYear, nMonth, nDay, nHour, nMinute, nSecond;
        if (sscanf(key.c_str(), "%04d%02d%02dT%02d%02d%02dZ", &nYear, &nMonth,
                   &nDay, &nHour, &nMinute, &nSecond) == 6)
        {
            struct tm brokenDown;
            memset(&brokenDown, 0, sizeof(brokenDown));
            brokenDown.tm_year = nYear - 1900;
            brokenDown.tm_mon = nMonth - 1;
            brokenDown.tm_mday = nDay;
            brokenDown.tm_hour = nHour;
            brokenDown.tm_min = nMinute;
            brokenDown.tm_sec = nMinute;
            const GIntBig nTS = CPLYMDHMSToUnixTime(&brokenDown);
            if (nLastTS != 0)
            {
                if (nLastInterval == 0)
                {
                    nLastInterval = nTS - nLastTS;
                }
                else if (nLastInterval != nTS - nLastTS)
                {
                    nLastInterval = 0;
                    break;
                }
            }
            nLastTS = nTS;
        }
    }

    const char *pszTimeRecordInterval = m_aosOptions.FetchNameValueDef(
        "TIME_RECORD_INTERVAL",
        m_poSrcDS->GetMetadataItem("timeRecordInterval"));
    if (pszTimeRecordInterval)
    {
        ret = ret &&
              WriteUInt16Value(m_featureInstanceGroup, "timeRecordInterval",
                               atoi(pszTimeRecordInterval));
    }
    else if (nLastInterval > 0 && nLastInterval < 65536)
    {
        ret = ret &&
              WriteUInt16Value(m_featureInstanceGroup, "timeRecordInterval",
                               static_cast<int>(nLastInterval));
    }

    ret = ret && WriteVarLengthStringValue(
                     m_featureInstanceGroup, "dateTimeOfFirstRecord",
                     oMapTimestampToDS.begin()->first.c_str());
    ret = ret && WriteVarLengthStringValue(
                     m_featureInstanceGroup, "dateTimeOfLastRecord",
                     oMapTimestampToDS.rbegin()->first.c_str());

    const char *pszDataDynamicity = m_aosOptions.FetchNameValueDef(
        "DATA_DYNAMICITY", m_poSrcDS->GetMetadataItem("dataDynamicity"));
    if (!pszDataDynamicity)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "DATA_DYNAMICITY creation option must "
                 "be specified.");
        return false;
    }
    {
        GH5_HIDTypeHolder hDataDynamicityEnumDataType(
            H5_CHECK(H5Tenum_create(H5T_STD_U8LE)));
        ret = ret && hDataDynamicityEnumDataType;

        uint8_t val;
        val = 1;
        ret = ret && H5_CHECK(H5Tenum_insert(hDataDynamicityEnumDataType,
                                             "observation", &val)) >= 0;
        val = 2;
        ret = ret &&
              H5_CHECK(H5Tenum_insert(hDataDynamicityEnumDataType,
                                      "astronomicalPrediction", &val)) >= 0;
        val = 3;
        ret = ret && H5_CHECK(H5Tenum_insert(hDataDynamicityEnumDataType,
                                             "analysisOrHybrid", &val)) >= 0;
        val = 5;
        ret =
            ret && H5_CHECK(H5Tenum_insert(hDataDynamicityEnumDataType,
                                           "hydrodynamicForecast", &val)) >= 0;

        const int nDataDynamicity =
            EQUAL(pszDataDynamicity, "observation")              ? 1
            : EQUAL(pszDataDynamicity, "astronomicalPrediction") ? 2
            : EQUAL(pszDataDynamicity, "analysisOrHybrid")       ? 3
            : EQUAL(pszDataDynamicity, "hydrodynamicForecast")
                ? 5
                : atoi(pszDataDynamicity);
        if (nDataDynamicity != 1 && nDataDynamicity != 2 &&
            nDataDynamicity != 3 && nDataDynamicity != 5)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "DATA_DYNAMICITY creation option must "
                     "be set to observation/1, astronomicalPrediction/2, "
                     "analysisOrHybrid/3 or hydrodynamicForecast/5.");
            return false;
        }
        ret = ret &&
              GH5_CreateAttribute(m_featureInstanceGroup, "dataDynamicity",
                                  hDataDynamicityEnumDataType) &&
              GH5_WriteAttribute(m_featureInstanceGroup, "dataDynamicity",
                                 nDataDynamicity);
    }

    if (m_poSrcDS->GetRasterCount() == 2 ||
        m_aosOptions.FetchNameValue("UNCERTAINTY"))
    {
        ret = ret && WriteUncertaintyDataset();
    }

    int iInstance = 0;
    double dfLastRatio = 0;
    for (const auto &iter : oMapTimestampToDS)
    {
        ++iInstance;
        ret = ret && CreateValuesGroup(CPLSPrintf("Group_%03d", iInstance));

        ret = ret && WriteVarLengthStringValue(m_valuesGroup, "timePoint",
                                               iter.first.c_str());

        std::unique_ptr<GDALDataset> poTmpDSHolder;
        GDALDataset *poSrcDS;
        if (std::holds_alternative<std::string>(iter.second))
        {
            poTmpDSHolder.reset(
                GDALDataset::Open(std::get<std::string>(iter.second).c_str(),
                                  GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR));
            if (!poTmpDSHolder)
            {
                return false;
            }
            poSrcDS = poTmpDSHolder.get();
        }
        else
        {
            CPLAssert(std::holds_alternative<GDALDataset *>(iter.second));
            poSrcDS = std::get<GDALDataset *>(iter.second);
        }

        const double dfNewRatio = static_cast<double>(iInstance) / numInstances;
        std::unique_ptr<void, decltype(&GDALDestroyScaledProgress)>
            pScaledProgressData(
                GDALCreateScaledProgress(dfLastRatio, dfNewRatio, pfnProgress,
                                         pProgressData),
                GDALDestroyScaledProgress);
        ret = ret && CopyValues(poSrcDS, GDALScaledProgress,
                                pScaledProgressData.get());
        dfLastRatio = dfNewRatio;
    }

    return ret;
}

/************************************************************************/
/*                     S104Creator::CreateGroupF()                      */
/************************************************************************/

// Per S-104 v2.0 spec
#define MIN_WATER_LEVEL_HEIGHT_VALUE -99.99
#define MAX_WATER_LEVEL_HEIGHT_VALUE 99.99

#define STRINGIFY(x) #x
#define XSTRINGIFY(x) STRINGIFY(x)

bool S104Creator::CreateGroupF()
{
    bool ret = S100BaseWriter::CreateGroupF();

    CPLStringList aosFeatureCodes;
    aosFeatureCodes.push_back(FEATURE_TYPE);
    ret = ret && WriteOneDimensionalVarLengthStringArray(
                     m_GroupF, "featureCode", aosFeatureCodes.List());

    {
        std::vector<std::array<const char *, GROUP_F_DATASET_FIELD_COUNT>> rows{
            {"waterLevelHeight", "Water Level Height", "metre", "-9999.00",
             "H5T_FLOAT", XSTRINGIFY(MIN_WATER_LEVEL_HEIGHT_VALUE),
             XSTRINGIFY(MAX_WATER_LEVEL_HEIGHT_VALUE), "closedInterval"},
            {"waterLevelTrend", "Water Level Trend", "", "0", "H5T_ENUM", "",
             "", ""},
            {"uncertainty", "Uncertainty", "metre", "-1.00", "H5T_FLOAT",
             "0.00", "99.99", "closedInterval"}};
        rows.resize(m_poSrcDS->GetRasterCount());
        ret = ret && WriteGroupFDataset(FEATURE_TYPE, rows);
    }

    return ret;
}

/************************************************************************/
/*                      S104Creator::CopyValues()                       */
/************************************************************************/

bool S104Creator::CopyValues(GDALDataset *poSrcDS, GDALProgressFunc pfnProgress,
                             void *pProgressData)
{
    CPLAssert(m_valuesGroup.get() >= 0);

    const int nYSize = poSrcDS->GetRasterYSize();
    const int nXSize = poSrcDS->GetRasterXSize();

    hsize_t dims[] = {static_cast<hsize_t>(nYSize),
                      static_cast<hsize_t>(nXSize)};

    GH5_HIDSpaceHolder hDataSpace(H5_CHECK(H5Screate_simple(2, dims, nullptr)));
    bool bRet = hDataSpace;

    const bool bDeflate =
        EQUAL(m_aosOptions.FetchNameValueDef("COMPRESS", "DEFLATE"), "DEFLATE");
    const int nCompressionLevel =
        atoi(m_aosOptions.FetchNameValueDef("ZLEVEL", "6"));
    const int nBlockSize =
        std::min(4096, std::max(100, atoi(m_aosOptions.FetchNameValueDef(
                                         "BLOCK_SIZE", "100"))));
    const int nBlockXSize = std::min(nXSize, nBlockSize);
    const int nBlockYSize = std::min(nYSize, nBlockSize);
    constexpr float fNoDataValueHeight = -9999.0f;
    constexpr GByte nNoDataValueTrend = 0;
    constexpr float fNoDataValueUncertainty = -1.0f;
    const int nComponents = poSrcDS->GetRasterCount();

    GH5_HIDTypeHolder hTrendEnumDataType(
        H5_CHECK(H5Tenum_create(H5T_STD_U8LE)));
    bRet = bRet && hTrendEnumDataType;
    {
        uint8_t val;
        val = 1;
        bRet = bRet && H5_CHECK(H5Tenum_insert(hTrendEnumDataType, "Decreasing",
                                               &val)) >= 0;
        val = 2;
        bRet = bRet && H5_CHECK(H5Tenum_insert(hTrendEnumDataType, "Increasing",
                                               &val)) >= 0;
        val = 3;
        bRet = bRet && H5_CHECK(H5Tenum_insert(hTrendEnumDataType, "Steady",
                                               &val)) >= 0;
    }

    GH5_HIDTypeHolder hDataType(H5_CHECK(
        H5Tcreate(H5T_COMPOUND, sizeof(float) + sizeof(GByte) +
                                    (nComponents == 3 ? sizeof(float) : 0))));
    bRet = bRet && hDataType &&
           H5_CHECK(H5Tinsert(hDataType, "waterLevelHeight", 0,
                              H5T_IEEE_F32LE)) >= 0 &&
           H5_CHECK(H5Tinsert(hDataType, "waterLevelTrend", sizeof(float),
                              hTrendEnumDataType)) >= 0;
    if (nComponents == 3 && bRet)
    {
        bRet = H5_CHECK(H5Tinsert(hDataType, "uncertainty",
                                  sizeof(float) + sizeof(GByte),
                                  H5T_IEEE_F32LE)) >= 0;
    }

    hsize_t chunk_size[] = {static_cast<hsize_t>(nBlockYSize),
                            static_cast<hsize_t>(nBlockXSize)};

    GH5_HIDParametersHolder hParams(H5_CHECK(H5Pcreate(H5P_DATASET_CREATE)));
    bRet = bRet && hParams &&
           H5_CHECK(H5Pset_fill_time(hParams, H5D_FILL_TIME_ALLOC)) >= 0 &&
           H5_CHECK(H5Pset_layout(hParams, H5D_CHUNKED)) >= 0 &&
           H5_CHECK(H5Pset_chunk(hParams, 2, chunk_size)) >= 0;

    if (bRet && bDeflate)
    {
        bRet = H5_CHECK(H5Pset_deflate(hParams, nCompressionLevel)) >= 0;
    }

    GH5_HIDDatasetHolder hDatasetID;
    if (bRet)
    {
        hDatasetID.reset(H5_CHECK(H5Dcreate(m_valuesGroup, "values", hDataType,
                                            hDataSpace, hParams)));
        bRet = hDatasetID;
    }

    GH5_HIDSpaceHolder hFileSpace;
    if (bRet)
    {
        hFileSpace.reset(H5_CHECK(H5Dget_space(hDatasetID)));
        bRet = hFileSpace;
    }

    const int nYBlocks = static_cast<int>(DIV_ROUND_UP(nYSize, nBlockYSize));
    const int nXBlocks = static_cast<int>(DIV_ROUND_UP(nXSize, nBlockXSize));
    std::vector<float> afValues(static_cast<size_t>(nBlockYSize) * nBlockXSize *
                                nComponents);
    std::vector<GByte> abyValues(
        static_cast<size_t>(nBlockYSize) * nBlockXSize *
        (sizeof(float) + sizeof(GByte) + sizeof(float)));
    const bool bReverseY = m_gt.yscale < 0;

    float fMinHeight = std::numeric_limits<float>::infinity();
    float fMaxHeight = -std::numeric_limits<float>::infinity();
    float fMinTrend = std::numeric_limits<float>::infinity();
    float fMaxTrend = -std::numeric_limits<float>::infinity();
    float fMinUncertainty = std::numeric_limits<float>::infinity();
    float fMaxUncertainty = -std::numeric_limits<float>::infinity();

    int bHasNoDataBand1 = FALSE;
    const double dfSrcNoDataBand1 =
        poSrcDS->GetRasterBand(1)->GetNoDataValue(&bHasNoDataBand1);
    const float fSrcNoDataBand1 = static_cast<float>(dfSrcNoDataBand1);

    int bHasNoDataBand3 = FALSE;
    const double dfSrcNoDataBand3 =
        nComponents == 3
            ? poSrcDS->GetRasterBand(3)->GetNoDataValue(&bHasNoDataBand3)
            : 0.0;
    const float fSrcNoDataBand3 = static_cast<float>(dfSrcNoDataBand3);

    for (int iY = 0; iY < nYBlocks && bRet; iY++)
    {
        const int nSrcYOff = bReverseY
                                 ? std::max(0, nYSize - (iY + 1) * nBlockYSize)
                                 : iY * nBlockYSize;
        const int nReqCountY = std::min(nBlockYSize, nYSize - iY * nBlockYSize);
        for (int iX = 0; iX < nXBlocks && bRet; iX++)
        {
            const int nReqCountX =
                std::min(nBlockXSize, nXSize - iX * nBlockXSize);

            bRet =
                poSrcDS->RasterIO(
                    GF_Read, iX * nBlockXSize, nSrcYOff, nReqCountX, nReqCountY,
                    bReverseY ? afValues.data() +
                                    (nReqCountY - 1) * nReqCountX * nComponents
                              : afValues.data(),
                    nReqCountX, nReqCountY, GDT_Float32, nComponents, nullptr,
                    static_cast<int>(sizeof(float)) * nComponents,
                    bReverseY ? -static_cast<GPtrDiff_t>(sizeof(float)) *
                                    nComponents * nReqCountX
                              : 0,
                    sizeof(float), nullptr) == CE_None;

            if (bRet)
            {
                size_t nOffset = 0;
                for (int i = 0; i < nReqCountY * nReqCountX; i++)
                {
                    {
                        float fVal = afValues[i * nComponents];
                        if ((bHasNoDataBand1 && fVal == fSrcNoDataBand1) ||
                            std::isnan(fVal))
                        {
                            fVal = fNoDataValueHeight;
                        }
                        else
                        {
                            fMinHeight = std::min(fMinHeight, fVal);
                            fMaxHeight = std::max(fMaxHeight, fVal);
                        }
                        CPL_LSBPTR32(&fVal);
                        memcpy(abyValues.data() + nOffset, &fVal, sizeof(fVal));
                        nOffset += sizeof(fVal);
                    }
                    {
                        const float fVal = afValues[i * nComponents + 1];
                        if (fVal != nNoDataValueTrend)
                        {
                            fMinTrend = std::min(fMinTrend, fVal);
                            fMaxTrend = std::max(fMaxTrend, fVal);
                        }
                        abyValues[nOffset] = static_cast<GByte>(fVal);
                        nOffset += sizeof(GByte);
                    }
                    if (nComponents == 3)
                    {
                        float fVal = afValues[i * nComponents + 2];
                        if ((bHasNoDataBand3 && fVal == fSrcNoDataBand3) ||
                            std::isnan(fVal))
                        {
                            fVal = fNoDataValueUncertainty;
                        }
                        else
                        {
                            fMinUncertainty = std::min(fMinUncertainty, fVal);
                            fMaxUncertainty = std::max(fMaxUncertainty, fVal);
                        }
                        CPL_LSBPTR32(&fVal);
                        memcpy(abyValues.data() + nOffset, &fVal, sizeof(fVal));
                        nOffset += sizeof(fVal);
                    }
                }
            }

            H5OFFSET_TYPE offset[] = {
                static_cast<H5OFFSET_TYPE>(iY) *
                    static_cast<H5OFFSET_TYPE>(nBlockYSize),
                static_cast<H5OFFSET_TYPE>(iX) *
                    static_cast<H5OFFSET_TYPE>(nBlockXSize)};
            hsize_t count[2] = {static_cast<hsize_t>(nReqCountY),
                                static_cast<hsize_t>(nReqCountX)};
            GH5_HIDSpaceHolder hMemSpace(
                H5_CHECK(H5Screate_simple(2, count, nullptr)));
            bRet =
                bRet &&
                H5_CHECK(H5Sselect_hyperslab(hFileSpace, H5S_SELECT_SET, offset,
                                             nullptr, count, nullptr)) >= 0 &&
                hMemSpace &&
                H5_CHECK(H5Dwrite(hDatasetID, hDataType, hMemSpace, hFileSpace,
                                  H5P_DEFAULT, abyValues.data())) >= 0 &&
                pfnProgress((static_cast<double>(iY) * nXBlocks + iX + 1) /
                                (static_cast<double>(nXBlocks) * nYBlocks),
                            "", pProgressData) != 0;
        }
    }

    if (fMinHeight > fMaxHeight)
    {
        fMinHeight = fMaxHeight = fNoDataValueHeight;
    }
    else if (!(fMinHeight >= MIN_WATER_LEVEL_HEIGHT_VALUE &&
               fMaxHeight <= MAX_WATER_LEVEL_HEIGHT_VALUE))
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Range of water level height in the dataset is [%f, %f] "
                 "whereas the "
                 "allowed range is [%.2f, %.2f]",
                 fMinHeight, fMaxHeight, MIN_WATER_LEVEL_HEIGHT_VALUE,
                 MAX_WATER_LEVEL_HEIGHT_VALUE);
    }

    if (fMaxTrend >= fMinTrend && fMinTrend < 1)
    {
        CPLError(
            CE_Warning, CPLE_AppDefined,
            "Negative water level trend value found, which is not allowed");
    }
    if (fMaxTrend >= fMinTrend && fMaxTrend > 3)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Water level trend value > 3 found, which is not allowed");
    }

    if (fMaxUncertainty >= fMinUncertainty && fMinUncertainty < 0)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Negative uncertainty value found (%f), which is not allowed "
                 "(except nodata value -1.0)",
                 fMinUncertainty);
    }

    if (bRet)
    {
        double prevMinHeight = 0;
        double prevMaxHeight = 0;
        if (GH5_FetchAttribute(m_featureGroup, "minDatasetHeight",
                               prevMinHeight) &&
            GH5_FetchAttribute(m_featureGroup, "maxDatasetHeight",
                               prevMaxHeight))
        {
            if (fMinHeight != fNoDataValueHeight)
            {
                prevMinHeight = std::min<double>(prevMinHeight, fMinHeight);
                prevMaxHeight = std::max<double>(prevMaxHeight, fMaxHeight);
                bRet = GH5_WriteAttribute(m_featureGroup, "minDatasetHeight",
                                          prevMinHeight) &&
                       GH5_WriteAttribute(m_featureGroup, "maxDatasetHeight",
                                          prevMaxHeight);
            }
        }
        else
        {
            bRet = WriteFloat32Value(m_featureGroup, "minDatasetHeight",
                                     fMinHeight) &&
                   WriteFloat32Value(m_featureGroup, "maxDatasetHeight",
                                     fMaxHeight);
        }
    }

    return bRet;
}

/************************************************************************/
/*                      S104DatasetDriverUnload()                       */
/************************************************************************/

static void S104DatasetDriverUnload(GDALDriver *)
{
    HDF5UnloadFileDriver();
}

/************************************************************************/
/*                      S104Dataset::CreateCopy()                       */
/************************************************************************/

/* static */
GDALDataset *S104Dataset::CreateCopy(const char *pszFilename,
                                     GDALDataset *poSrcDS, int /* bStrict*/,
                                     CSLConstList papszOptions,
                                     GDALProgressFunc pfnProgress,
                                     void *pProgressData)
{
    S104Creator creator(pszFilename, poSrcDS, papszOptions);
    if (!creator.Create(pfnProgress, pProgressData))
        return nullptr;

    VSIStatBufL sStatBuf;
    if (VSIStatL(pszFilename, &sStatBuf) == 0 &&
        sStatBuf.st_size > 10 * 1024 * 1024)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "%s file size exceeds 10 MB, which is the upper limit "
                 "suggested for wireless transmission to marine vessels",
                 pszFilename);
    }

    GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly);
    return Open(&oOpenInfo);
}

/************************************************************************/
/*                         GDALRegister_S104()                          */
/************************************************************************/
void GDALRegister_S104()

{
    if (!GDAL_CHECK_VERSION("S104"))
        return;

    if (GDALGetDriverByName(S104_DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    S104DriverSetCommonMetadata(poDriver);
    poDriver->pfnOpen = S104Dataset::Open;
    poDriver->pfnCreateCopy = S104Dataset::CreateCopy;
    poDriver->pfnUnloadDriver = S104DatasetDriverUnload;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}

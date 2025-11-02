/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  Read S104 datasets.
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys dot com>
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

#include <limits>
#include <map>

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
                "VERTICAL_CS_MEANING", "depth, meters, orientation down");
        else if (nVerticalCS == 6499)
            poDS->GDALDataset::SetMetadataItem(
                "VERTICAL_CS_MEANING", "height, meters, orientation up");

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
         {"methodWaterLevelProduct", "minDatasetHeight", "maxDatasetHeight"})
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
                            mo.GetMetadataItem(S100_VERTICAL_DATUM_MEANING);
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
                        else
                        {
                            pszValue =
                                mo.GetMetadataItem(S100_VERTICAL_DATUM_NAME);
                            if (pszValue)
                            {
                                verticalDatum = ", vertical datum ";
                                verticalDatum += pszValue;
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
        if (poDataDynamicity->GetDataType().GetClass() == GEDTC_NUMERIC)
        {
            const int nVal = poDataDynamicity->ReadAsInt();
            const std::map<int, const char *> oDataDynamicityMap = {
                {1, "Observation"},
                {2, "Astronomical prediction"},
                {3, "Analysis or hybrid method"},
                {5, "Hydrodynamic model forecast"},
            };
            const auto oIter = oDataDynamicityMap.find(nVal);
            if (oIter != oDataDynamicityMap.end())
                poDS->GDALDataset::SetMetadataItem("DATA_DYNAMICITY_MEANING",
                                                   oIter->second);
        }
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
        if (oComponents.size() != 2 ||
            oComponents[0]->GetName() != "waterLevelHeight" ||
            oComponents[0]->GetType().GetNumericDataType() != GDT_Float32 ||
            oComponents[1]->GetName() != "waterLevelTrend" ||
            (oComponents[1]->GetType().GetNumericDataType() != GDT_Byte &&
             // In theory should be Byte, but 104US00_ches_dcf2_20190606T12Z.h5 uses Int32
             oComponents[1]->GetType().GetNumericDataType() != GDT_Int32))
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
    }

    poDS->GDALDataset::SetMetadataItem(GDALMD_AREA_OR_POINT, GDALMD_AOP_POINT);

    // Setup/check for pam .aux.xml.
    poDS->SetDescription(osFilename.c_str());
    poDS->TryLoadXML();

    // Setup overviews.
    poDS->oOvManager.Initialize(poDS.get(), osFilename.c_str());

    return poDS.release();
}

/************************************************************************/
/*                      S104DatasetDriverUnload()                       */
/************************************************************************/

static void S104DatasetDriverUnload(GDALDriver *)
{
    HDF5UnloadFileDriver();
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
    poDriver->pfnUnloadDriver = S104DatasetDriverUnload;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}

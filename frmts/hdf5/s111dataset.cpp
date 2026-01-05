/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  Read/create S111 datasets.
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
#include <charconv>
#include <cmath>
#include <limits>
#include <map>
#include <variant>

/************************************************************************/
/*                             S111Dataset                              */
/************************************************************************/

class S111Dataset final : public S100BaseDataset
{
  public:
    explicit S111Dataset(const std::string &osFilename)
        : S100BaseDataset(osFilename)
    {
    }

    ~S111Dataset() override;

    static GDALDataset *Open(GDALOpenInfo *);
    static GDALDataset *CreateCopy(const char *pszFilename,
                                   GDALDataset *poSrcDS, int bStrict,
                                   char **papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData);
};

S111Dataset::~S111Dataset() = default;

/************************************************************************/
/*                            S111RasterBand                            */
/************************************************************************/

class S111RasterBand final : public GDALProxyRasterBand
{
    friend class S111Dataset;
    std::unique_ptr<GDALDataset> m_poDS{};
    GDALRasterBand *m_poUnderlyingBand = nullptr;
    std::string m_osUnitType{};
    std::unique_ptr<GDALRasterAttributeTable> m_poRAT{};

    CPL_DISALLOW_COPY_ASSIGN(S111RasterBand)

  public:
    explicit S111RasterBand(std::unique_ptr<GDALDataset> &&poDSIn)
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

    char **GetMetadata(const char *pszDomain) override
    {
        // Short-circuit GDALProxyRasterBand...
        return GDALRasterBand::GetMetadata(pszDomain);
    }
};

GDALRasterBand *
S111RasterBand::RefUnderlyingRasterBand(bool /*bForceOpen*/) const
{
    return m_poUnderlyingBand;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *S111Dataset::Open(GDALOpenInfo *poOpenInfo)

{
    // Confirm that this appears to be a S111 file.
    if (!S111DatasetIdentify(poOpenInfo))
        return nullptr;

    HDF5_GLOBAL_LOCK();

    if (poOpenInfo->nOpenFlags & GDAL_OF_MULTIDIM_RASTER)
    {
        return HDF5Dataset::OpenMultiDim(poOpenInfo);
    }

    // Confirm the requested access is supported.
    if (poOpenInfo->eAccess == GA_Update)
    {
        ReportUpdateNotSupportedByDriver("S111");
        return nullptr;
    }

    std::string osFilename(poOpenInfo->pszFilename);
    std::string osFeatureInstance = "SurfaceCurrent.01";
    std::string osGroup;
    if (STARTS_WITH(poOpenInfo->pszFilename, "S111:"))
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

    auto poDS = std::make_unique<S111Dataset>(osFilename);
    if (!poDS->Init())
        return nullptr;

    const auto &poRootGroup = poDS->m_poRootGroup;

    // Read additional metadata
    for (const char *pszAttrName : {"depthTypeIndex"})
    {
        auto poAttr = poRootGroup->GetAttribute(pszAttrName);
        if (poAttr)
        {
            const char *pszVal = poAttr->ReadAsString();
            if (pszVal)
            {
                poDS->GDALDataset::SetMetadataItem(pszAttrName, pszVal);
            }
        }
    }

    if (const auto poDepthTypeIndex =
            poRootGroup->GetAttribute("depthTypeIndex"))
    {
        if (poDepthTypeIndex->GetDataType().GetClass() == GEDTC_NUMERIC)
        {
            const int nDepthType = poDepthTypeIndex->ReadAsInt();
            if (nDepthType == 1)
            {
                poDS->GDALDataset::SetMetadataItem("DEPTH_TYPE_INDEX_NAME",
                                                   "heightOrDepth");
                poDS->GDALDataset::SetMetadataItem(
                    "DEPTH_TYPE_INDEX_DEFINITION", "Height or depth");
            }
            else if (nDepthType == 2)
            {
                poDS->GDALDataset::SetMetadataItem("DEPTH_TYPE_INDEX_NAME",
                                                   "layerAverage");
                poDS->GDALDataset::SetMetadataItem(
                    "DEPTH_TYPE_INDEX_DEFINITION", "Layer average");
            }
            poDS->GDALDataset::SetMetadataItem(
                "depthTypeIndex", std::to_string(nDepthType).c_str());
        }
    }

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

    auto poSurfaceCurrent = poRootGroup->OpenGroup("SurfaceCurrent");
    if (!poSurfaceCurrent)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find /SurfaceCurrent group");
        return nullptr;
    }

    auto poDataCodingFormat =
        poSurfaceCurrent->GetAttribute("dataCodingFormat");
    if (!poDataCodingFormat ||
        poDataCodingFormat->GetDataType().GetClass() != GEDTC_NUMERIC)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find /SurfaceCurrent/dataCodingFormat attribute");
        return nullptr;
    }
    const int nDataCodingFormat = poDataCodingFormat->ReadAsInt();
    if (nDataCodingFormat != 2)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "dataCodingFormat=%d is not supported by the S111 driver",
                 nDataCodingFormat);
        return nullptr;
    }

    // Read additional metadata
    for (const char *pszAttrName :
         {"methodCurrentsProduct", "minDatasetCurrentSpeed",
          "maxDatasetCurrentSpeed", "commonPointRule", "interpolationType"})
    {
        auto poAttr = poSurfaceCurrent->GetAttribute(pszAttrName);
        if (poAttr)
        {
            const char *pszVal = poAttr->ReadAsString();
            if (pszVal)
            {
                poDS->GDALDataset::SetMetadataItem(pszAttrName, pszVal);
            }
        }
    }

    if (auto poCommonPointRule =
            poSurfaceCurrent->GetAttribute("commonPointRule"))
    {
        poDS->SetMetadataForCommonPointRule(poCommonPointRule.get());
    }

    if (auto poInterpolationType =
            poSurfaceCurrent->GetAttribute("interpolationType"))
    {
        poDS->SetMetadataForInterpolationType(poInterpolationType.get());
    }

    int nNumInstances = 1;
    if (osGroup.empty())
    {
        auto poNumInstances = poSurfaceCurrent->GetAttribute("numInstances");
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
             poSurfaceCurrent->GetGroupNames())
        {
            auto poFeatureInstance =
                poSurfaceCurrent->OpenGroup(featureInstanceName);
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
                            CPLSPrintf("S111:\"%s\":%s:%s", osFilename.c_str(),
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

    auto poFeatureInstance = poSurfaceCurrent->OpenGroup(osFeatureInstance);
    if (!poFeatureInstance)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find /SurfaceCurrent/%s group",
                 osFeatureInstance.c_str());
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

    // Read optional uncertainty array
    if (auto poUncertainty = poFeatureInstance->OpenMDArray("uncertainty"))
    {
        auto &apoDims = poUncertainty->GetDimensions();
        if (poUncertainty->GetDataType().GetClass() == GEDTC_COMPOUND &&
            apoDims.size() == 1 && apoDims[0]->GetSize() == 2)
        {
            const auto &oComponents =
                poUncertainty->GetDataType().GetComponents();
            if (oComponents.size() == 2 &&
                oComponents[0]->GetName() == "name" &&
                oComponents[0]->GetType().GetClass() == GEDTC_STRING &&
                oComponents[1]->GetName() == "value" &&
                (oComponents[1]->GetType().GetNumericDataType() ==
                     GDT_Float32 ||
                 oComponents[1]->GetType().GetNumericDataType() == GDT_Float64))
            {
                auto poName = poUncertainty->GetView("[\"name\"]");
                auto poValue = poUncertainty->GetView("[\"value\"]");
                if (poName && poValue)
                {
                    char *apszStr[2] = {nullptr, nullptr};
                    double adfVals[2] = {0, 0};
                    GUInt64 arrayStartIdx[] = {0};
                    size_t count[] = {2};
                    GInt64 arrayStep[] = {1};
                    GPtrDiff_t bufferStride[] = {1};
                    if (poName->Read(arrayStartIdx, count, arrayStep,
                                     bufferStride, oComponents[0]->GetType(),
                                     apszStr) &&
                        poValue->Read(
                            arrayStartIdx, count, arrayStep, bufferStride,
                            GDALExtendedDataType::Create(GDT_Float64), adfVals))
                    {
                        for (int i = 0; i < 2; ++i)
                        {
                            std::string osName = apszStr[i];
                            if (osName[0] >= 'a' && osName[0] <= 'z')
                                osName[0] = osName[0] - 'a' + 'A';
                            osName = "uncertainty" + osName;
                            poDS->GDALDataset::SetMetadataItem(
                                osName.c_str(), CPLSPrintf("%f", adfVals[i]));
                        }
                    }
                    VSIFree(apszStr[0]);
                    VSIFree(apszStr[1]);
                }
            }
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
                    CPLSPrintf("S111:\"%s\":%s", osFilename.c_str(),
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
                     "Cannot find /SurfaceCurrent/%s/%s group",
                     osFeatureInstance.c_str(), osGroup.c_str());
            return nullptr;
        }

        // Read additional metadata
        for (const char *pszAttrName : {"timePoint"})
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
                     "Cannot find /SurfaceCurrent/%s/%s/values array",
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
        if (!(oComponents.size() >= 2 &&
              ((oComponents[0]->GetName() == "surfaceCurrentSpeed" &&
                oComponents[0]->GetType().GetNumericDataType() == GDT_Float32 &&
                oComponents[1]->GetName() == "surfaceCurrentDirection" &&
                oComponents[1]->GetType().GetNumericDataType() ==
                    GDT_Float32) ||
               // S111US_20170829.0100_W078.N44_F2_loofs_type2.h5 has direction first...
               (oComponents[0]->GetName() == "surfaceCurrentDirection" &&
                oComponents[0]->GetType().GetNumericDataType() == GDT_Float32 &&
                oComponents[1]->GetName() == "surfaceCurrentSpeed" &&
                oComponents[1]->GetType().GetNumericDataType() ==
                    GDT_Float32))))
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

        // Create surfaceCurrentSpeed band
        auto poSurfaceCurrentSpeed =
            poValuesArray->GetView("[\"surfaceCurrentSpeed\"]");
        auto poSurfaceCurrentSpeedDS = std::unique_ptr<GDALDataset>(
            poSurfaceCurrentSpeed->AsClassicDataset(1, 0));
        auto poSurfaceCurrentSpeedBand = std::make_unique<S111RasterBand>(
            std::move(poSurfaceCurrentSpeedDS));
        poSurfaceCurrentSpeedBand->SetDescription("surfaceCurrentSpeed");
        poSurfaceCurrentSpeedBand->m_osUnitType = "knots";

        // From S-111 v1.2 table 9.1 (Speed ranges) and 9.2 (Colour schemas)
        auto poRAT = std::make_unique<GDALDefaultRasterAttributeTable>();
        poRAT->CreateColumn("speed_band", GFT_Integer, GFU_Generic);
        poRAT->CreateColumn("min_speed", GFT_Real, GFU_Min);
        poRAT->CreateColumn("width_band", GFT_Real, GFU_Generic);
        poRAT->CreateColumn("color", GFT_String, GFU_Generic);
        poRAT->CreateColumn("red", GFT_Integer, GFU_RedMin);
        poRAT->CreateColumn("green", GFT_Integer, GFU_GreenMin);
        poRAT->CreateColumn("blue", GFT_Integer, GFU_BlueMin);

        const struct
        {
            int nSpeedBand;
            double dfMinSpeed;
            double dfWidthBand;
            const char *pszColor;
            int nRed;
            int nGreen;
            int nBlue;
        } aoRatValues[] = {
            {1, 0.0, 0.5, "purple", 118, 82, 226},
            {2, 0.5, 0.5, "dark blue", 72, 152, 211},
            {3, 1.0, 1.0, "light blue", 97, 203, 229},
            {4, 2.0, 1.0, "dark green", 109, 188, 69},
            {5, 3.0, 2.0, "light green", 180, 220, 0},
            {6, 5.0, 2.0, "yellow green", 205, 193, 0},
            {7, 7.0, 3.0, "orange", 248, 167, 24},
            {8, 10.0, 3.0, "pink", 247, 162, 157},
            {9, 13.0, 86.0, "red", 255, 30, 30},
        };

        int iRow = 0;
        for (const auto &oRecord : aoRatValues)
        {
            int iCol = 0;
            poRAT->SetValue(iRow, iCol++, oRecord.nSpeedBand);
            poRAT->SetValue(iRow, iCol++, oRecord.dfMinSpeed);
            poRAT->SetValue(iRow, iCol++, oRecord.dfWidthBand);
            poRAT->SetValue(iRow, iCol++, oRecord.pszColor);
            poRAT->SetValue(iRow, iCol++, oRecord.nRed);
            poRAT->SetValue(iRow, iCol++, oRecord.nGreen);
            poRAT->SetValue(iRow, iCol++, oRecord.nBlue);
            ++iRow;
        }

        poSurfaceCurrentSpeedBand->m_poRAT = std::move(poRAT);

        poDS->SetBand(1, poSurfaceCurrentSpeedBand.release());

        // Create surfaceCurrentDirection band
        auto poSurfaceCurrentDirection =
            poValuesArray->GetView("[\"surfaceCurrentDirection\"]");
        auto poSurfaceCurrentDirectionDS = std::unique_ptr<GDALDataset>(
            poSurfaceCurrentDirection->AsClassicDataset(1, 0));
        auto poSurfaceCurrentDirectionBand = std::make_unique<S111RasterBand>(
            std::move(poSurfaceCurrentDirectionDS));
        poSurfaceCurrentDirectionBand->SetDescription(
            "surfaceCurrentDirection");
        poSurfaceCurrentDirectionBand->m_osUnitType = "degree";
        poSurfaceCurrentDirectionBand->GDALRasterBand::SetMetadataItem(
            "ANGLE_CONVENTION", "From true north, clockwise");
        poDS->SetBand(2, poSurfaceCurrentDirectionBand.release());

        for (size_t i = 2; i < oComponents.size(); ++i)
        {
            if (oComponents[i]->GetName() == "speedUncertainty" &&
                oComponents[i]->GetType().GetNumericDataType() == GDT_Float32)
            {
                auto poSubArray =
                    poValuesArray->GetView("[\"speedUncertainty\"]");
                auto poSubArrayDS = std::unique_ptr<GDALDataset>(
                    poSubArray->AsClassicDataset(1, 0));
                auto poSubArrayBand =
                    std::make_unique<S111RasterBand>(std::move(poSubArrayDS));
                poSubArrayBand->SetDescription("speedUncertainty");
                poSubArrayBand->m_osUnitType = "knot";
                poDS->SetBand(poDS->nBands + 1, poSubArrayBand.release());
            }
            else if (oComponents[i]->GetName() == "directionUncertainty" &&
                     oComponents[i]->GetType().GetNumericDataType() ==
                         GDT_Float32)
            {
                auto poSubArray =
                    poValuesArray->GetView("[\"directionUncertainty\"]");
                auto poSubArrayDS = std::unique_ptr<GDALDataset>(
                    poSubArray->AsClassicDataset(1, 0));
                auto poSubArrayBand =
                    std::make_unique<S111RasterBand>(std::move(poSubArrayDS));
                poSubArrayBand->SetDescription("directionUncertainty");
                poSubArrayBand->m_osUnitType = "degree";
                poDS->SetBand(poDS->nBands + 1, poSubArrayBand.release());
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
/*                              S111Creator                             */
/************************************************************************/

class S111Creator final : public S100BaseWriter
{
  public:
    S111Creator(const char *pszDestFilename, GDALDataset *poSrcDS,
                CSLConstList papszOptions)
        : S100BaseWriter(pszDestFilename, poSrcDS, papszOptions)
    {
    }

    ~S111Creator() override;

    bool Create(GDALProgressFunc pfnProgress, void *pProgressData);

    static constexpr const char *FEATURE_TYPE = "SurfaceCurrent";

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
/*                      S111Creator::~S111Creator()                     */
/************************************************************************/

S111Creator::~S111Creator()
{
    S111Creator::Close();
}

/************************************************************************/
/*                         S111Creator::Create()                        */
/************************************************************************/

bool S111Creator::Create(GDALProgressFunc pfnProgress, void *pProgressData)
{
    CPLStringList aosDatasets(
        CSLTokenizeString2(m_aosOptions.FetchNameValue("DATASETS"), ",", 0));
    if (m_poSrcDS->GetRasterCount() == 0 && aosDatasets.empty())
    {
        // Deal with S111 -> S111 translation;
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
                    STARTS_WITH(pszValue, "S111:"))
                {
                    if (strstr(pszValue, ":SurfaceCurrent."))
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
                        S111Creator oAuxCreator(m_osDestFilename.c_str(),
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
                S111Creator oAuxCreator(m_osDestFilename.c_str(), poTmpDS.get(),
                                        aosOptions.List());
                return oAuxCreator.Create(pfnProgress, pProgressData);
            }
        }
    }

    if (m_poSrcDS->GetRasterCount() != 2 && m_poSrcDS->GetRasterCount() != 4)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Source dataset %s must have 2 or 4 bands",
                 m_poSrcDS->GetDescription());
        return false;
    }

    if (!BaseChecks("S111", /* crsMustBeEPSG = */ false,
                    /* verticalDatumRequired = */ false))
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
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Only up to 999 datasets are supported for a same feature "
                 "instance group");
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
            std::unique_ptr<GDALDataset>(S111Dataset::Open(&oOpenInfo));
        if (!poOriDS)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s is not a valid existing S111 dataset",
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
            m_featureGroup.reset(H5_CHECK(H5Gopen(m_hdf5, "SurfaceCurrent")));
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
            const int newNumInstances = static_cast<int>(dfNumInstances) + 1;
            ret = ret && GH5_WriteAttribute(m_featureGroup, "numInstances",
                                            newNumInstances);
            ret = ret && CreateFeatureInstanceGroup(CPLSPrintf(
                             "SurfaceCurrent.%02d", newNumInstances));
            ret = ret && FillFeatureInstanceGroup(oMapTimestampToDS,
                                                  pfnProgress, pProgressData);
        }

        return Close() && ret;
    }
    else
    {
        bool ret = CreateFile();
        ret = ret && WriteProductSpecification("INT.IHO.S-111.2.0");
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

        const char *pszDepthTypeIndex = m_aosOptions.FetchNameValueDef(
            "DEPTH_TYPE", m_poSrcDS->GetMetadataItem("depthTypeIndex"));
        if (!pszDepthTypeIndex)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "DEPTH_TYPE creation option must be specified.");
            return false;
        }

        GH5_HIDTypeHolder hDepthTypeIndexEnumDataType(
            H5_CHECK(H5Tenum_create(H5T_STD_U8LE)));
        ret = ret && hDepthTypeIndexEnumDataType;

        uint8_t val;
        val = 1;
        ret = ret && H5_CHECK(H5Tenum_insert(hDepthTypeIndexEnumDataType,
                                             "heightOrDepth", &val)) >= 0;
        val = 2;
        ret = ret && H5_CHECK(H5Tenum_insert(hDepthTypeIndexEnumDataType,
                                             "layerAverage", &val)) >= 0;

        const int nDepthTypeIndex = EQUAL(pszDepthTypeIndex, "heightOrDepth")
                                        ? 1
                                    : EQUAL(pszDepthTypeIndex, "layerAverage")
                                        ? 2
                                        : atoi(pszDepthTypeIndex);
        if (nDepthTypeIndex != 1 && nDepthTypeIndex != 2)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "DEPTH_TYPE creation option must "
                     "be set to heightOrDepth/1 or layerAverage/2.");
            return false;
        }
        ret = ret &&
              GH5_CreateAttribute(m_hdf5, "depthTypeIndex",
                                  hDepthTypeIndexEnumDataType) &&
              GH5_WriteAttribute(m_hdf5, "depthTypeIndex", nDepthTypeIndex);

        const char *pszVerticalCS = m_aosOptions.FetchNameValueDef(
            "VERTICAL_CS", m_poSrcDS->GetMetadataItem("verticalCS"));
        if (!pszVerticalCS)
        {
            if (nDepthTypeIndex == 1)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "VERTICAL_CS creation option must be specified when "
                         "DEPTH_TYPE = heightOrDepth");
                return false;
            }
        }
        else
        {
            const int nVerticalCS = EQUAL(pszVerticalCS, "DEPTH") ? 6498
                                    : EQUAL(pszVerticalCS, "HEIGHT")
                                        ? 6499
                                        : atoi(pszVerticalCS);
            if (nVerticalCS != 6498 && nVerticalCS != 6499)
            {
                CPLError(
                    CE_Failure, CPLE_NotSupported,
                    "VERTICAL_CS creation option must be set either to 6498 "
                    "(depth/down, metre), or 6499 (height/up, metre)");
                return false;
            }

            ret = ret && WriteVerticalCS(nVerticalCS);
        }

        ret = ret && WriteVerticalCoordinateBase(2);  // verticalDatum

        if (m_nVerticalDatum > 0)
        {
            // 1=s100VerticalDatum, 2=EPSG
            ret = ret && WriteVerticalDatumReference(
                             m_hdf5, m_nVerticalDatum <= 1024 ? 1 : 2);
            ret = ret &&
                  WriteVerticalDatum(m_hdf5, H5T_STD_I32LE, m_nVerticalDatum);
        }
        else if (nDepthTypeIndex == 1)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "VERTICAL_DATUM creation option must be specified when "
                     "DEPTH_TYPE = heightOrDepth");
            return false;
        }

        const char *pszSurfaceCurrentTrendThreshold =
            m_aosOptions.FetchNameValueDef(
                "SURFACE_CURRENT_DEPTH",
                m_poSrcDS->GetMetadataItem("surfaceCurrentDepth"));
        if (!pszSurfaceCurrentTrendThreshold)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "SURFACE_CURRENT_DEPTH creation option must be "
                     "specified.");
            return false;
        }
        if (CPLGetValueType(pszSurfaceCurrentTrendThreshold) ==
            CPL_VALUE_STRING)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "SURFACE_CURRENT_DEPTH creation option value must "
                     "be a numeric value.");
            return false;
        }
        ret =
            ret && WriteFloat32Value(m_hdf5, "surfaceCurrentDepth",
                                     CPLAtof(pszSurfaceCurrentTrendThreshold));

        const char *pszDatasetDeliveryInterval = m_aosOptions.FetchNameValueDef(
            "DATASET_DELIVERY_INTERVAL",
            m_poSrcDS->GetMetadataItem("datasetDeliveryInterval"));
        if (pszDatasetDeliveryInterval)
        {
            ret = ret &&
                  WriteVarLengthStringValue(m_hdf5, "datasetDeliveryInterval",
                                            pszDatasetDeliveryInterval);
        }

        // SurfaceCurrent
        ret = ret && CreateFeatureGroup(FEATURE_TYPE);
        ret = ret && WriteFeatureGroupAttributes();
        ret = ret && WriteAxisNames(m_featureGroup);

        ret = ret && CreateFeatureInstanceGroup("SurfaceCurrent.01");
        ret = ret && FillFeatureInstanceGroup(oMapTimestampToDS, pfnProgress,
                                              pProgressData);

        ret = ret && CreateGroupF();

        return Close() && ret;
    }
}

/************************************************************************/
/*            S111Creator::WriteFeatureGroupAttributes()                */
/************************************************************************/

bool S111Creator::WriteFeatureGroupAttributes()
{
    CPLAssert(m_featureGroup);

    // 3 = high (recommended)
    const char *pszCommonPointRule = m_aosOptions.FetchNameValueDef(
        "COMMON_POINT_RULE", m_poSrcDS->GetMetadataItem("commonPointRule"));
    if (!pszCommonPointRule)
        pszCommonPointRule = "3";  // all (recommended)
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
    const char *pszMethodCurrentsProduct = m_aosOptions.FetchNameValueDef(
        "METHOD_CURRENTS_PRODUCT",
        m_poSrcDS->GetMetadataItem("methodCurrentsProduct"));
    if (pszMethodCurrentsProduct)
        WriteVarLengthStringValue(m_featureGroup, "methodCurrentsProduct",
                                  pszMethodCurrentsProduct);
    ret = ret && WriteInterpolationType(m_featureGroup, 10);  // discrete
    ret = ret && WriteNumInstances(m_featureGroup, H5T_STD_U32LE, 1);
    ret = ret && WriteSequencingRuleScanDirection(m_featureGroup,
                                                  m_poSRS->IsProjected()
                                                      ? "Easting, Northing"
                                                      : "Longitude, Latitude");
    ret = ret && WriteSequencingRuleType(m_featureGroup, 1);  // Linear
    return ret;
}

/************************************************************************/
/*                S111Creator::WriteUncertaintyDataset()                */
/************************************************************************/

bool S111Creator::WriteUncertaintyDataset()
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
    constexpr hsize_t NUM_ROWS = 2;
    hsize_t dims[] = {NUM_ROWS};
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
    GByte abyValues[NUM_ROWS * (sizeof(char *) + sizeof(float))];
    {
        const char *pszName = "surfaceCurrentSpeed";
        const char *pszVal = m_aosOptions.FetchNameValueDef(
            "UNCERTAINTY_SPEED",
            m_poSrcDS->GetMetadataItem("uncertaintySurfaceCurrentSpeed"));
        float fVal = pszVal ? static_cast<float>(CPLAtof(pszVal)) : -1.0f;
        CPL_LSBPTR32(&fVal);
        memcpy(abyValues, &pszName, sizeof(char **));
        memcpy(abyValues + sizeof(char *), &fVal, sizeof(fVal));
    }
    {
        const char *pszName = "surfaceCurrentDirection";
        const char *pszVal = m_aosOptions.FetchNameValueDef(
            "UNCERTAINTY_DIRECTION",
            m_poSrcDS->GetMetadataItem("uncertaintySurfaceCurrentDirection"));
        float fVal = pszVal ? static_cast<float>(CPLAtof(pszVal)) : -1.0f;
        CPL_LSBPTR32(&fVal);
        memcpy(abyValues + sizeof(char *) + sizeof(float), &pszName,
               sizeof(char *));
        memcpy(abyValues + sizeof(char *) + sizeof(float) + sizeof(char *),
               &fVal, sizeof(fVal));
    }

    H5OFFSET_TYPE offset[] = {0};
    bRet = bRet &&
           H5_CHECK(H5Sselect_hyperslab(hFileSpace, H5S_SELECT_SET, offset,
                                        nullptr, dims, nullptr)) >= 0 &&
           H5_CHECK(H5Dwrite(hDatasetID, hDataType, hDataSpace, hFileSpace,
                             H5P_DEFAULT, abyValues)) >= 0;
    return bRet;
}

/************************************************************************/
/*              S111Creator::FillFeatureInstanceGroup()                 */
/************************************************************************/

bool S111Creator::FillFeatureInstanceGroup(
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
        GH5_HIDTypeHolder hDepthTypeIndexEnumDataType(
            H5_CHECK(H5Tenum_create(H5T_STD_U8LE)));
        ret = ret && hDepthTypeIndexEnumDataType;

        uint8_t val;
        val = 1;
        ret = ret && H5_CHECK(H5Tenum_insert(hDepthTypeIndexEnumDataType,
                                             "observation", &val)) >= 0;
        val = 2;
        ret = ret &&
              H5_CHECK(H5Tenum_insert(hDepthTypeIndexEnumDataType,
                                      "astronomicalPrediction", &val)) >= 0;
        val = 3;
        ret = ret && H5_CHECK(H5Tenum_insert(hDepthTypeIndexEnumDataType,
                                             "analysisOrHybrid", &val)) >= 0;
        val = 5;
        ret =
            ret && H5_CHECK(H5Tenum_insert(hDepthTypeIndexEnumDataType,
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
                                  hDepthTypeIndexEnumDataType) &&
              GH5_WriteAttribute(m_featureInstanceGroup, "dataDynamicity",
                                 nDataDynamicity);
    }

    if (m_poSrcDS->GetRasterCount() == 2 ||
        m_aosOptions.FetchNameValue("UNCERTAINTY_SPEED") ||
        m_aosOptions.FetchNameValue("UNCERTAINTY_DIRECTION"))
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
/*                      S111Creator::CreateGroupF()                     */
/************************************************************************/

// Per S-111 v2.0 spec

constexpr float string_to_float(std::string_view str)
{
    if (str.empty())
        return 0.0f;

    size_t i = 0;
    bool neg = false;
    if (str[0] == '-')
    {
        neg = true;
        ++i;
    }
    else if (str[0] == '+')
    {
        ++i;
    }

    float int_part = 0.0f;
    for (; i < str.size() && str[i] >= '0' && str[i] <= '9'; ++i)
    {
        int_part = int_part * 10.0f + (str[i] - '0');
    }

    float frac_part = 0.0f;
    float divisor = 1.0f;
    if (i < str.size() && str[i] == '.')
    {
        for (++i; i < str.size() && str[i] >= '0' && str[i] <= '9'; ++i)
        {
            frac_part = frac_part * 10.0f + (str[i] - '0');
            divisor *= 10.0f;
        }
    }

    float result = int_part + frac_part / divisor;
    return neg ? -result : result;
}

constexpr const char *MIN_SPEED_STR = "0.00";
constexpr float MIN_SPEED = string_to_float(MIN_SPEED_STR);
constexpr const char *MAX_SPEED_STR = "99.00";
constexpr float MAX_SPEED = string_to_float(MAX_SPEED_STR);
constexpr const char *NODATA_SPEED_STR = "-9999.00";
constexpr float NODATA_SPEED = string_to_float(NODATA_SPEED_STR);

constexpr const char *MIN_DIR_STR = "0.0";
constexpr float MIN_DIR = string_to_float(MIN_DIR_STR);
constexpr const char *MAX_DIR_STR = "359.9";
constexpr float MAX_DIR = string_to_float(MAX_DIR_STR);
constexpr const char *NODATA_DIR_STR = "-9999.0";
constexpr float NODATA_DIR = string_to_float(NODATA_DIR_STR);

constexpr const char *NODATA_UNCT_STR = "-1.0";
constexpr float NODATA_UNCT = string_to_float(NODATA_UNCT_STR);

bool S111Creator::CreateGroupF()
{
    bool ret = S100BaseWriter::CreateGroupF();

    CPLStringList aosFeatureCodes;
    aosFeatureCodes.push_back(FEATURE_TYPE);
    ret = ret && WriteOneDimensionalVarLengthStringArray(
                     m_GroupF, "featureCode", aosFeatureCodes.List());

    {
        std::vector<std::array<const char *, GROUP_F_DATASET_FIELD_COUNT>> rows{
            {"surfaceCurrentSpeed", "Surface Current Speed", "knot",
             NODATA_SPEED_STR, "H5T_FLOAT", MIN_SPEED_STR, MAX_SPEED_STR,
             "geSemiInterval"},
            {"surfaceCurrentDirection", "Surface Current Direction", "degree",
             "-9999.0", "H5T_FLOAT", MIN_DIR_STR, MAX_DIR_STR,
             "closedInterval"},
            {"speedUncertainty", "Speed Uncertainty", "knot", NODATA_UNCT_STR,
             "H5T_FLOAT", MIN_SPEED_STR, MAX_SPEED_STR, "geSemiInterval"},
            {"directionUncertainty", "Direction Uncertainty", "degree",
             NODATA_UNCT_STR, "H5T_FLOAT", MIN_DIR_STR, MAX_DIR_STR,
             "closedInterval"},
        };
        rows.resize(m_poSrcDS->GetRasterCount());
        ret = ret && WriteGroupFDataset(FEATURE_TYPE, rows);
    }

    return ret;
}

/************************************************************************/
/*                       S111Creator::CopyValues()                      */
/************************************************************************/

bool S111Creator::CopyValues(GDALDataset *poSrcDS, GDALProgressFunc pfnProgress,
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
    const int nComponents = poSrcDS->GetRasterCount();
    CPLAssert(nComponents == 2 || nComponents == 4);

    GH5_HIDTypeHolder hDataType(
        H5_CHECK(H5Tcreate(H5T_COMPOUND, sizeof(float) * nComponents)));
    bRet = bRet && hDataType &&
           H5_CHECK(H5Tinsert(hDataType, "surfaceCurrentSpeed", 0,
                              H5T_IEEE_F32LE)) >= 0 &&
           H5_CHECK(H5Tinsert(hDataType, "surfaceCurrentDirection",
                              sizeof(float), H5T_IEEE_F32LE)) >= 0;
    if (nComponents == 4 && bRet)
    {
        bRet = H5_CHECK(H5Tinsert(hDataType, "speedUncertainty",
                                  2 * sizeof(float), H5T_IEEE_F32LE)) >= 0 &&
               H5_CHECK(H5Tinsert(hDataType, "directionUncertainty",
                                  3 * sizeof(float), H5T_IEEE_F32LE)) >= 0;
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
    const bool bReverseY = m_gt[5] < 0;

    constexpr std::array<float, 4> afNoDataValue{NODATA_SPEED, NODATA_DIR,
                                                 NODATA_UNCT, NODATA_UNCT};

    constexpr float INF = std::numeric_limits<float>::infinity();
    std::array<float, 4> afMin{INF, INF, INF, INF};
    std::array<float, 4> afMax{-INF, -INF, -INF, -INF};

    std::array<int, 4> abHasNoDataBand{FALSE, FALSE, FALSE, FALSE};
    std::array<float, 4> afSrcNoData{0, 0, 0, 0};
    for (int i = 0; i < nComponents; ++i)
    {
        afSrcNoData[i] = static_cast<float>(
            poSrcDS->GetRasterBand(i + 1)->GetNoDataValue(&abHasNoDataBand[i]));
    }

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
                for (int i = 0; i < nReqCountY * nReqCountX; i++)
                {
                    for (int iC = 0; iC < nComponents; ++iC)
                    {
                        float fVal = afValues[i * nComponents + iC];
                        if ((abHasNoDataBand[iC] && fVal == afSrcNoData[iC]) ||
                            std::isnan(fVal))
                        {
                            afValues[i * nComponents + iC] = afNoDataValue[iC];
                        }
                        else
                        {
                            afMin[iC] = std::min(afMin[iC], fVal);
                            afMax[iC] = std::max(afMax[iC], fVal);
                        }
                        CPL_LSBPTR32(&afValues[i * nComponents + iC]);
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
                                  H5P_DEFAULT, afValues.data())) >= 0 &&
                pfnProgress((static_cast<double>(iY) * nXBlocks + iX + 1) /
                                (static_cast<double>(nXBlocks) * nYBlocks),
                            "", pProgressData) != 0;
        }
    }

    constexpr int IDX_SPEED = 0;
    constexpr int IDX_DIR = 1;
    constexpr int IDX_UNC_SPEED = 2;
    constexpr int IDX_UNC_DIR = 3;

    if (afMin[IDX_SPEED] > afMax[IDX_SPEED])
    {
        afMin[IDX_SPEED] = afMax[IDX_SPEED] = NODATA_SPEED;
    }
    else if (!(afMin[IDX_SPEED] >= MIN_SPEED && afMax[IDX_SPEED] <= MAX_SPEED))
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Range of surface current speed in the dataset is [%f, %f] "
                 "whereas the "
                 "allowed range is [%.2f, %.2f]",
                 afMin[IDX_SPEED], afMax[IDX_SPEED], MIN_SPEED, MAX_SPEED);
    }

    if (afMin[IDX_DIR] > afMax[IDX_DIR])
    {
        afMin[IDX_DIR] = afMax[IDX_DIR] = NODATA_DIR;
    }
    else if (!(afMin[IDX_DIR] >= MIN_DIR && afMax[IDX_DIR] <= MAX_DIR))
    {
        CPLError(
            CE_Warning, CPLE_AppDefined,
            "Range of surface current direction in the dataset is [%f, %f] "
            "whereas the "
            "allowed range is [%.2f, %.2f]",
            afMin[IDX_DIR], afMax[IDX_DIR], MIN_DIR, MAX_DIR);
    }

    if (afMax[IDX_UNC_SPEED] >= afMin[IDX_UNC_SPEED] &&
        afMin[IDX_UNC_SPEED] < 0)
    {
        CPLError(
            CE_Warning, CPLE_AppDefined,
            "Negative speed uncertainty value found (%f), which is not allowed "
            "(except nodata value %s)",
            afMin[IDX_UNC_SPEED], NODATA_UNCT_STR);
    }

    if (afMax[IDX_UNC_DIR] >= afMin[IDX_UNC_DIR] && afMin[IDX_UNC_DIR] < 0)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Negative direction uncertainty value found (%f), which is "
                 "not allowed (except nodata value %s)",
                 afMin[IDX_UNC_DIR], NODATA_UNCT_STR);
    }

    if (bRet)
    {
        double prevMinSpeed = 0;
        double prevMaxSpeed = 0;
        if (GH5_FetchAttribute(m_featureGroup, "minDatasetCurrentSpeed",
                               prevMinSpeed) &&
            GH5_FetchAttribute(m_featureGroup, "maxDatasetCurrentSpeed",
                               prevMaxSpeed))
        {
            if (afMin[IDX_SPEED] != NODATA_SPEED)
            {
                prevMinSpeed = std::min<double>(prevMinSpeed, afMin[IDX_SPEED]);
                prevMaxSpeed = std::max<double>(prevMaxSpeed, afMax[IDX_SPEED]);
                bRet =
                    GH5_WriteAttribute(m_featureGroup, "minDatasetCurrentSpeed",
                                       prevMinSpeed) &&
                    GH5_WriteAttribute(m_featureGroup, "maxDatasetCurrentSpeed",
                                       prevMaxSpeed);
            }
        }
        else
        {
            bRet = WriteFloat64Value(m_featureGroup, "minDatasetCurrentSpeed",
                                     afMin[IDX_SPEED]) &&
                   WriteFloat64Value(m_featureGroup, "maxDatasetCurrentSpeed",
                                     afMax[IDX_SPEED]);
        }
    }

    return bRet;
}

/************************************************************************/
/*                      S111Dataset::CreateCopy()                       */
/************************************************************************/

/* static */
GDALDataset *S111Dataset::CreateCopy(const char *pszFilename,
                                     GDALDataset *poSrcDS, int /* bStrict*/,
                                     char **papszOptions,
                                     GDALProgressFunc pfnProgress,
                                     void *pProgressData)
{
    S111Creator creator(pszFilename, poSrcDS, papszOptions);
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
/*                      S111DatasetDriverUnload()                       */
/************************************************************************/

static void S111DatasetDriverUnload(GDALDriver *)
{
    HDF5UnloadFileDriver();
}

/************************************************************************/
/*                         GDALRegister_S111()                          */
/************************************************************************/
void GDALRegister_S111()

{
    if (!GDAL_CHECK_VERSION("S111"))
        return;

    if (GDALGetDriverByName(S111_DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    S111DriverSetCommonMetadata(poDriver);
    poDriver->pfnOpen = S111Dataset::Open;
    poDriver->pfnCreateCopy = S111Dataset::CreateCopy;
    poDriver->pfnUnloadDriver = S111DatasetDriverUnload;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}

/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  Read S102 bathymetric datasets.
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2023-2025, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "cpl_vsi.h"

#include "hdf5dataset.h"
#include "hdf5drivercore.h"
#include "gh5_convenience.h"
#include "rat.h"
#include "s100.h"

#include "gdal_frmts.h"
#include "gdal_priv.h"
#include "gdal_proxy.h"
#include "gdal_rat.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string_view>

/************************************************************************/
/*                             S102Dataset                              */
/************************************************************************/

class S102Dataset final : public S100BaseDataset
{
    bool OpenQuality(GDALOpenInfo *poOpenInfo,
                     const std::shared_ptr<GDALGroup> &poRootGroup);

  public:
    explicit S102Dataset(const std::string &osFilename)
        : S100BaseDataset(osFilename)
    {
    }

    ~S102Dataset() override;

    static GDALDataset *Open(GDALOpenInfo *);
    static GDALDataset *CreateCopy(const char *pszFilename,
                                   GDALDataset *poSrcDS, int bStrict,
                                   CSLConstList papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData);
};

S102Dataset::~S102Dataset() = default;

/************************************************************************/
/*                            S102RasterBand                            */
/************************************************************************/

class S102RasterBand final : public GDALProxyRasterBand
{
    friend class S102Dataset;
    std::unique_ptr<GDALDataset> m_poDS{};
    GDALRasterBand *m_poUnderlyingBand = nullptr;
    double m_dfMinimum = std::numeric_limits<double>::quiet_NaN();
    double m_dfMaximum = std::numeric_limits<double>::quiet_NaN();

    CPL_DISALLOW_COPY_ASSIGN(S102RasterBand)

  public:
    explicit S102RasterBand(std::unique_ptr<GDALDataset> &&poDSIn)
        : m_poDS(std::move(poDSIn)),
          m_poUnderlyingBand(m_poDS->GetRasterBand(1))
    {
        eDataType = m_poUnderlyingBand->GetRasterDataType();
        m_poUnderlyingBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
    }

    GDALRasterBand *
    RefUnderlyingRasterBand(bool /*bForceOpen*/ = true) const override;

    double GetMinimum(int *pbSuccess = nullptr) override
    {
        if (pbSuccess)
            *pbSuccess = !std::isnan(m_dfMinimum);
        return m_dfMinimum;
    }

    double GetMaximum(int *pbSuccess = nullptr) override
    {
        if (pbSuccess)
            *pbSuccess = !std::isnan(m_dfMaximum);
        return m_dfMaximum;
    }

    const char *GetUnitType() override
    {
        return "metre";
    }
};

GDALRasterBand *
S102RasterBand::RefUnderlyingRasterBand(bool /*bForceOpen*/) const
{
    return m_poUnderlyingBand;
}

/************************************************************************/
/*                 S102GeoreferencedMetadataRasterBand                  */
/************************************************************************/

class S102GeoreferencedMetadataRasterBand final : public GDALProxyRasterBand
{
    friend class S102Dataset;

    std::unique_ptr<GDALDataset> m_poDS{};
    GDALRasterBand *m_poUnderlyingBand = nullptr;
    std::unique_ptr<GDALRasterAttributeTable> m_poRAT{};

    CPL_DISALLOW_COPY_ASSIGN(S102GeoreferencedMetadataRasterBand)

  public:
    explicit S102GeoreferencedMetadataRasterBand(
        std::unique_ptr<GDALDataset> &&poDSIn,
        std::unique_ptr<GDALRasterAttributeTable> &&poRAT)
        : m_poDS(std::move(poDSIn)),
          m_poUnderlyingBand(m_poDS->GetRasterBand(1)),
          m_poRAT(std::move(poRAT))
    {
        eDataType = m_poUnderlyingBand->GetRasterDataType();
        m_poUnderlyingBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
    }

    GDALRasterBand *
    RefUnderlyingRasterBand(bool /*bForceOpen*/ = true) const override;

    GDALRasterAttributeTable *GetDefaultRAT() override
    {
        return m_poRAT.get();
    }
};

GDALRasterBand *S102GeoreferencedMetadataRasterBand::RefUnderlyingRasterBand(
    bool /*bForceOpen*/) const
{
    return m_poUnderlyingBand;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *S102Dataset::Open(GDALOpenInfo *poOpenInfo)

{
    // Confirm that this appears to be a S102 file.
    if (!S102DatasetIdentify(poOpenInfo))
        return nullptr;

    HDF5_GLOBAL_LOCK();

    if (poOpenInfo->nOpenFlags & GDAL_OF_MULTIDIM_RASTER)
    {
        return HDF5Dataset::OpenMultiDim(poOpenInfo);
    }

    // Confirm the requested access is supported.
    if (poOpenInfo->eAccess == GA_Update)
    {
        ReportUpdateNotSupportedByDriver("S102");
        return nullptr;
    }

    std::string osFilename(poOpenInfo->pszFilename);
    bool bIsSubdataset = false;
    bool bIsQuality = false;
    std::string osBathymetryCoverageName = "BathymetryCoverage.01";
    if (STARTS_WITH(poOpenInfo->pszFilename, "S102:"))
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
            bIsSubdataset = true;
            osFilename = aosTokens[1];
            if (EQUAL(aosTokens[2], "BathymetryCoverage"))
            {
                // Default dataset
            }
            else if (STARTS_WITH(aosTokens[2], "BathymetryCoverage"))
            {
                osBathymetryCoverageName = aosTokens[2];
            }
            else if (EQUAL(aosTokens[2], "QualityOfSurvey") ||  // < v3
                     EQUAL(aosTokens[2], "QualityOfBathymetryCoverage"))  // v3
            {
                bIsQuality = true;
            }
            else
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Unsupported subdataset component: '%s'. Expected "
                         "'QualityOfSurvey'",
                         aosTokens[2]);
                return nullptr;
            }
        }
        else
        {
            return nullptr;
        }
    }

    auto poDS = std::make_unique<S102Dataset>(osFilename);
    if (!poDS->Init())
        return nullptr;

    const auto &poRootGroup = poDS->m_poRootGroup;

    auto poBathymetryCoverage = poRootGroup->OpenGroup("BathymetryCoverage");
    if (!poBathymetryCoverage)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "S102: Cannot find /BathymetryCoverage group");
        return nullptr;
    }

    if (!bIsSubdataset)
    {
        auto poNumInstances =
            poBathymetryCoverage->GetAttribute("numInstances");
        if (poNumInstances &&
            poNumInstances->GetDataType().GetClass() == GEDTC_NUMERIC)
        {
            const int nNumInstances = poNumInstances->ReadAsInt();
            if (nNumInstances != 1)
            {
                CPLStringList aosSubDSList;
                int iSubDS = 0;
                for (const std::string &coverageName :
                     poBathymetryCoverage->GetGroupNames())
                {
                    auto poCoverage =
                        poBathymetryCoverage->OpenGroup(coverageName);
                    if (poCoverage)
                    {
                        GDALMajorObject mo;
                        // Read first vertical datum from root group and let the
                        // coverage override it.
                        S100ReadVerticalDatum(&mo, poRootGroup.get());
                        S100ReadVerticalDatum(&mo, poCoverage.get());
                        ++iSubDS;
                        aosSubDSList.SetNameValue(
                            CPLSPrintf("SUBDATASET_%d_NAME", iSubDS),
                            CPLSPrintf("S102:\"%s\":%s", osFilename.c_str(),
                                       coverageName.c_str()));
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
                        aosSubDSList.SetNameValue(
                            CPLSPrintf("SUBDATASET_%d_DESC", iSubDS),
                            CPLSPrintf(
                                "Bathymetric gridded data, instance %s%s",
                                coverageName.c_str(), verticalDatum.c_str()));
                    }
                }
                auto poGroupQuality =
                    poRootGroup->OpenGroup("QualityOfBathymetryCoverage");
                if (poGroupQuality)
                {
                    auto poQualityOfBathymetryCoverage01 =
                        poGroupQuality->OpenGroup(
                            "QualityOfBathymetryCoverage.01");
                    if (poQualityOfBathymetryCoverage01)
                    {
                        ++iSubDS;
                        aosSubDSList.SetNameValue(
                            CPLSPrintf("SUBDATASET_%d_NAME", iSubDS),
                            CPLSPrintf(
                                "S102:\"%s\":QualityOfBathymetryCoverage",
                                osFilename.c_str()));
                        aosSubDSList.SetNameValue(
                            CPLSPrintf("SUBDATASET_%d_DESC", iSubDS),
                            "Georeferenced metadata "
                            "QualityOfBathymetryCoverage");
                    }
                }

                poDS->GDALDataset::SetMetadata(aosSubDSList.List(),
                                               "SUBDATASETS");

                // Setup/check for pam .aux.xml.
                poDS->SetDescription(osFilename.c_str());
                poDS->TryLoadXML();

                // Setup overviews.
                poDS->oOvManager.Initialize(poDS.get(), osFilename.c_str());

                return poDS.release();
            }
        }
    }

    if (bIsQuality)
    {
        if (!poDS->OpenQuality(poOpenInfo, poRootGroup))
            return nullptr;

        // Setup/check for pam .aux.xml.
        poDS->SetDescription(osFilename.c_str());
        poDS->TryLoadXML();

        // Setup overviews.
        poDS->oOvManager.Initialize(poDS.get(), osFilename.c_str());

        return poDS.release();
    }

    auto poBathymetryCoverageInstance =
        poBathymetryCoverage->OpenGroup(osBathymetryCoverageName);
    if (!poBathymetryCoverageInstance)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "S102: Cannot find %s group in BathymetryCoverage group",
                 osBathymetryCoverageName.c_str());
        return nullptr;
    }

    if (auto poStartSequence =
            poBathymetryCoverage->GetAttribute("startSequence"))
    {
        const char *pszStartSequence = poStartSequence->ReadAsString();
        if (pszStartSequence && !EQUAL(pszStartSequence, "0,0"))
        {
            // Shouldn't happen given this is imposed by the spec.
            // Cf 4.2.1.1.1.12 "startSequence" of Ed 3.0 spec, page 13
            CPLError(CE_Failure, CPLE_AppDefined,
                     "startSequence (=%s) != 0,0 is not supported",
                     pszStartSequence);
            return nullptr;
        }
    }

    // Potentially override vertical datum
    S100ReadVerticalDatum(poDS.get(), poBathymetryCoverageInstance.get());

    const bool bNorthUp = CPLTestBool(
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "NORTH_UP", "YES"));

    // Compute geotransform
    poDS->m_bHasGT = S100GetGeoTransform(poBathymetryCoverageInstance.get(),
                                         poDS->m_gt, bNorthUp);

    auto poGroup001 = poBathymetryCoverageInstance->OpenGroup("Group_001");
    if (!poGroup001)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "S102: Cannot find "
                 "/BathymetryCoverage/BathymetryCoverage.01/Group_001");
        return nullptr;
    }
    auto poValuesArray = poGroup001->OpenMDArray("values");
    if (!poValuesArray || poValuesArray->GetDimensionCount() != 2)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "S102: Cannot find "
                 "/BathymetryCoverage/BathymetryCoverage.01/Group_001/values");
        return nullptr;
    }
    const auto &oType = poValuesArray->GetDataType();
    if (oType.GetClass() != GEDTC_COMPOUND)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "S102: Wrong type for "
                 "/BathymetryCoverage/BathymetryCoverage.01/Group_001/values");
        return nullptr;
    }
    const auto &oComponents = oType.GetComponents();
    if (oComponents.size() == 0 || oComponents[0]->GetName() != "depth")
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "S102: Wrong type for "
                 "/BathymetryCoverage/BathymetryCoverage.01/Group_001/values");
        return nullptr;
    }

    if (bNorthUp)
        poValuesArray = poValuesArray->GetView("[::-1,...]");

    auto poDepth = poValuesArray->GetView("[\"depth\"]");

    // Mandatory in v2.2. Since v3.0, EPSG:6498 is the only allowed value
    bool bCSIsElevation = false;
    auto poVerticalCS = poRootGroup->GetAttribute("verticalCS");
    if (poVerticalCS && poVerticalCS->GetDataType().GetClass() == GEDTC_NUMERIC)
    {
        const auto nVal = poVerticalCS->ReadAsInt();
        if (nVal == 6498)  // Depth metre
        {
            // nothing to do
        }
        else if (nVal == 6499)  // Height metre
        {
            bCSIsElevation = true;
        }
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported, "Unsupported verticalCS=%d",
                     nVal);
        }
    }

    const bool bUseElevation =
        EQUAL(CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                                   "DEPTH_OR_ELEVATION", "DEPTH"),
              "ELEVATION");
    const bool bInvertDepth = (bUseElevation && !bCSIsElevation) ||
                              (!bUseElevation && bCSIsElevation);
    const double dfDepthNoData = poDepth->GetNoDataValueAsDouble();
    auto poDepthDS = [&poDepth, bInvertDepth, dfDepthNoData]()
    {
        if (bInvertDepth)
        {
            auto poInverted = poDepth->GetUnscaled(-1, 0, dfDepthNoData);
            return std::unique_ptr<GDALDataset>(
                poInverted->AsClassicDataset(1, 0));
        }
        else
        {
            return std::unique_ptr<GDALDataset>(
                poDepth->AsClassicDataset(1, 0));
        }
    }();

    poDS->nRasterXSize = poDepthDS->GetRasterXSize();
    poDS->nRasterYSize = poDepthDS->GetRasterYSize();

    // Create depth (or elevation) band
    auto poDepthBand = new S102RasterBand(std::move(poDepthDS));
    poDepthBand->SetDescription(bUseElevation ? "elevation" : "depth");

    auto poMinimumDepth = poGroup001->GetAttribute("minimumDepth");
    if (poMinimumDepth &&
        poMinimumDepth->GetDataType().GetClass() == GEDTC_NUMERIC)
    {
        const double dfVal = poMinimumDepth->ReadAsDouble();
        if (dfVal != dfDepthNoData)
        {
            if (bInvertDepth)
                poDepthBand->m_dfMaximum = -dfVal;
            else
                poDepthBand->m_dfMinimum = dfVal;
        }
    }

    auto poMaximumDepth = poGroup001->GetAttribute("maximumDepth");
    if (poMaximumDepth &&
        poMaximumDepth->GetDataType().GetClass() == GEDTC_NUMERIC)
    {
        const double dfVal = poMaximumDepth->ReadAsDouble();
        if (dfVal != dfDepthNoData)
        {
            if (bInvertDepth)
                poDepthBand->m_dfMinimum = -dfVal;
            else
                poDepthBand->m_dfMaximum = dfVal;
        }
    }

    poDS->SetBand(1, poDepthBand);

    const bool bHasUncertainty =
        oComponents.size() >= 2 && oComponents[1]->GetName() == "uncertainty";
    if (bHasUncertainty)
    {
        // Create uncertainty band
        auto poUncertainty = poValuesArray->GetView("[\"uncertainty\"]");
        const double dfUncertaintyNoData =
            poUncertainty->GetNoDataValueAsDouble();
        auto poUncertaintyDS =
            std::unique_ptr<GDALDataset>(poUncertainty->AsClassicDataset(1, 0));

        auto poUncertaintyBand = new S102RasterBand(std::move(poUncertaintyDS));
        poUncertaintyBand->SetDescription("uncertainty");

        auto poMinimumUncertainty =
            poGroup001->GetAttribute("minimumUncertainty");
        if (poMinimumUncertainty &&
            poMinimumUncertainty->GetDataType().GetClass() == GEDTC_NUMERIC)
        {
            const double dfVal = poMinimumUncertainty->ReadAsDouble();
            if (dfVal != dfUncertaintyNoData)
            {
                poUncertaintyBand->m_dfMinimum = dfVal;
            }
        }

        auto poMaximumUncertainty =
            poGroup001->GetAttribute("maximumUncertainty");
        if (poMaximumUncertainty &&
            poMaximumUncertainty->GetDataType().GetClass() == GEDTC_NUMERIC)
        {
            const double dfVal = poMaximumUncertainty->ReadAsDouble();
            if (dfVal != dfUncertaintyNoData)
            {
                poUncertaintyBand->m_dfMaximum = dfVal;
            }
        }

        poDS->SetBand(2, poUncertaintyBand);
    }

    poDS->GDALDataset::SetMetadataItem(GDALMD_AREA_OR_POINT, GDALMD_AOP_POINT);

    auto poGroupQuality = poRootGroup->OpenGroup("QualityOfSurvey");
    const bool bIsNamedQualityOfSurvey = poGroupQuality != nullptr;
    if (!bIsNamedQualityOfSurvey)
    {
        // S102 v3 now uses QualityOfBathymetryCoverage instead of QualityOfSurvey
        poGroupQuality = poRootGroup->OpenGroup("QualityOfBathymetryCoverage");
    }
    if (!bIsSubdataset && poGroupQuality)
    {
        const char *pszNameOfQualityGroup = bIsNamedQualityOfSurvey
                                                ? "QualityOfSurvey"
                                                : "QualityOfBathymetryCoverage";
        auto poGroupQuality01 = poGroupQuality->OpenGroup(
            CPLSPrintf("%s.01", pszNameOfQualityGroup));
        if (poGroupQuality01)
        {
            poDS->GDALDataset::SetMetadataItem(
                "SUBDATASET_1_NAME",
                CPLSPrintf("S102:\"%s\":BathymetryCoverage",
                           osFilename.c_str()),
                "SUBDATASETS");
            poDS->GDALDataset::SetMetadataItem(
                "SUBDATASET_1_DESC", "Bathymetric gridded data", "SUBDATASETS");

            poDS->GDALDataset::SetMetadataItem(
                "SUBDATASET_2_NAME",
                CPLSPrintf("S102:\"%s\":%s", osFilename.c_str(),
                           pszNameOfQualityGroup),
                "SUBDATASETS");
            poDS->GDALDataset::SetMetadataItem(
                "SUBDATASET_2_DESC",
                CPLSPrintf("Georeferenced metadata %s", pszNameOfQualityGroup),
                "SUBDATASETS");
        }
    }

    // Setup/check for pam .aux.xml.
    poDS->SetDescription(osFilename.c_str());
    poDS->TryLoadXML();

    // Setup overviews.
    poDS->oOvManager.Initialize(poDS.get(), osFilename.c_str());

    return poDS.release();
}

/************************************************************************/
/*                            OpenQuality()                             */
/************************************************************************/

bool S102Dataset::OpenQuality(GDALOpenInfo *poOpenInfo,
                              const std::shared_ptr<GDALGroup> &poRootGroup)
{
    const bool bNorthUp = CPLTestBool(
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "NORTH_UP", "YES"));

    const char *pszNameOfQualityGroup = "QualityOfSurvey";
    auto poGroupQuality = poRootGroup->OpenGroup(pszNameOfQualityGroup);
    if (!poGroupQuality)
    {
        pszNameOfQualityGroup = "QualityOfBathymetryCoverage";
        poGroupQuality = poRootGroup->OpenGroup(pszNameOfQualityGroup);
        if (!poGroupQuality)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find group /QualityOfSurvey or "
                     "/QualityOfBathymetryCoverage");
            return false;
        }
    }

    const std::string osQuality01Name =
        std::string(pszNameOfQualityGroup).append(".01");
    const std::string osQuality01FullName = std::string("/")
                                                .append(pszNameOfQualityGroup)
                                                .append("/")
                                                .append(osQuality01Name);
    auto poGroupQuality01 = poGroupQuality->OpenGroup(osQuality01Name);
    if (!poGroupQuality01)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find group %s",
                 osQuality01FullName.c_str());
        return false;
    }

    if (auto poStartSequence = poGroupQuality01->GetAttribute("startSequence"))
    {
        const char *pszStartSequence = poStartSequence->ReadAsString();
        if (pszStartSequence && !EQUAL(pszStartSequence, "0,0"))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "startSequence (=%s) != 0,0 is not supported",
                     pszStartSequence);
            return false;
        }
    }

    // Compute geotransform
    m_bHasGT = S100GetGeoTransform(poGroupQuality01.get(), m_gt, bNorthUp);

    auto poGroup001 = poGroupQuality01->OpenGroup("Group_001");
    if (!poGroup001)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find group %s/Group_001",
                 osQuality01FullName.c_str());
        return false;
    }

    auto poValuesArray = poGroup001->OpenMDArray("values");
    if (!poValuesArray)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find array "
                 "%s/Group_001/values",
                 osQuality01FullName.c_str());
        return false;
    }

    {
        const auto &oType = poValuesArray->GetDataType();
        if (oType.GetClass() == GEDTC_NUMERIC &&
            oType.GetNumericDataType() == GDT_UInt32)
        {
            // ok
        }
        else if (oType.GetClass() == GEDTC_COMPOUND &&
                 oType.GetComponents().size() == 1 &&
                 oType.GetComponents()[0]->GetType().GetClass() ==
                     GEDTC_NUMERIC &&
                 oType.GetComponents()[0]->GetType().GetNumericDataType() ==
                     GDT_UInt32)
        {
            // seen in a S102 v3 product (102DE00CA22_UNC_MD.H5), although
            // I believe this is non-conformant.

            // Escape potentials single-quote and double-quote with back-slash
            CPLString osEscapedCompName(oType.GetComponents()[0]->GetName());
            osEscapedCompName.replaceAll("\\", "\\\\")
                .replaceAll("'", "\\'")
                .replaceAll("\"", "\\\"");

            // Gets a view with that single component extracted.
            poValuesArray = poValuesArray->GetView(
                std::string("['").append(osEscapedCompName).append("']"));
            if (!poValuesArray)
                return false;
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported data type for %s",
                     poValuesArray->GetFullName().c_str());
            return false;
        }
    }

    if (poValuesArray->GetDimensionCount() != 2)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported number of dimensions for %s",
                 poValuesArray->GetFullName().c_str());
        return false;
    }

    auto poFeatureAttributeTable =
        poGroupQuality->OpenMDArray("featureAttributeTable");
    if (!poFeatureAttributeTable)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find array /%s/featureAttributeTable",
                 pszNameOfQualityGroup);
        return false;
    }

    {
        const auto &oType = poFeatureAttributeTable->GetDataType();
        if (oType.GetClass() != GEDTC_COMPOUND)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported data type for %s",
                     poFeatureAttributeTable->GetFullName().c_str());
            return false;
        }

        const auto &poComponents = oType.GetComponents();
        if (poComponents.size() >= 1 && poComponents[0]->GetName() != "id")
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Missing 'id' component in %s",
                     poFeatureAttributeTable->GetFullName().c_str());
            return false;
        }
    }

    if (bNorthUp)
        poValuesArray = poValuesArray->GetView("[::-1,...]");

    auto poDS =
        std::unique_ptr<GDALDataset>(poValuesArray->AsClassicDataset(1, 0));
    if (!poDS)
        return false;

    nRasterXSize = poDS->GetRasterXSize();
    nRasterYSize = poDS->GetRasterYSize();

    auto poRAT =
        HDF5CreateRAT(poFeatureAttributeTable, /* bFirstColIsMinMax = */ true);
    auto poBand = std::make_unique<S102GeoreferencedMetadataRasterBand>(
        std::move(poDS), std::move(poRAT));
    SetBand(1, poBand.release());

    return true;
}

/************************************************************************/
/*                             S102Creator                              */
/************************************************************************/

class S102Creator final : public S100BaseWriter
{
  public:
    S102Creator(const char *pszDestFilename, GDALDataset *poSrcDS,
                CSLConstList papszOptions)
        : S100BaseWriter(pszDestFilename, poSrcDS, papszOptions)
    {
    }

    ~S102Creator() override;

    bool Create(GDALProgressFunc pfnProgress, void *pProgressData);

    // From the S102 spec
    static constexpr float NODATA = 1000000.0f;
    static constexpr const char *FEATURE_TYPE = "BathymetryCoverage";
    static constexpr const char *QUALITY_FEATURE_TYPE =
        "QualityOfBathymetryCoverage";

  protected:
    bool Close() override
    {
        return BaseClose();
    }

  private:
    bool WriteFeatureGroupAttributes(bool isQuality);
    bool CopyValues(GDALProgressFunc pfnProgress, void *pProgressData);
    bool CopyQualityValues(GDALDataset *poQualityDS,
                           const std::set<int> &oSetRATId,
                           GDALProgressFunc pfnProgress, void *pProgressData);
    bool WriteFeatureAttributeTable(const GDALRasterAttributeTable *poRAT);
    bool CreateGroupF(bool hasQualityOfBathymetryCoverage);
};

/************************************************************************/
/*                     S102Creator::~S102Creator()                      */
/************************************************************************/

S102Creator::~S102Creator()
{
    S102Creator::Close();
}

/************************************************************************/
/*                        S102Creator::Create()                         */
/************************************************************************/

// S102 v3.0 Table 10-8 - Elements of featureAttributeTable compound datatype
static const struct
{
    const char *pszName;
    const char *pszType;
} gasFeatureAttributeTableMembers[] = {
    {"id", "uint32"},
    {"dataAssessment", "uint8"},
    {"featuresDetected.leastDepthOfDetectedFeaturesMeasured", "boolean"},
    {"featuresDetected.significantFeaturesDetected", "boolean"},
    {"featuresDetected.sizeOfFeaturesDetected", "float32"},
    {"featureSizeVar", "float32"},
    {"fullSeafloorCoverageAchieved", "boolean"},
    {"bathyCoverage", "boolean"},
    {"zoneOfConfidence.horizontalPositionUncertainty.uncertaintyFixed",
     "float32"},
    {"zoneOfConfidence.horizontalPositionUncertainty.uncertaintyVariableFactor",
     "float32"},
    {"surveyDateRange.dateStart", "date"},
    {"surveyDateRange.dateEnd", "date"},
    {"sourceSurveyID", "string"},
    {"surveyAuthority", "string"},
    {"typeOfBathymetricEstimationUncertainty", "enumeration"},
};

bool S102Creator::Create(GDALProgressFunc pfnProgress, void *pProgressData)
{
    if (m_poSrcDS->GetRasterCount() != 1 && m_poSrcDS->GetRasterCount() != 2)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Source dataset must have one or two bands");
        return false;
    }

    if (!BaseChecks("S102", /* crsMustBeEPSG = */ true,
                    /* verticalDatumRequired = */ true))
        return false;

    const bool bAppendSubdataset =
        CPLTestBool(m_aosOptions.FetchNameValueDef("APPEND_SUBDATASET", "NO"));

    std::unique_ptr<GDALDataset> poQualityDS;
    const char *pszQualityDataset =
        m_aosOptions.FetchNameValue("QUALITY_DATASET");
    const GDALRasterAttributeTable *poRAT = nullptr;
    if (!pszQualityDataset && !bAppendSubdataset)
    {
        const char *pszSubDSName =
            m_poSrcDS->GetMetadataItem("SUBDATASET_2_NAME", "SUBDATASETS");
        if (pszSubDSName &&
            cpl::starts_with(std::string_view(pszSubDSName), "S102:") &&
            cpl::ends_with(std::string_view(pszSubDSName),
                           ":QualityOfBathymetryCoverage"))
        {
            pszQualityDataset = pszSubDSName;
        }
    }

    std::set<int> oSetRATId;
    if (pszQualityDataset)
    {
        if (bAppendSubdataset)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Quality dataset can only be set on initial creation");
            return false;
        }
        poQualityDS.reset(GDALDataset::Open(
            pszQualityDataset, GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR, nullptr,
            nullptr, nullptr));
        if (!poQualityDS)
            return false;

        if (poQualityDS->GetRasterCount() != 1)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s does not have a single band.", pszQualityDataset);
            return false;
        }
        if (!GDALDataTypeIsInteger(
                poQualityDS->GetRasterBand(1)->GetRasterDataType()))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s band is not of an integer data type.",
                     pszQualityDataset);
            return false;
        }
        if (poQualityDS->GetRasterXSize() != m_poSrcDS->GetRasterXSize() ||
            poQualityDS->GetRasterYSize() != m_poSrcDS->GetRasterYSize())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s does not have the same dimensions as %s.",
                     pszQualityDataset, m_poSrcDS->GetDescription());
            return false;
        }

        const auto poQualityDS_SRS = poQualityDS->GetSpatialRef();
        if (!poQualityDS_SRS || !poQualityDS_SRS->IsSame(m_poSRS))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s does not have the same CRS as %s.", pszQualityDataset,
                     m_poSrcDS->GetDescription());
            return false;
        }

        GDALGeoTransform gt;
        if (poQualityDS->GetGeoTransform(gt) != CE_None || gt != m_gt)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s does not have the same geotransform as %s.",
                     pszQualityDataset, m_poSrcDS->GetDescription());
            return false;
        }

        poRAT = poQualityDS->GetRasterBand(1)->GetDefaultRAT();
        if (!poRAT)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s does not have a raster attribute table.",
                     poQualityDS->GetDescription());
            return false;
        }

        const int nRATColumnCount = poRAT->GetColumnCount();
        std::set<std::string_view> setKnownColumnNames;
        for (const auto &entry : gasFeatureAttributeTableMembers)
            setKnownColumnNames.insert(entry.pszName);
        int iRATIdField = -1;
        for (int i = 0; i < nRATColumnCount; ++i)
        {
            const char *pszColName = poRAT->GetNameOfCol(i);
            if (strcmp(pszColName, "id") == 0)
            {
                iRATIdField = i;
            }
            else if (!cpl::contains(setKnownColumnNames, pszColName))
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "'%s' is not a valid S102 feature attribute table "
                         "column name.",
                         pszColName);
            }
        }
        if (iRATIdField < 0)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Input raster attribute table lacks an integer 'id' field");
            return false;
        }
        const int nRATRowCount = poRAT->GetRowCount();
        for (int i = 0; i < nRATRowCount; ++i)
        {
            const int nID = poRAT->GetValueAsInt(i, iRATIdField);
            if (nID == 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "id=0 is not allowed in input raster attribute table");
                return false;
            }
            else if (nID < 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Negative id is not allowed in input raster attribute "
                         "table");
                return false;
            }
            else if (cpl::contains(oSetRATId, nID))
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Several rows of input raster attribute table have id=%d",
                    nID);
                return false;
            }
            oSetRATId.insert(nID);
        }
    }

    if (!((m_nVerticalDatum >= 1 && m_nVerticalDatum <= 30) ||
          m_nVerticalDatum == 44))
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "VERTICAL_DATUM=%d value is a valid S100 value but not "
                 "allowed in S102. Valid values are [1, 30] or 44",
                 m_nVerticalDatum);
    }

    if (!(m_nEPSGCode == 4326 || m_nEPSGCode == 5041 || m_nEPSGCode == 5042 ||
          (m_nEPSGCode >= 32601 && m_nEPSGCode <= 32660) ||
          (m_nEPSGCode >= 32701 && m_nEPSGCode <= 32760)))
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "The EPSG code of the CRS is %d. "
                 "Only EPSG codes 4326, 5041, 5042, [32601, 32660], "
                 "[32701, 32760] are officially supported. "
                 "The dataset may not be recognized by other software",
                 m_nEPSGCode);
    }

    if (bAppendSubdataset)
    {
        GDALOpenInfo oOpenInfo(m_osDestFilename.c_str(), GA_ReadOnly);
        auto poOriDS =
            std::unique_ptr<GDALDataset>(S102Dataset::Open(&oOpenInfo));
        if (!poOriDS)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s is not a valid existing S102 dataset",
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
            m_featureGroup.reset(
                H5_CHECK(H5Gopen(m_hdf5, "BathymetryCoverage")));
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
            ret = ret && CreateFeatureInstanceGroup(CPLSPrintf(
                             "BathymetryCoverage.%02d", newNumInstances));
            ret = ret && WriteFIGGridRelatedParameters(m_featureInstanceGroup);
            if (dfMainVerticalDatum != m_nVerticalDatum)
            {
                ret = ret &&
                      GH5_CreateAttribute(m_featureInstanceGroup,
                                          "verticalDatumReference",
                                          H5T_STD_U8LE) &&
                      // s100VerticalDatum
                      GH5_WriteAttribute(m_featureInstanceGroup,
                                         "verticalDatumReference", 1);
                ret =
                    ret && WriteVerticalDatum(m_featureInstanceGroup,
                                              H5T_STD_U16LE, m_nVerticalDatum);
            }

            ret = ret && WriteNumGRP(m_featureInstanceGroup, H5T_STD_U8LE, 1);

            ret = ret && CreateValuesGroup("Group_001");
            ret = ret && WriteVarLengthStringValue(m_valuesGroup, "timePoint",
                                                   "00010101T000000Z");
            ret = ret && CopyValues(pfnProgress, pProgressData);
        }

        // Update global bounding box
        OGREnvelope sExistingExtent;
        ret = ret && GH5_FetchAttribute(m_hdf5, "westBoundLongitude",
                                        sExistingExtent.MinX, true);
        ret = ret && GH5_FetchAttribute(m_hdf5, "southBoundLatitude",
                                        sExistingExtent.MinY, true);
        ret = ret && GH5_FetchAttribute(m_hdf5, "eastBoundLongitude",
                                        sExistingExtent.MaxX, true);
        ret = ret && GH5_FetchAttribute(m_hdf5, "northBoundLatitude",
                                        sExistingExtent.MaxY, true);

        sExtent.Merge(sExistingExtent);
        ret = ret &&
              GH5_WriteAttribute(m_hdf5, "westBoundLongitude", sExtent.MinX);
        ret = ret &&
              GH5_WriteAttribute(m_hdf5, "southBoundLatitude", sExtent.MinY);
        ret = ret &&
              GH5_WriteAttribute(m_hdf5, "eastBoundLongitude", sExtent.MaxX);
        ret = ret &&
              GH5_WriteAttribute(m_hdf5, "northBoundLatitude", sExtent.MaxY);

        return Close() && ret;
    }
    else
    {
        bool ret = CreateFile();
        ret = ret && WriteProductSpecification("INT.IHO.S-102.3.0.0");
        ret = ret && WriteIssueDate();
        ret = ret && WriteIssueTime(/* bAutogenerateFromCurrent = */ false);
        ret = ret && WriteHorizontalCRS();
        ret = ret && WriteTopLevelBoundingBox();
        ret = ret && WriteVerticalCS(6498);           // Depth, metre, down
        ret = ret && WriteVerticalCoordinateBase(2);  // verticalDatum
        // s100VerticalDatum
        ret = ret && WriteVerticalDatumReference(m_hdf5, 1);
        ret =
            ret && WriteVerticalDatum(m_hdf5, H5T_STD_U16LE, m_nVerticalDatum);

        // BathymetryCoverage
        ret = ret && CreateFeatureGroup(FEATURE_TYPE);
        ret = ret && WriteFeatureGroupAttributes(/* isQuality = */ false);
        ret = ret && WriteAxisNames(m_featureGroup);

        ret = ret && CreateFeatureInstanceGroup("BathymetryCoverage.01");
        ret = ret && WriteFIGGridRelatedParameters(m_featureInstanceGroup);
        ret = ret && WriteNumGRP(m_featureInstanceGroup, H5T_STD_U8LE, 1);

        ret = ret && CreateValuesGroup("Group_001");

        ret = ret && WriteVarLengthStringValue(m_valuesGroup, "timePoint",
                                               "00010101T000000Z");

        const double dfIntermediatePct =
            m_poSrcDS->GetRasterCount() /
            (m_poSrcDS->GetRasterCount() + (poQualityDS ? 1.0 : 0.0));
        std::unique_ptr<void, decltype(&GDALDestroyScaledProgress)>
            pScaledProgressData(GDALCreateScaledProgress(0.0, dfIntermediatePct,
                                                         pfnProgress,
                                                         pProgressData),
                                GDALDestroyScaledProgress);
        ret = ret && CopyValues(GDALScaledProgress, pScaledProgressData.get());

        if (poQualityDS)
        {
            // QualityOfBathymetryCoverage group
            ret = ret && CreateFeatureGroup(QUALITY_FEATURE_TYPE);
            ret = ret && WriteFeatureGroupAttributes(/* isQuality = */ true);
            ret = ret && WriteAxisNames(m_featureGroup);
            ret = ret && WriteFeatureAttributeTable(poRAT);

            ret = ret &&
                  CreateFeatureInstanceGroup("QualityOfBathymetryCoverage.01");
            ret = ret && WriteFIGGridRelatedParameters(m_featureInstanceGroup);
            ret = ret && WriteNumGRP(m_featureInstanceGroup, H5T_STD_U8LE, 1);

            ret = ret && CreateValuesGroup("Group_001");
            pScaledProgressData.reset(GDALCreateScaledProgress(
                dfIntermediatePct, 1.0, pfnProgress, pProgressData));
            ret = ret && CopyQualityValues(poQualityDS.get(), oSetRATId,
                                           GDALScaledProgress,
                                           pScaledProgressData.get());
        }

        ret = ret && CreateGroupF(poQualityDS != nullptr);

        return Close() && ret;
    }
}

/************************************************************************/
/*              S102Creator::WriteFeatureGroupAttributes()              */
/************************************************************************/

bool S102Creator::WriteFeatureGroupAttributes(bool isQuality)
{
    CPLAssert(m_featureGroup);

    bool ret = WriteCommonPointRule(m_featureGroup, 2);  // low
    if (isQuality)
    {
        // Feature oriented Regular Grid
        ret = ret && WriteDataCodingFormat(m_featureGroup, 9);
    }
    else
    {
        ret = ret && WriteDataCodingFormat(m_featureGroup, 2);  // Regular grid
    }
    ret = ret && WriteDataOffsetCode(m_featureGroup, 5);  // Center of cell
    ret = ret && WriteDimension(m_featureGroup, 2);
    const char *pszHorizontalPositionUncertainty =
        m_aosOptions.FetchNameValue("HORIZONTAL_POSITION_UNCERTAINTY");
    ret =
        ret &&
        WriteHorizontalPositionUncertainty(
            m_featureGroup,
            pszHorizontalPositionUncertainty &&
                    pszHorizontalPositionUncertainty[0]
                ? static_cast<float>(CPLAtof(pszHorizontalPositionUncertainty))
                : -1.0f);
    const char *pszVerticalUncertainty =
        m_aosOptions.FetchNameValue("VERTICAL_UNCERTAINTY");
    ret = ret && WriteVerticalUncertainty(
                     m_featureGroup,
                     pszVerticalUncertainty && pszVerticalUncertainty[0]
                         ? static_cast<float>(CPLAtof(pszVerticalUncertainty))
                         : -1.0f);
    ret = ret && WriteInterpolationType(m_featureGroup, 1);  // Nearest neighbor
    ret = ret && WriteNumInstances(m_featureGroup, H5T_STD_U8LE, 1);
    ret = ret && WriteSequencingRuleScanDirection(m_featureGroup,
                                                  m_poSRS->IsProjected()
                                                      ? "Easting, Northing"
                                                      : "Longitude, Latitude");
    ret = ret && WriteSequencingRuleType(m_featureGroup, 1);  // Linear
    return ret;
}

/************************************************************************/
/*              S102Creator::WriteFeatureAttributeTable()               */
/************************************************************************/

bool S102Creator::WriteFeatureAttributeTable(
    const GDALRasterAttributeTable *poRAT)
{
    CPLAssert(m_featureGroup);

    std::map<std::string_view, const char *> mapKnownColumns;
    for (const auto &entry : gasFeatureAttributeTableMembers)
        mapKnownColumns[entry.pszName] = entry.pszType;

    const int nColCount = poRAT->GetColumnCount();

    size_t nCompoundSize = 0;
    size_t nMEMCompoundSize = 0;
    for (int i = 0; i < nColCount; ++i)
    {
        const char *pszColName = poRAT->GetNameOfCol(i);
        const auto iter = mapKnownColumns.find(pszColName);
        size_t nMemberSize = sizeof(char *);
        if (iter != mapKnownColumns.end())
        {
            const char *pszType = iter->second;
            if (strcmp(pszType, "uint8") == 0 ||
                strcmp(pszType, "boolean") == 0 ||
                strcmp(pszType, "enumeration") == 0)
            {
                nMemberSize = sizeof(uint8_t);
            }
            else if (strcmp(pszType, "uint32") == 0)
            {
                nMemberSize = sizeof(uint32_t);
            }
            else if (strcmp(pszType, "float32") == 0)
            {
                nMemberSize = sizeof(float);
            }
            else if (strcmp(pszType, "string") == 0 ||
                     strcmp(pszType, "date") == 0)
            {
                nMemberSize = sizeof(char *);
            }
            else
            {
                CPLAssert(false);
            }
        }
        else
        {
            GDALRATFieldType eType = poRAT->GetTypeOfCol(i);
            switch (eType)
            {
                case GFT_Integer:
                    nMemberSize = sizeof(int32_t);
                    break;
                case GFT_Real:
                    nMemberSize = sizeof(double);
                    break;
                case GFT_Boolean:
                    nMemberSize = sizeof(uint8_t);
                    break;
                case GFT_String:
                case GFT_DateTime:
                case GFT_WKBGeometry:
                    nMemberSize = sizeof(char *);
                    break;
            }
        }
        nCompoundSize += nMemberSize;
        if ((nMEMCompoundSize % nMemberSize) != 0)
            nMEMCompoundSize += nMemberSize - (nMEMCompoundSize % nMemberSize);
        nMEMCompoundSize += nMemberSize;
    }

    GH5_HIDTypeHolder hDataType(
        H5_CHECK(H5Tcreate(H5T_COMPOUND, nCompoundSize)));
    GH5_HIDTypeHolder hDataTypeMEM(
        H5_CHECK(H5Tcreate(H5T_COMPOUND, nMEMCompoundSize)));
    GH5_HIDTypeHolder hVarLengthType(H5_CHECK(H5Tcopy(H5T_C_S1)));
    bool bRet = hDataType && hDataTypeMEM && hVarLengthType &&
                H5_CHECK(H5Tset_size(hVarLengthType, H5T_VARIABLE)) >= 0 &&
                H5_CHECK(H5Tset_strpad(hVarLengthType, H5T_STR_NULLTERM)) >= 0;

    GH5_HIDTypeHolder hEnumType;
    std::vector<const char *> apszTypes;

    size_t nOffset = 0;
    size_t nMEMOffset = 0;
    std::vector<size_t> anMEMOffsets;
    for (int i = 0; i < nColCount && bRet; ++i)
    {
        const char *pszColName = poRAT->GetNameOfCol(i);
        const auto iter = mapKnownColumns.find(pszColName);
        hid_t hMemberType = hVarLengthType.get();
        hid_t hMemberNativeType = hVarLengthType.get();
        if (iter != mapKnownColumns.end())
        {
            const char *pszType = iter->second;
            if (strcmp(pszType, "uint8") == 0 ||
                strcmp(pszType, "boolean") == 0)
            {
                hMemberType = H5T_STD_U8LE;
                hMemberNativeType = H5T_NATIVE_UCHAR;
            }
            else if (strcmp(pszType, "uint32") == 0)
            {
                hMemberType = H5T_STD_U32LE;
                hMemberNativeType = H5T_NATIVE_UINT;
            }
            else if (strcmp(pszType, "float32") == 0)
            {
                hMemberType = H5T_IEEE_F32LE;
                hMemberNativeType = H5T_NATIVE_FLOAT;
            }
            else if (strcmp(pszType, "string") == 0 ||
                     strcmp(pszType, "date") == 0)
            {
                hMemberType = hVarLengthType.get();
                hMemberNativeType = hVarLengthType.get();
            }
            else if (strcmp(pszType, "enumeration") == 0 &&
                     strcmp(pszColName,
                            "typeOfBathymetricEstimationUncertainty") == 0)
            {
                hEnumType.reset(H5_CHECK(H5Tenum_create(H5T_STD_U8LE)));
                bRet = hEnumType;
                if (bRet)
                {
                    uint8_t val;
                    val = 1;
                    bRet = bRet &&
                           H5_CHECK(H5Tenum_insert(
                               hEnumType, "rawStandardDeviation", &val)) >= 0;
                    val = 2;
                    bRet = bRet &&
                           H5_CHECK(H5Tenum_insert(
                               hEnumType, "cUBEStandardDeviation", &val)) >= 0;
                    val = 3;
                    bRet = bRet &&
                           H5_CHECK(H5Tenum_insert(
                               hEnumType, "productUncertainty", &val)) >= 0;
                    val = 4;
                    bRet = bRet && H5_CHECK(H5Tenum_insert(
                                       hEnumType, "historicalStandardDeviation",
                                       &val)) >= 0;

                    hMemberType = hEnumType.get();
                    hMemberNativeType = hEnumType.get();
                }
            }
            else
            {
                CPLAssert(false);
            }
            apszTypes.push_back(pszType);
        }
        else
        {
            GDALRATFieldType eType = poRAT->GetTypeOfCol(i);
            switch (eType)
            {
                case GFT_Integer:
                    hMemberType = H5T_STD_I32LE;
                    hMemberNativeType = H5T_NATIVE_INT;
                    apszTypes.push_back("int32");
                    break;
                case GFT_Real:
                    hMemberType = H5T_IEEE_F64LE;
                    hMemberNativeType = H5T_NATIVE_DOUBLE;
                    apszTypes.push_back("float64");
                    break;
                case GFT_Boolean:
                    hMemberType = H5T_STD_U8LE;
                    hMemberNativeType = H5T_NATIVE_UCHAR;
                    apszTypes.push_back("boolean");
                    break;
                case GFT_String:
                case GFT_DateTime:
                case GFT_WKBGeometry:
                    apszTypes.push_back("string");
                    break;
            }
        }

        CPLAssert(H5Tget_size(hMemberType) == H5Tget_size(hMemberNativeType));

        bRet = bRet && H5_CHECK(H5Tinsert(hDataType, pszColName, nOffset,
                                          hMemberType)) >= 0;

        const size_t nMemberSize = H5Tget_size(hMemberType);
        if ((nMEMOffset % nMemberSize) != 0)
            nMEMOffset += nMemberSize - (nMEMOffset % nMemberSize);
        anMEMOffsets.push_back(nMEMOffset);
        bRet = bRet && H5_CHECK(H5Tinsert(hDataTypeMEM, pszColName, nMEMOffset,
                                          hMemberNativeType)) >= 0;
        nOffset += nMemberSize;
        nMEMOffset += nMemberSize;
    }
    CPLAssert(nOffset == nCompoundSize);
    CPLAssert(nMEMOffset == nMEMCompoundSize);

    CPLAssert(apszTypes.size() == static_cast<size_t>(nColCount));

    const int nRowCount = poRAT->GetRowCount();
    hsize_t dims[] = {static_cast<hsize_t>(nRowCount)};
    GH5_HIDSpaceHolder hDataSpace(H5_CHECK(H5Screate_simple(1, dims, nullptr)));
    bRet = bRet && hDataSpace;
    GH5_HIDDatasetHolder hDatasetID;
    GH5_HIDSpaceHolder hFileSpace;
    GH5_HIDParametersHolder hParams(H5_CHECK(H5Pcreate(H5P_DATASET_CREATE)));
    bRet = bRet && hParams;
    if (bRet)
    {
        bRet = H5_CHECK(H5Pset_layout(hParams, H5D_CHUNKED)) >= 0;
        hsize_t chunk_size[] = {static_cast<hsize_t>(1)};
        bRet = bRet && H5_CHECK(H5Pset_chunk(hParams, 1, chunk_size)) >= 0;
        hDatasetID.reset(
            H5_CHECK(H5Dcreate(m_featureGroup, "featureAttributeTable",
                               hDataType, hDataSpace, hParams)));
        bRet = bRet && hDatasetID;
    }
    if (bRet)
    {
        hFileSpace.reset(H5_CHECK(H5Dget_space(hDatasetID)));
        bRet = hFileSpace;
    }

    hsize_t count[] = {1};
    GH5_HIDSpaceHolder hMemSpace(H5_CHECK(H5Screate_simple(1, count, nullptr)));
    bRet = bRet && hMemSpace;

    std::vector<GByte> abyBuffer(nMEMCompoundSize);
    std::vector<std::string> asBuffers(nColCount);
    for (int iRow = 0; iRow < nRowCount && bRet; ++iRow)
    {
        for (int iCol = 0; iCol < nColCount && bRet; ++iCol)
        {
            const char *const pszType = apszTypes[iCol];
            GByte *const pabyDst = abyBuffer.data() + anMEMOffsets[iCol];
            if (strcmp(pszType, "uint8") == 0 ||
                strcmp(pszType, "boolean") == 0 ||
                strcmp(pszType, "enumeration") == 0)
            {
                const uint8_t nVal =
                    static_cast<uint8_t>(poRAT->GetValueAsInt(iRow, iCol));
                *pabyDst = nVal;
            }
            else if (strcmp(pszType, "int32") == 0 ||
                     strcmp(pszType, "uint32") == 0)
            {
                const int nVal = poRAT->GetValueAsInt(iRow, iCol);
                memcpy(pabyDst, &nVal, sizeof(nVal));
            }
            else if (strcmp(pszType, "float32") == 0)
            {
                const float fVal =
                    static_cast<float>(poRAT->GetValueAsDouble(iRow, iCol));
                memcpy(pabyDst, &fVal, sizeof(fVal));
            }
            else if (strcmp(pszType, "float64") == 0)
            {
                const double dfVal = poRAT->GetValueAsDouble(iRow, iCol);
                memcpy(pabyDst, &dfVal, sizeof(dfVal));
            }
            else if (strcmp(pszType, "string") == 0)
            {
                asBuffers[iCol] = poRAT->GetValueAsString(iRow, iCol);
                const char *pszStr = asBuffers[iCol].c_str();
                memcpy(pabyDst, &pszStr, sizeof(pszStr));
            }
            else if (strcmp(pszType, "date") == 0)
            {
                asBuffers[iCol] = poRAT->GetValueAsString(iRow, iCol);
                if (asBuffers[iCol].size() != 8)
                {
                    OGRField sField;
                    if (OGRParseDate(asBuffers[iCol].c_str(), &sField, 0))
                    {
                        asBuffers[iCol] = CPLString().Printf(
                            "%04d%02d%02d", sField.Date.Year, sField.Date.Month,
                            sField.Date.Day);
                    }
                }
                const char *pszStr = asBuffers[iCol].c_str();
                memcpy(pabyDst, &pszStr, sizeof(pszStr));
            }
            else
            {
                CPLAssert(false);
            }
        }

        H5OFFSET_TYPE offset[] = {static_cast<H5OFFSET_TYPE>(iRow)};
        bRet =
            bRet &&
            H5_CHECK(H5Sselect_hyperslab(hFileSpace, H5S_SELECT_SET, offset,
                                         nullptr, count, nullptr)) >= 0 &&
            H5_CHECK(H5Dwrite(hDatasetID, hDataTypeMEM, hMemSpace, hFileSpace,
                              H5P_DEFAULT, abyBuffer.data())) >= 0;
    }

    return bRet;
}

/************************************************************************/
/*                     S102Creator::CreateGroupF()                      */
/************************************************************************/

// Per S-102 v3.0 spec
#define MIN_DEPTH_VALUE -14
#define MAX_DEPTH_VALUE 11050

#define STRINGIFY(x) #x
#define XSTRINGIFY(x) STRINGIFY(x)

bool S102Creator::CreateGroupF(bool hasQualityOfBathymetryCoverage)
{
    bool ret = S100BaseWriter::CreateGroupF();

    CPLStringList aosFeatureCodes;
    aosFeatureCodes.push_back(FEATURE_TYPE);
    if (hasQualityOfBathymetryCoverage)
        aosFeatureCodes.push_back(QUALITY_FEATURE_TYPE);
    ret = ret && WriteOneDimensionalVarLengthStringArray(
                     m_GroupF, "featureCode", aosFeatureCodes.List());

    {
        std::vector<std::array<const char *, GROUP_F_DATASET_FIELD_COUNT>> rows{
            {"depth", "depth", "metres", "1000000", "H5T_FLOAT",
             XSTRINGIFY(MIN_DEPTH_VALUE), XSTRINGIFY(MAX_DEPTH_VALUE),
             "closedInterval"},
            {"uncertainty", "uncertainty", "metres", "1000000", "H5T_FLOAT",
             "0", "", "geSemiInterval"}};
        rows.resize(m_poSrcDS->GetRasterCount());
        ret = ret && WriteGroupFDataset(FEATURE_TYPE, rows);
    }
    {
        std::vector<std::array<const char *, GROUP_F_DATASET_FIELD_COUNT>> rows{
            {"iD", "ID", "", "0", "H5T_INTEGER", "1", "", "geSemiInterval"}};
        ret = ret && WriteGroupFDataset(QUALITY_FEATURE_TYPE, rows);
    }

    return ret;
}

/************************************************************************/
/*                      S102Creator::CopyValues()                       */
/************************************************************************/

bool S102Creator::CopyValues(GDALProgressFunc pfnProgress, void *pProgressData)
{
    CPLAssert(m_valuesGroup.get() >= 0);

    const int nYSize = m_poSrcDS->GetRasterYSize();
    const int nXSize = m_poSrcDS->GetRasterXSize();

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
    const float fNoDataValue = NODATA;
    const int nComponents = m_poSrcDS->GetRasterCount();

    GH5_HIDTypeHolder hDataType(
        H5_CHECK(H5Tcreate(H5T_COMPOUND, nComponents * sizeof(float))));
    bRet = bRet && hDataType &&
           H5_CHECK(H5Tinsert(hDataType, "depth", 0, H5T_IEEE_F32LE)) >= 0 &&
           (nComponents == 1 ||
            H5_CHECK(H5Tinsert(hDataType, "uncertainty", sizeof(float),
                               H5T_IEEE_F32LE)) >= 0);

    hsize_t chunk_size[] = {static_cast<hsize_t>(nBlockYSize),
                            static_cast<hsize_t>(nBlockXSize)};

    const float afFillValue[] = {fNoDataValue, fNoDataValue};
    GH5_HIDParametersHolder hParams(H5_CHECK(H5Pcreate(H5P_DATASET_CREATE)));
    bRet = bRet && hParams &&
           H5_CHECK(H5Pset_fill_time(hParams, H5D_FILL_TIME_ALLOC)) >= 0 &&
           H5_CHECK(H5Pset_fill_value(hParams, hDataType, afFillValue)) >= 0 &&
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
    const bool bReverseY = m_gt.yscale < 0;

    float fMinDepth = std::numeric_limits<float>::infinity();
    float fMaxDepth = -std::numeric_limits<float>::infinity();
    float fMinUncertainty = std::numeric_limits<float>::infinity();
    float fMaxUncertainty = -std::numeric_limits<float>::infinity();

    int bHasNoDataBand1 = FALSE;
    const char *pszFirstBandDesc =
        m_poSrcDS->GetRasterBand(1)->GetDescription();
    const float fMulFactor =
        EQUAL(pszFirstBandDesc, "elevation") ? -1.0f : 1.0f;
    if (fMulFactor < 0.0f)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Automatically convert from elevation to depth by negating "
                 "elevation values");
    }
    const double dfSrcNoDataBand1 =
        m_poSrcDS->GetRasterBand(1)->GetNoDataValue(&bHasNoDataBand1);
    const float fSrcNoDataBand1 = static_cast<float>(dfSrcNoDataBand1);
    int bHasNoDataBand2 = FALSE;
    const double dfSrcNoDataBand2 =
        nComponents == 2
            ? m_poSrcDS->GetRasterBand(2)->GetNoDataValue(&bHasNoDataBand2)
            : 0.0;
    const float fSrcNoDataBand2 = static_cast<float>(dfSrcNoDataBand2);

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
                m_poSrcDS->RasterIO(
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
                    {
                        float fVal = afValues[i * nComponents];
                        if ((bHasNoDataBand1 && fVal == fSrcNoDataBand1) ||
                            std::isnan(fVal))
                        {
                            afValues[i * nComponents] = fNoDataValue;
                        }
                        else
                        {
                            fVal *= fMulFactor;
                            afValues[i * nComponents] = fVal;
                            fMinDepth = std::min(fMinDepth, fVal);
                            fMaxDepth = std::max(fMaxDepth, fVal);
                        }
                    }
                    if (nComponents == 2)
                    {
                        const float fVal = afValues[i * nComponents + 1];
                        if ((bHasNoDataBand2 && fVal == fSrcNoDataBand2) ||
                            std::isnan(fVal))
                        {
                            afValues[i * nComponents + 1] = fNoDataValue;
                        }
                        else
                        {
                            fMinUncertainty = std::min(fMinUncertainty, fVal);
                            fMaxUncertainty = std::max(fMaxUncertainty, fVal);
                        }
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

    if (fMinDepth > fMaxDepth)
    {
        fMinDepth = fMaxDepth = fNoDataValue;
    }
    else if (!(fMinDepth >= MIN_DEPTH_VALUE && fMaxDepth <= MAX_DEPTH_VALUE))
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Range of depth in the dataset is [%f, %f] whereas the "
                 "allowed range is [%d, %d]",
                 fMinDepth, fMaxDepth, MIN_DEPTH_VALUE, MAX_DEPTH_VALUE);
    }

    if (fMinUncertainty > fMaxUncertainty)
    {
        fMinUncertainty = fMaxUncertainty = fNoDataValue;
    }
    else if (fMinUncertainty < 0)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Negative uncertainty value found, which is not allowed");
    }

    return bRet &&
           WriteFloat32Value(m_valuesGroup, "minimumDepth", fMinDepth) &&
           WriteFloat32Value(m_valuesGroup, "maximumDepth", fMaxDepth) &&
           WriteFloat32Value(m_valuesGroup, "minimumUncertainty",
                             fMinUncertainty) &&
           WriteFloat32Value(m_valuesGroup, "maximumUncertainty",
                             fMaxUncertainty);
}

/************************************************************************/
/*                   S102Creator::CopyQualityValues()                   */
/************************************************************************/

bool S102Creator::CopyQualityValues(GDALDataset *poQualityDS,
                                    const std::set<int> &oSetRATId,
                                    GDALProgressFunc pfnProgress,
                                    void *pProgressData)
{
    CPLAssert(m_valuesGroup.get() >= 0);

    const int nYSize = poQualityDS->GetRasterYSize();
    const int nXSize = poQualityDS->GetRasterXSize();

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
    constexpr uint32_t nNoDataValue = 0;

    hsize_t chunk_size[] = {static_cast<hsize_t>(nBlockYSize),
                            static_cast<hsize_t>(nBlockXSize)};

    GH5_HIDParametersHolder hParams(H5_CHECK(H5Pcreate(H5P_DATASET_CREATE)));
    bRet = bRet && hParams &&
           H5_CHECK(H5Pset_fill_time(hParams, H5D_FILL_TIME_ALLOC)) >= 0 &&
           H5_CHECK(H5Pset_fill_value(hParams, H5T_STD_U32LE, &nNoDataValue)) >=
               0 &&
           H5_CHECK(H5Pset_layout(hParams, H5D_CHUNKED)) >= 0 &&
           H5_CHECK(H5Pset_chunk(hParams, 2, chunk_size)) >= 0;

    if (bRet && bDeflate)
    {
        bRet = H5_CHECK(H5Pset_deflate(hParams, nCompressionLevel)) >= 0;
    }

    GH5_HIDDatasetHolder hDatasetID;
    if (bRet)
    {
        hDatasetID.reset(H5_CHECK(H5Dcreate(
            m_valuesGroup, "values", H5T_STD_U32LE, hDataSpace, hParams)));
        bRet = hDatasetID;
    }

    GH5_HIDSpaceHolder hFileSpace(H5_CHECK(H5Dget_space(hDatasetID)));
    bRet = bRet && hFileSpace;

    const int nYBlocks = static_cast<int>(DIV_ROUND_UP(nYSize, nBlockYSize));
    const int nXBlocks = static_cast<int>(DIV_ROUND_UP(nXSize, nBlockXSize));
    std::vector<uint32_t> anValues(static_cast<size_t>(nBlockYSize) *
                                   nBlockXSize);
    const bool bReverseY = m_gt.yscale < 0;

    int bHasSrcNoData = FALSE;
    const double dfSrcNoData =
        poQualityDS->GetRasterBand(1)->GetNoDataValue(&bHasSrcNoData);
    const uint32_t nSrcNoData = static_cast<uint32_t>(dfSrcNoData);

    std::set<int> oSetRATIdCopy(oSetRATId);
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
                poQualityDS->GetRasterBand(1)->RasterIO(
                    GF_Read, iX * nBlockXSize, nSrcYOff, nReqCountX, nReqCountY,
                    bReverseY ? anValues.data() + (nReqCountY - 1) * nReqCountX
                              : anValues.data(),
                    nReqCountX, nReqCountY, GDT_UInt32, 0,
                    bReverseY ? -static_cast<GPtrDiff_t>(sizeof(uint32_t)) *
                                    nReqCountX
                              : 0,
                    nullptr) == CE_None;

            if (bRet)
            {
                for (int i = 0; i < nReqCountY * nReqCountX; i++)
                {
                    if (bHasSrcNoData && anValues[i] == nSrcNoData)
                    {
                        anValues[i] = nNoDataValue;
                    }
                    else if (anValues[i] != 0 &&
                             !cpl::contains(oSetRATIdCopy, anValues[i]))
                    {
                        CPLError(
                            CE_Warning, CPLE_AppDefined,
                            "Quality grid contains nodes with id %u, but there "
                            "is no such entry in the feature attribute table",
                            anValues[i]);
                        oSetRATIdCopy.insert(anValues[i]);
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
            GH5_HIDSpaceHolder hMemSpace(H5Screate_simple(2, count, nullptr));
            bRet =
                bRet && hMemSpace &&
                H5_CHECK(H5Sselect_hyperslab(hFileSpace, H5S_SELECT_SET, offset,
                                             nullptr, count, nullptr)) >= 0 &&
                H5_CHECK(H5Dwrite(hDatasetID, H5T_NATIVE_UINT, hMemSpace,
                                  hFileSpace, H5P_DEFAULT, anValues.data())) >=
                    0 &&
                pfnProgress((static_cast<double>(iY) * nXBlocks + iX + 1) /
                                (static_cast<double>(nXBlocks) * nYBlocks),
                            "", pProgressData) != 0;
        }
    }

    return bRet;
}

/************************************************************************/
/*                      S102Dataset::CreateCopy()                       */
/************************************************************************/

/* static */
GDALDataset *S102Dataset::CreateCopy(const char *pszFilename,
                                     GDALDataset *poSrcDS, int /* bStrict*/,
                                     CSLConstList papszOptions,
                                     GDALProgressFunc pfnProgress,
                                     void *pProgressData)
{
    S102Creator creator(pszFilename, poSrcDS, papszOptions);
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
/*                      S102DatasetDriverUnload()                       */
/************************************************************************/

static void S102DatasetDriverUnload(GDALDriver *)
{
    HDF5UnloadFileDriver();
}

/************************************************************************/
/*                         GDALRegister_S102()                          */
/************************************************************************/
void GDALRegister_S102()

{
    if (!GDAL_CHECK_VERSION("S102"))
        return;

    if (GDALGetDriverByName(S102_DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    S102DriverSetCommonMetadata(poDriver);
    poDriver->pfnOpen = S102Dataset::Open;
    poDriver->pfnCreateCopy = S102Dataset::CreateCopy;
    poDriver->pfnUnloadDriver = S102DatasetDriverUnload;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}

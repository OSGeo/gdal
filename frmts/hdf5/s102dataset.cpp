/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  Read S102 bathymetric datasets.
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

#include <cmath>
#include <limits>

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
/*                   S102GeoreferencedMetadataRasterBand                */
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
/*                           OpenQuality()                              */
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
    poDriver->pfnUnloadDriver = S102DatasetDriverUnload;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}

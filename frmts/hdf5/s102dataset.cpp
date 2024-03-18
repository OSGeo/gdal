/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  Read S102 bathymetric datasets.
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys dot com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_port.h"
#include "hdf5dataset.h"
#include "hdf5drivercore.h"
#include "gh5_convenience.h"
#include "rat.h"
#include "s100.h"

#include "gdal_priv.h"
#include "gdal_proxy.h"
#include "gdal_rat.h"

#include <limits>

/************************************************************************/
/*                             S102Dataset                              */
/************************************************************************/

class S102Dataset final : public S100BaseDataset
{
    bool OpenQualityOfSurvey(GDALOpenInfo *poOpenInfo,
                             const std::shared_ptr<GDALGroup> &poRootGroup);

  public:
    explicit S102Dataset(const std::string &osFilename)
        : S100BaseDataset(osFilename)
    {
    }

    static GDALDataset *Open(GDALOpenInfo *);
};

/************************************************************************/
/*                            S102RasterBand                            */
/************************************************************************/

class S102RasterBand : public GDALProxyRasterBand
{
    friend class S102Dataset;
    std::unique_ptr<GDALDataset> m_poDS{};
    GDALRasterBand *m_poUnderlyingBand = nullptr;
    double m_dfMinimum = std::numeric_limits<double>::quiet_NaN();
    double m_dfMaximum = std::numeric_limits<double>::quiet_NaN();

  public:
    explicit S102RasterBand(std::unique_ptr<GDALDataset> &&poDSIn)
        : m_poDS(std::move(poDSIn)),
          m_poUnderlyingBand(m_poDS->GetRasterBand(1))
    {
        eDataType = m_poUnderlyingBand->GetRasterDataType();
        m_poUnderlyingBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
    }

    GDALRasterBand *
    RefUnderlyingRasterBand(bool /*bForceOpen*/ = true) const override
    {
        return m_poUnderlyingBand;
    }

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

/************************************************************************/
/*                   S102GeoreferencedMetadataRasterBand                */
/************************************************************************/

class S102GeoreferencedMetadataRasterBand : public GDALProxyRasterBand
{
    friend class S102Dataset;

    std::unique_ptr<GDALDataset> m_poDS{};
    GDALRasterBand *m_poUnderlyingBand = nullptr;
    std::unique_ptr<GDALRasterAttributeTable> m_poRAT{};

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
    RefUnderlyingRasterBand(bool /*bForceOpen*/ = true) const override
    {
        return m_poUnderlyingBand;
    }

    GDALRasterAttributeTable *GetDefaultRAT() override
    {
        return m_poRAT.get();
    }
};

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
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The S102 driver does not support update access.");
        return nullptr;
    }

    std::string osFilename(poOpenInfo->pszFilename);
    bool bIsSubdataset = false;
    bool bIsQualityOfSurvey = false;
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
            else if (EQUAL(aosTokens[2], "QualityOfSurvey"))
            {
                bIsQualityOfSurvey = true;
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
    auto poBathymetryCoverage01 = poRootGroup->OpenGroupFromFullname(
        "/BathymetryCoverage/BathymetryCoverage.01");
    if (!poBathymetryCoverage01)
        return nullptr;

    const bool bNorthUp = CPLTestBool(
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "NORTH_UP", "YES"));

    if (bIsQualityOfSurvey)
    {
        if (!poDS->OpenQualityOfSurvey(poOpenInfo, poRootGroup))
            return nullptr;

        // Setup/check for pam .aux.xml.
        poDS->SetDescription(osFilename.c_str());
        poDS->TryLoadXML();

        // Setup overviews.
        poDS->oOvManager.Initialize(poDS.get(), osFilename.c_str());

        return poDS.release();
    }

    // Compute geotransform
    poDS->m_bHasGT = S100GetGeoTransform(poBathymetryCoverage01.get(),
                                         poDS->m_adfGeoTransform, bNorthUp);

    auto poGroup001 = poBathymetryCoverage01->OpenGroup("Group_001");
    if (!poGroup001)
        return nullptr;
    auto poValuesArray = poGroup001->OpenMDArray("values");
    if (!poValuesArray || poValuesArray->GetDimensionCount() != 2)
        return nullptr;

    const auto &oType = poValuesArray->GetDataType();
    if (oType.GetClass() != GEDTC_COMPOUND)
        return nullptr;
    const auto &oComponents = oType.GetComponents();
    if (oComponents.size() != 2 || oComponents[0]->GetName() != "depth" ||
        oComponents[1]->GetName() != "uncertainty")
    {
        return nullptr;
    }

    if (bNorthUp)
        poValuesArray = poValuesArray->GetView("[::-1,...]");

    auto poDepth = poValuesArray->GetView("[\"depth\"]");

    // Mandatory in v2.2
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

    auto poUncertainty = poValuesArray->GetView("[\"uncertainty\"]");
    const double dfUncertaintyNoData = poUncertainty->GetNoDataValueAsDouble();
    auto poUncertaintyDS =
        std::unique_ptr<GDALDataset>(poUncertainty->AsClassicDataset(1, 0));

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

    // Create uncertainty band
    auto poUncertaintyBand = new S102RasterBand(std::move(poUncertaintyDS));
    poUncertaintyBand->SetDescription("uncertainty");

    auto poMinimumUncertainty = poGroup001->GetAttribute("minimumUncertainty");
    if (poMinimumUncertainty &&
        poMinimumUncertainty->GetDataType().GetClass() == GEDTC_NUMERIC)
    {
        const double dfVal = poMinimumUncertainty->ReadAsDouble();
        if (dfVal != dfUncertaintyNoData)
        {
            poUncertaintyBand->m_dfMinimum = dfVal;
        }
    }

    auto poMaximumUncertainty = poGroup001->GetAttribute("maximumUncertainty");
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

    poDS->GDALDataset::SetMetadataItem(GDALMD_AREA_OR_POINT, GDALMD_AOP_POINT);

    auto poGroupQualityOfSurvey = poRootGroup->OpenGroup("QualityOfSurvey");
    if (!bIsSubdataset && poGroupQualityOfSurvey)
    {
        auto poGroupQualityOfSurvey01 =
            poGroupQualityOfSurvey->OpenGroup("QualityOfSurvey.01");
        if (poGroupQualityOfSurvey01)
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
                CPLSPrintf("S102:\"%s\":QualityOfSurvey", osFilename.c_str()),
                "SUBDATASETS");
            poDS->GDALDataset::SetMetadataItem(
                "SUBDATASET_2_DESC", "Georeferenced metadata QualityOfSurvey",
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
/*                       OpenQualityOfSurvey()                          */
/************************************************************************/

bool S102Dataset::OpenQualityOfSurvey(
    GDALOpenInfo *poOpenInfo, const std::shared_ptr<GDALGroup> &poRootGroup)
{
    const bool bNorthUp = CPLTestBool(
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "NORTH_UP", "YES"));

    auto poGroupQualityOfSurvey = poRootGroup->OpenGroup("QualityOfSurvey");
    if (!poGroupQualityOfSurvey)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find group /QualityOfSurvey");
        return false;
    }

    auto poGroupQualityOfSurvey01 =
        poGroupQualityOfSurvey->OpenGroup("QualityOfSurvey.01");
    if (!poGroupQualityOfSurvey01)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find group /QualityOfSurvey/QualityOfSurvey.01");
        return false;
    }

    if (auto poStartSequence =
            poGroupQualityOfSurvey01->GetAttribute("startSequence"))
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
    m_bHasGT = S100GetGeoTransform(poGroupQualityOfSurvey01.get(),
                                   m_adfGeoTransform, bNorthUp);

    auto poGroup001 = poGroupQualityOfSurvey01->OpenGroup("Group_001");
    if (!poGroup001)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Cannot find group /QualityOfSurvey/QualityOfSurvey.01/Group_001");
        return false;
    }

    auto poValuesArray = poGroup001->OpenMDArray("values");
    if (!poValuesArray)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find array "
                 "/QualityOfSurvey/QualityOfSurvey.01/Group_001/values");
        return false;
    }

    {
        const auto &oType = poValuesArray->GetDataType();
        if (oType.GetClass() != GEDTC_NUMERIC &&
            oType.GetNumericDataType() != GDT_UInt32)
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
        poGroupQualityOfSurvey->OpenMDArray("featureAttributeTable");
    if (!poFeatureAttributeTable)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find array /QualityOfSurvey/featureAttributeTable");
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

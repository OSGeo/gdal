/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  Read S104 datasets.
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
/*                             S104Dataset                              */
/************************************************************************/

class S104Dataset final : public S100BaseDataset
{
  public:
    explicit S104Dataset(const std::string &osFilename)
        : S100BaseDataset(osFilename)
    {
    }

    static GDALDataset *Open(GDALOpenInfo *);
};

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

  public:
    explicit S104RasterBand(std::unique_ptr<GDALDataset> &&poDSIn)
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

    const char *GetUnitType() override
    {
        return m_osUnitType.c_str();
    }

    GDALRasterAttributeTable *GetDefaultRAT() override
    {
        return m_poRAT.get();
    }
};

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
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The S104 driver does not support update access.");
        return nullptr;
    }

    std::string osFilename(poOpenInfo->pszFilename);
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
        else
        {
            return nullptr;
        }
    }

    auto poDS = std::make_unique<S104Dataset>(osFilename);
    if (!poDS->Init())
        return nullptr;

    const auto &poRootGroup = poDS->m_poRootGroup;

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

    auto poWaterLevel01 = poWaterLevel->OpenGroup("WaterLevel.01");
    if (!poWaterLevel01)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find /WaterLevel/WaterLevel.01 group");
        return nullptr;
    }

    // Read additional metadata
    for (const char *pszAttrName :
         {"timeRecordInterval", "dateTimeOfFirstRecord", "dateTimeOfLastRecord",
          "numberOfTimes"})
    {
        auto poAttr = poWaterLevel01->GetAttribute(pszAttrName);
        if (poAttr)
        {
            const char *pszVal = poAttr->ReadAsString();
            if (pszVal)
            {
                poDS->GDALDataset::SetMetadataItem(pszAttrName, pszVal);
            }
        }
    }

    if (auto poStartSequence = poWaterLevel01->GetAttribute("startSequence"))
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
            poWaterLevel01.get(), poDS->nRasterXSize, poDS->nRasterYSize))
    {
        return nullptr;
    }

    const bool bNorthUp = CPLTestBool(
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "NORTH_UP", "YES"));

    // Compute geotransform
    poDS->m_bHasGT = S100GetGeoTransform(poWaterLevel01.get(),
                                         poDS->m_adfGeoTransform, bNorthUp);

    if (osGroup.empty())
    {
        const auto aosGroupNames = poWaterLevel01->GetGroupNames();
        int iSubDS = 1;
        for (const auto &osSubGroup : aosGroupNames)
        {
            if (auto poSubGroup = poWaterLevel01->OpenGroup(osSubGroup))
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
        auto poGroup = poWaterLevel01->OpenGroup(osGroup);
        if (!poGroup)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find /WaterLevel/WaterLevel.01/%s group",
                     osGroup.c_str());
            return nullptr;
        }

        auto poValuesArray = poGroup->OpenMDArray("values");
        if (!poValuesArray)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find /WaterLevel/WaterLevel.01/%s/values array",
                     osGroup.c_str());
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

/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  Read S111 datasets.
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
/*                             S111Dataset                              */
/************************************************************************/

class S111Dataset final : public S100BaseDataset
{
  public:
    explicit S111Dataset(const std::string &osFilename)
        : S100BaseDataset(osFilename)
    {
    }

    static GDALDataset *Open(GDALOpenInfo *);
};

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

  public:
    explicit S111RasterBand(std::unique_ptr<GDALDataset> &&poDSIn)
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

    char **GetMetadata(const char *pszDomain) override
    {
        // Short-circuit GDALProxyRasterBand...
        return GDALRasterBand::GetMetadata(pszDomain);
    }
};

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
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The S111 driver does not support update access.");
        return nullptr;
    }

    std::string osFilename(poOpenInfo->pszFilename);
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
        else
        {
            return nullptr;
        }
    }

    auto poDS = std::make_unique<S111Dataset>(osFilename);
    if (!poDS->Init())
        return nullptr;

    const auto &poRootGroup = poDS->m_poRootGroup;

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
          "maxDatasetCurrentSpeed"})
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

    auto poSurfaceCurrent01 = poSurfaceCurrent->OpenGroup("SurfaceCurrent.01");
    if (!poSurfaceCurrent01)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find /SurfaceCurrent/SurfaceCurrent.01 group");
        return nullptr;
    }

    // Read additional metadata
    for (const char *pszAttrName :
         {"timeRecordInterval", "dateTimeOfFirstRecord", "dateTimeOfLastRecord",
          "numberOfTimes"})
    {
        auto poAttr = poSurfaceCurrent01->GetAttribute(pszAttrName);
        if (poAttr)
        {
            const char *pszVal = poAttr->ReadAsString();
            if (pszVal)
            {
                poDS->GDALDataset::SetMetadataItem(pszAttrName, pszVal);
            }
        }
    }

    if (auto poStartSequence =
            poSurfaceCurrent01->GetAttribute("startSequence"))
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
            poSurfaceCurrent01.get(), poDS->nRasterXSize, poDS->nRasterYSize))
    {
        return nullptr;
    }

    const bool bNorthUp = CPLTestBool(
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "NORTH_UP", "YES"));

    // Compute geotransform
    poDS->m_bHasGT = S100GetGeoTransform(poSurfaceCurrent01.get(),
                                         poDS->m_adfGeoTransform, bNorthUp);

    if (osGroup.empty())
    {
        const auto aosGroupNames = poSurfaceCurrent01->GetGroupNames();
        int iSubDS = 1;
        for (const auto &osSubGroup : aosGroupNames)
        {
            if (auto poSubGroup = poSurfaceCurrent01->OpenGroup(osSubGroup))
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
        auto poGroup = poSurfaceCurrent01->OpenGroup(osGroup);
        if (!poGroup)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find /SurfaceCurrent/SurfaceCurrent.01/%s group",
                     osGroup.c_str());
            return nullptr;
        }

        auto poValuesArray = poGroup->OpenMDArray("values");
        if (!poValuesArray)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Cannot find /SurfaceCurrent/SurfaceCurrent.01/%s/values array",
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
        if (!(oComponents.size() == 2 &&
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
    poDriver->pfnUnloadDriver = S111DatasetDriverUnload;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}

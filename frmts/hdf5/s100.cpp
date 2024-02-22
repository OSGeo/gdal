/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  Read S100 bathymetric datasets.
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

#include "s100.h"
#include "hdf5dataset.h"

#include <algorithm>
#include <cmath>

/************************************************************************/
/*                       S100BaseDataset()                              */
/************************************************************************/

S100BaseDataset::S100BaseDataset(const std::string &osFilename)
    : m_osFilename(osFilename)
{
}

/************************************************************************/
/*                              Init()                                  */
/************************************************************************/

bool S100BaseDataset::Init()
{
    // Open the file as an HDF5 file.
    hid_t fapl = H5Pcreate(H5P_FILE_ACCESS);
    H5Pset_driver(fapl, HDF5GetFileDriver(), nullptr);
    hid_t hHDF5 = H5Fopen(m_osFilename.c_str(), H5F_ACC_RDONLY, fapl);
    H5Pclose(fapl);
    if (hHDF5 < 0)
        return false;

    auto poSharedResources = GDAL::HDF5SharedResources::Create(m_osFilename);
    poSharedResources->m_hHDF5 = hHDF5;

    m_poRootGroup = HDF5Dataset::OpenGroup(poSharedResources);
    if (m_poRootGroup == nullptr)
        return false;

    S100ReadSRS(m_poRootGroup.get(), m_oSRS);

    S100ReadVerticalDatum(this, m_poRootGroup.get());

    m_osMetadataFile =
        S100ReadMetadata(this, m_osFilename, m_poRootGroup.get());

    return true;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr S100BaseDataset::GetGeoTransform(double *padfGeoTransform)

{
    if (m_bHasGT)
    {
        memcpy(padfGeoTransform, m_adfGeoTransform, sizeof(double) * 6);
        return CE_None;
    }

    return GDALPamDataset::GetGeoTransform(padfGeoTransform);
}

/************************************************************************/
/*                         GetSpatialRef()                              */
/************************************************************************/

const OGRSpatialReference *S100BaseDataset::GetSpatialRef() const
{
    if (!m_oSRS.IsEmpty())
        return &m_oSRS;
    return GDALPamDataset::GetSpatialRef();
}

/************************************************************************/
/*                         GetFileList()                                */
/************************************************************************/

char **S100BaseDataset::GetFileList()
{
    char **papszFileList = GDALPamDataset::GetFileList();
    if (!m_osMetadataFile.empty())
        papszFileList = CSLAddString(papszFileList, m_osMetadataFile.c_str());
    return papszFileList;
}

/************************************************************************/
/*                            S100ReadSRS()                             */
/************************************************************************/

bool S100ReadSRS(const GDALGroup *poRootGroup, OGRSpatialReference &oSRS)
{
    // Get SRS
    auto poHorizontalCRS = poRootGroup->GetAttribute("horizontalCRS");
    if (poHorizontalCRS &&
        poHorizontalCRS->GetDataType().GetClass() == GEDTC_NUMERIC)
    {
        // horizontalCRS is v2.2
        oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if (oSRS.importFromEPSG(poHorizontalCRS->ReadAsInt()) != OGRERR_NONE)
        {
            oSRS.Clear();
        }
    }
    else
    {
        auto poHorizontalDatumReference =
            poRootGroup->GetAttribute("horizontalDatumReference");
        auto poHorizontalDatumValue =
            poRootGroup->GetAttribute("horizontalDatumValue");
        if (poHorizontalDatumReference && poHorizontalDatumValue)
        {
            const char *pszAuthName =
                poHorizontalDatumReference->ReadAsString();
            const char *pszAuthCode = poHorizontalDatumValue->ReadAsString();
            if (pszAuthName && pszAuthCode)
            {
                oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                if (oSRS.SetFromUserInput(
                        (std::string(pszAuthName) + ':' + pszAuthCode).c_str(),
                        OGRSpatialReference::
                            SET_FROM_USER_INPUT_LIMITATIONS_get()) !=
                    OGRERR_NONE)
                {
                    oSRS.Clear();
                }
            }
        }
    }
    return !oSRS.IsEmpty();
}

/************************************************************************/
/*               S100GetNumPointsLongitudinalLatitudinal()              */
/************************************************************************/

bool S100GetNumPointsLongitudinalLatitudinal(const GDALGroup *poGroup,
                                             int &nNumPointsLongitudinal,
                                             int &nNumPointsLatitudinal)
{
    auto poSpacingX = poGroup->GetAttribute("gridSpacingLongitudinal");
    auto poSpacingY = poGroup->GetAttribute("gridSpacingLatitudinal");
    auto poNumPointsLongitudinal =
        poGroup->GetAttribute("numPointsLongitudinal");
    auto poNumPointsLatitudinal = poGroup->GetAttribute("numPointsLatitudinal");
    if (poSpacingX &&
        poSpacingX->GetDataType().GetNumericDataType() == GDT_Float64 &&
        poSpacingY &&
        poSpacingY->GetDataType().GetNumericDataType() == GDT_Float64 &&
        poNumPointsLongitudinal &&
        GDALDataTypeIsInteger(
            poNumPointsLongitudinal->GetDataType().GetNumericDataType()) &&
        poNumPointsLatitudinal &&
        GDALDataTypeIsInteger(
            poNumPointsLatitudinal->GetDataType().GetNumericDataType()))
    {
        nNumPointsLongitudinal = poNumPointsLongitudinal->ReadAsInt();
        nNumPointsLatitudinal = poNumPointsLatitudinal->ReadAsInt();

        // Those are optional, but use them when available, to detect
        // potential inconsistency
        auto poEastBoundLongitude = poGroup->GetAttribute("eastBoundLongitude");
        auto poWestBoundLongitude = poGroup->GetAttribute("westBoundLongitude");
        auto poSouthBoundLongitude =
            poGroup->GetAttribute("southBoundLatitude");
        auto poNorthBoundLatitude = poGroup->GetAttribute("northBoundLatitude");
        if (poEastBoundLongitude &&
            GDALDataTypeIsFloating(
                poEastBoundLongitude->GetDataType().GetNumericDataType()) &&
            poWestBoundLongitude &&
            GDALDataTypeIsFloating(
                poWestBoundLongitude->GetDataType().GetNumericDataType()) &&
            poSouthBoundLongitude &&
            GDALDataTypeIsFloating(
                poSouthBoundLongitude->GetDataType().GetNumericDataType()) &&
            poNorthBoundLatitude &&
            GDALDataTypeIsFloating(
                poNorthBoundLatitude->GetDataType().GetNumericDataType()))
        {
            const double dfSpacingX = poSpacingX->ReadAsDouble();
            const double dfSpacingY = poSpacingY->ReadAsDouble();

            const double dfEast = poEastBoundLongitude->ReadAsDouble();
            const double dfWest = poWestBoundLongitude->ReadAsDouble();
            const double dfSouth = poSouthBoundLongitude->ReadAsDouble();
            const double dfNorth = poNorthBoundLatitude->ReadAsDouble();
            if (std::fabs((dfWest + nNumPointsLongitudinal * dfSpacingX) -
                          dfEast) < 5 * dfSpacingX &&
                std::fabs((dfSouth + nNumPointsLatitudinal * dfSpacingY) -
                          dfNorth) < 5 * dfSpacingY)
            {
                // We need up to 5 spacings for product
                // S-111 Trial Data Set Release 1.1/111UK_20210401T000000Z_SolentAndAppr_dcf2.h5
            }
            else
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Caution: "
                    "eastBoundLongitude/westBoundLongitude/southBoundLatitude/"
                    "northBoundLatitude/gridSpacingLongitudinal/"
                    "gridSpacingLatitudinal/numPointsLongitudinal/"
                    "numPointsLatitudinal do not seem to be consistent");
                CPLDebug("S100", "Computed east = %f. Actual = %f",
                         dfWest + nNumPointsLongitudinal * dfSpacingX, dfEast);
                CPLDebug("S100", "Computed north = %f. Actual = %f",
                         dfSouth + nNumPointsLatitudinal * dfSpacingY, dfNorth);
            }
        }

        return true;
    }
    return false;
}

/************************************************************************/
/*                        S100GetGeoTransform()                         */
/************************************************************************/

bool S100GetGeoTransform(const GDALGroup *poGroup, double adfGeoTransform[6],
                         bool bNorthUp)
{
    auto poOriginX = poGroup->GetAttribute("gridOriginLongitude");
    auto poOriginY = poGroup->GetAttribute("gridOriginLatitude");
    auto poSpacingX = poGroup->GetAttribute("gridSpacingLongitudinal");
    auto poSpacingY = poGroup->GetAttribute("gridSpacingLatitudinal");
    if (poOriginX &&
        poOriginX->GetDataType().GetNumericDataType() == GDT_Float64 &&
        poOriginY &&
        poOriginY->GetDataType().GetNumericDataType() == GDT_Float64 &&
        poSpacingX &&
        poSpacingX->GetDataType().GetNumericDataType() == GDT_Float64 &&
        poSpacingY &&
        poSpacingY->GetDataType().GetNumericDataType() == GDT_Float64)
    {
        int nNumPointsLongitudinal = 0;
        int nNumPointsLatitudinal = 0;
        if (!S100GetNumPointsLongitudinalLatitudinal(
                poGroup, nNumPointsLongitudinal, nNumPointsLatitudinal))
            return false;

        const double dfSpacingX = poSpacingX->ReadAsDouble();
        const double dfSpacingY = poSpacingY->ReadAsDouble();

        adfGeoTransform[0] = poOriginX->ReadAsDouble();
        adfGeoTransform[3] =
            poOriginY->ReadAsDouble() +
            (bNorthUp ? dfSpacingY * (nNumPointsLatitudinal - 1) : 0);
        adfGeoTransform[1] = dfSpacingX;
        adfGeoTransform[5] = bNorthUp ? -dfSpacingY : dfSpacingY;

        // From pixel-center convention to pixel-corner convention
        adfGeoTransform[0] -= adfGeoTransform[1] / 2;
        adfGeoTransform[3] -= adfGeoTransform[5] / 2;

        return true;
    }
    return false;
}

/************************************************************************/
/*                        S100GetDimensions()                           */
/************************************************************************/

bool S100GetDimensions(
    const GDALGroup *poGroup,
    std::vector<std::shared_ptr<GDALDimension>> &apoDims,
    std::vector<std::shared_ptr<GDALMDArray>> &apoIndexingVars)
{
    auto poOriginX = poGroup->GetAttribute("gridOriginLongitude");
    auto poOriginY = poGroup->GetAttribute("gridOriginLatitude");
    auto poSpacingX = poGroup->GetAttribute("gridSpacingLongitudinal");
    auto poSpacingY = poGroup->GetAttribute("gridSpacingLatitudinal");
    if (poOriginX &&
        poOriginX->GetDataType().GetNumericDataType() == GDT_Float64 &&
        poOriginY &&
        poOriginY->GetDataType().GetNumericDataType() == GDT_Float64 &&
        poSpacingX &&
        poSpacingX->GetDataType().GetNumericDataType() == GDT_Float64 &&
        poSpacingY &&
        poSpacingY->GetDataType().GetNumericDataType() == GDT_Float64)
    {
        int nNumPointsLongitudinal = 0;
        int nNumPointsLatitudinal = 0;
        if (!S100GetNumPointsLongitudinalLatitudinal(
                poGroup, nNumPointsLongitudinal, nNumPointsLatitudinal))
            return false;

        {
            auto poDim = std::make_shared<GDALDimensionWeakIndexingVar>(
                std::string(), "Y", GDAL_DIM_TYPE_HORIZONTAL_Y, std::string(),
                nNumPointsLatitudinal);
            auto poIndexingVar = GDALMDArrayRegularlySpaced::Create(
                std::string(), poDim->GetName(), poDim,
                poOriginY->ReadAsDouble(), poSpacingY->ReadAsDouble(), 0);
            poDim->SetIndexingVariable(poIndexingVar);
            apoDims.emplace_back(poDim);
            apoIndexingVars.emplace_back(poIndexingVar);
        }

        {
            auto poDim = std::make_shared<GDALDimensionWeakIndexingVar>(
                std::string(), "X", GDAL_DIM_TYPE_HORIZONTAL_X, std::string(),
                nNumPointsLongitudinal);
            auto poIndexingVar = GDALMDArrayRegularlySpaced::Create(
                std::string(), poDim->GetName(), poDim,
                poOriginX->ReadAsDouble(), poSpacingX->ReadAsDouble(), 0);
            poDim->SetIndexingVariable(poIndexingVar);
            apoDims.emplace_back(poDim);
            apoIndexingVars.emplace_back(poIndexingVar);
        }

        return true;
    }
    return false;
}

/************************************************************************/
/*                       S100ReadVerticalDatum()                        */
/************************************************************************/

void S100ReadVerticalDatum(GDALDataset *poDS, const GDALGroup *poRootGroup)
{
    // https://iho.int/uploads/user/pubs/standards/s-100/S-100_5.0.0_Final_Clean_Web.pdf
    // Table S100_VerticalAndSoundingDatum page 20
    static const struct
    {
        int nCode;
        const char *pszMeaning;
        const char *pszAbbrev;
    } asVerticalDatums[] = {
        {1, "meanLowWaterSprings", "MLWS"},
        {2, "meanLowerLowWaterSprings", nullptr},
        {3, "meanSeaLevel", "MSL"},
        {4, "lowestLowWater", nullptr},
        {5, "meanLowWater", "MLW"},
        {6, "lowestLowWaterSprings", nullptr},
        {7, "approximateMeanLowWaterSprings", nullptr},
        {8, "indianSpringLowWater", nullptr},
        {9, "lowWaterSprings", nullptr},
        {10, "approximateLowestAstronomicalTide", nullptr},
        {11, "nearlyLowestLowWater", nullptr},
        {12, "meanLowerLowWater", "MLLW"},
        {13, "lowWater", "LW"},
        {14, "approximateMeanLowWater", nullptr},
        {15, "approximateMeanLowerLowWater", nullptr},
        {16, "meanHighWater", "MHW"},
        {17, "meanHighWaterSprings", "MHWS"},
        {18, "highWater", "HW"},
        {19, "approximateMeanSeaLevel", nullptr},
        {20, "highWaterSprings", nullptr},
        {21, "meanHigherHighWater", "MHHW"},
        {22, "equinoctialSpringLowWater", nullptr},
        {23, "lowestAstronomicalTide", "LAT"},
        {24, "localDatum", nullptr},
        {25, "internationalGreatLakesDatum1985", nullptr},
        {26, "meanWaterLevel", nullptr},
        {27, "lowerLowWaterLargeTide", nullptr},
        {28, "higherHighWaterLargeTide", nullptr},
        {29, "nearlyHighestHighWater", nullptr},
        {30, "highestAstronomicalTide", "HAT"},
        {44, "balticSeaChartDatum2000", nullptr},
        {46, "internationalGreatLakesDatum2020", nullptr},
    };

    auto poVerticalDatum = poRootGroup->GetAttribute("verticalDatum");
    if (poVerticalDatum &&
        poVerticalDatum->GetDataType().GetClass() == GEDTC_NUMERIC)
    {
        bool bFound = false;
        const auto nVal = poVerticalDatum->ReadAsInt();
        for (const auto &sVerticalDatum : asVerticalDatums)
        {
            if (sVerticalDatum.nCode == nVal)
            {
                bFound = true;
                poDS->GDALDataset::SetMetadataItem("VERTICAL_DATUM_MEANING",
                                                   sVerticalDatum.pszMeaning);
                if (sVerticalDatum.pszAbbrev)
                    poDS->GDALDataset::SetMetadataItem(
                        "VERTICAL_DATUM_ABBREV", sVerticalDatum.pszAbbrev);
                break;
            }
        }
        if (!bFound)
        {
            poDS->GDALDataset::SetMetadataItem("verticalDatum",
                                               CPLSPrintf("%d", nVal));
        }
    }
}

/************************************************************************/
/*                         S100ReadMetadata()                           */
/************************************************************************/

std::string S100ReadMetadata(GDALDataset *poDS, const std::string &osFilename,
                             const GDALGroup *poRootGroup)
{
    std::string osMetadataFile;
    for (const auto &poAttr : poRootGroup->GetAttributes())
    {
        const auto &osName = poAttr->GetName();
        if (osName == "metadata")
        {
            const char *pszVal = poAttr->ReadAsString();
            if (pszVal && pszVal[0])
            {
                osMetadataFile = CPLFormFilename(CPLGetPath(osFilename.c_str()),
                                                 pszVal, nullptr);
                VSIStatBufL sStat;
                if (VSIStatL(osMetadataFile.c_str(), &sStat) != 0)
                {
                    // Test products from https://data.admiralty.co.uk/portal/apps/sites/#/marine-data-portal/pages/s-100
                    // advertise a metadata filename starting with "MD_", per the spec,
                    // but the actual filename does not start with "MD_"...
                    if (STARTS_WITH(pszVal, "MD_"))
                    {
                        osMetadataFile =
                            CPLFormFilename(CPLGetPath(osFilename.c_str()),
                                            pszVal + strlen("MD_"), nullptr);
                        if (VSIStatL(osMetadataFile.c_str(), &sStat) != 0)
                        {
                            osMetadataFile.clear();
                        }
                    }
                    else
                    {
                        osMetadataFile.clear();
                    }
                }
            }
        }
        else if (osName != "horizontalCRS" &&
                 osName != "horizontalDatumReference" &&
                 osName != "horizontalDatumValue" &&
                 osName != "productSpecification" &&
                 osName != "eastBoundLongitude" &&
                 osName != "northBoundLatitude" &&
                 osName != "southBoundLatitude" &&
                 osName != "westBoundLongitude" && osName != "extentTypeCode" &&
                 osName != "verticalCS" && osName != "verticalCoordinateBase" &&
                 osName != "verticalDatumReference" &&
                 osName != "verticalDatum")
        {
            const char *pszVal = poAttr->ReadAsString();
            if (pszVal && pszVal[0])
                poDS->GDALDataset::SetMetadataItem(osName.c_str(), pszVal);
        }
    }
    return osMetadataFile;
}

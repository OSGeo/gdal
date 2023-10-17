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
/*                        S100GetGeoTransform()                         */
/************************************************************************/

bool S100GetGeoTransform(const GDALGroup *poGroup, double adfGeoTransform[6],
                         bool bNorthUp)
{
    auto poOriginX = poGroup->GetAttribute("gridOriginLongitude");
    auto poOriginY = poGroup->GetAttribute("gridOriginLatitude");
    auto poSpacingX = poGroup->GetAttribute("gridSpacingLongitudinal");
    auto poSpacingY = poGroup->GetAttribute("gridSpacingLatitudinal");
    auto poNumPointsLongitudinal =
        poGroup->GetAttribute("numPointsLongitudinal");
    auto poNumPointsLatitudinal = poGroup->GetAttribute("numPointsLatitudinal");
    if (poOriginX &&
        poOriginX->GetDataType().GetNumericDataType() == GDT_Float64 &&
        poOriginY &&
        poOriginY->GetDataType().GetNumericDataType() == GDT_Float64 &&
        poSpacingX &&
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
        adfGeoTransform[0] = poOriginX->ReadAsDouble();
        adfGeoTransform[3] =
            poOriginY->ReadAsDouble() +
            (bNorthUp ? poSpacingY->ReadAsDouble() *
                            (poNumPointsLatitudinal->ReadAsInt() - 1)
                      : 0);
        adfGeoTransform[1] = poSpacingX->ReadAsDouble();
        adfGeoTransform[5] =
            bNorthUp ? -poSpacingY->ReadAsDouble() : poSpacingY->ReadAsDouble();

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
    auto poNumPointsLongitudinal =
        poGroup->GetAttribute("numPointsLongitudinal");
    auto poNumPointsLatitudinal = poGroup->GetAttribute("numPointsLatitudinal");
    if (poOriginX &&
        poOriginX->GetDataType().GetNumericDataType() == GDT_Float64 &&
        poOriginY &&
        poOriginY->GetDataType().GetNumericDataType() == GDT_Float64 &&
        poSpacingX &&
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
        {
            auto poDim = std::make_shared<GDALDimensionWeakIndexingVar>(
                std::string(), "Y", GDAL_DIM_TYPE_HORIZONTAL_Y, std::string(),
                poNumPointsLatitudinal->ReadAsInt());
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
                poNumPointsLongitudinal->ReadAsInt());
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

/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Read ArcGIS .vat.dbf raster attribute table
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_priv.h"
#include "gdal_rat.h"
#include "ogrsf_frmts.h"

#include <algorithm>

/************************************************************************/
/*                            GDALLoadVATDBF()                          */
/************************************************************************/

/**
 * \brief Load a ESRI .vat.dbf auxiliary file as a GDAL attribute table.
 *
 * @since GDAL 3.11
 */
std::unique_ptr<GDALRasterAttributeTable>
GDALLoadVATDBF(const char *pszFilename)
{
    auto poDS = std::unique_ptr<GDALDataset>(
        GDALDataset::Open(pszFilename, GDAL_OF_VECTOR | GDAL_OF_VERBOSE_ERROR,
                          nullptr, nullptr, nullptr));
    auto poLayer = poDS ? poDS->GetLayer(0) : nullptr;
    if (!poLayer)
        return nullptr;
    auto poRAT = std::make_unique<GDALDefaultRasterAttributeTable>();

    const auto poFDefn = poLayer->GetLayerDefn();
    int iRedIdxFloat = -1;
    int iGreenIdxFloat = -1;
    int iBlueIdxFloat = -1;
    const int nFieldCount = poFDefn->GetFieldCount();
    for (int i = 0; i < nFieldCount; ++i)
    {
        const auto poFieldDefn = poFDefn->GetFieldDefn(i);
        const auto eFieldType = poFieldDefn->GetType();
        const char *pszName = poFieldDefn->GetNameRef();
        if (EQUAL(pszName, "VALUE"))
        {
            if (eFieldType == OFTReal)
                poRAT->CreateColumn(pszName, GFT_Real, GFU_MinMax);
            else
                poRAT->CreateColumn(pszName, GFT_Integer, GFU_MinMax);
        }
        else if (EQUAL(pszName, "COUNT") &&
                 (eFieldType == OFTInteger || eFieldType == OFTInteger64))
        {
            poRAT->CreateColumn(pszName, GFT_Integer, GFU_PixelCount);
        }
        else if ((STARTS_WITH_CI(pszName, "CLASS") || EQUAL(pszName, "NAME")) &&
                 eFieldType == OFTString)
        {
            poRAT->CreateColumn(pszName, GFT_String, GFU_Name);
        }
        else if (EQUAL(pszName, "RED") && !strstr(pszName, "min") &&
                 !strstr(pszName, "max") && eFieldType == OFTReal)
        {
            // Convert from [0,1] to [0,255]
            iRedIdxFloat = i;
            poRAT->CreateColumn(pszName, GFT_Integer, GFU_Red);
        }
        else if (EQUAL(pszName, "GREEN") && !strstr(pszName, "min") &&
                 !strstr(pszName, "max") && eFieldType == OFTReal)
        {
            // Convert from [0,1] to [0,255]
            iGreenIdxFloat = i;
            poRAT->CreateColumn(pszName, GFT_Integer, GFU_Green);
        }
        else if (EQUAL(pszName, "BLUE") && !strstr(pszName, "min") &&
                 !strstr(pszName, "max") && eFieldType == OFTReal)
        {
            // Convert from [0,1] to [0,255]
            iBlueIdxFloat = i;
            poRAT->CreateColumn(pszName, GFT_Integer, GFU_Blue);
        }
        else
        {
            poRAT->CreateColumn(
                pszName,
                eFieldType == OFTReal ? GFT_Real
                : (eFieldType == OFTInteger || eFieldType == OFTInteger64)
                    ? GFT_Integer
                    : GFT_String,
                GFU_Generic);
        }
    }

    int iRow = 0;
    for (auto &&poFeature : *poLayer)
    {
        for (int i = 0; i < nFieldCount; ++i)
        {
            if (i == iRedIdxFloat || i == iGreenIdxFloat || i == iBlueIdxFloat)
            {
                poRAT->SetValue(
                    iRow, i,
                    static_cast<int>(
                        std::clamp(255.0 * poFeature->GetFieldAsDouble(i) + 0.5,
                                   0.0, 255.0)));
            }
            else
            {
                switch (poRAT->GDALDefaultRasterAttributeTable::GetTypeOfCol(i))
                {
                    case GFT_Integer:
                    {
                        poRAT->GDALDefaultRasterAttributeTable::SetValue(
                            iRow, i, poFeature->GetFieldAsInteger(i));
                        break;
                    }
                    case GFT_Real:
                    {
                        poRAT->GDALDefaultRasterAttributeTable::SetValue(
                            iRow, i, poFeature->GetFieldAsDouble(i));
                        break;
                    }
                    case GFT_String:
                    {
                        poRAT->GDALDefaultRasterAttributeTable::SetValue(
                            iRow, i, poFeature->GetFieldAsString(i));
                        break;
                    }
                }
            }
        }
        ++iRow;
    }

    return poRAT;
}

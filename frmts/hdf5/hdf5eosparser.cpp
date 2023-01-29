/******************************************************************************
 * $Id$
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  Implementation of HDF5 HDFEOS parser
 * Author:   Even Rouault
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_error.h"
#include "nasakeywordhandler.h"

#include "hdf5eosparser.h"

#include <cstring>
#include <utility>

/************************************************************************/
/*                             HasHDFEOS()                              */
/************************************************************************/

bool HDF5EOSParser::HasHDFEOS(hid_t hRoot)
{
    hsize_t numObjs = 0;
    H5Gget_num_objs(hRoot, &numObjs);
    bool bFound = false;
    for (hsize_t i = 0; i < numObjs; ++i)
    {
        char szName[128];
        ssize_t nLen =
            H5Gget_objname_by_idx(hRoot, i, szName, sizeof(szName) - 1);
        if (nLen > 0)
        {
            szName[nLen] = 0;
            if (strcmp(szName, "HDFEOS INFORMATION") == 0)
            {
                bFound = true;
                break;
            }
        }
    }
    if (!bFound)
        return false;

    H5G_stat_t oStatbuf;
    if (H5Gget_objinfo(hRoot, "HDFEOS INFORMATION", false, &oStatbuf) < 0)
        return false;

    auto hHDFEOSInformation = H5Gopen(hRoot, "HDFEOS INFORMATION");
    if (hHDFEOSInformation < 0)
    {
        return false;
    }
    H5Gclose(hHDFEOSInformation);
    return true;
}

/************************************************************************/
/*                               Parse()                                */
/************************************************************************/

bool HDF5EOSParser::Parse(hid_t hRoot)
{
    auto hHDFEOSInformation = H5Gopen(hRoot, "HDFEOS INFORMATION");
    if (hHDFEOSInformation < 0)
    {
        return false;
    }

    const hid_t hArrayId = H5Dopen(hHDFEOSInformation, "StructMetadata.0");
    if (hArrayId < 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find StructMetadata.0");
        H5Gclose(hHDFEOSInformation);
        return false;
    }

    const hid_t hAttrSpace = H5Dget_space(hArrayId);
    const hid_t hAttrTypeID = H5Dget_type(hArrayId);
    const hid_t hAttrNativeType =
        H5Tget_native_type(hAttrTypeID, H5T_DIR_DEFAULT);

    // Fetch StructMetadata.0 content in a std::string
    std::string osResult;
    if (H5Tget_class(hAttrNativeType) == H5T_STRING &&
        !H5Tis_variable_str(hAttrNativeType) &&
        H5Sget_simple_extent_ndims(hAttrSpace) == 0)
    {
        const auto nSize = H5Tget_size(hAttrNativeType);
        if (nSize > 10 * 1024 * 1024)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Too large HDFEOS INFORMATION.StructMetadata.0");
        }
        else
        {
            osResult.resize(nSize);
            H5Dread(hArrayId, hAttrNativeType, H5S_ALL, hAttrSpace, H5P_DEFAULT,
                    &osResult[0]);
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "HDFEOS INFORMATION.StructMetadata.0 not of type string");
    }
    H5Sclose(hAttrSpace);
    H5Tclose(hAttrNativeType);
    H5Tclose(hAttrTypeID);

    H5Dclose(hArrayId);
    H5Gclose(hHDFEOSInformation);

    if (osResult.empty())
        return false;

    // Parse StructMetadata.0 with NASAKeywordHandler
    NASAKeywordHandler oKWHandler;
    if (!oKWHandler.Parse(osResult.c_str()))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot parse HDFEOS INFORMATION.StructMetadata.0 with "
                 "NASAKeywordHandler");
        return false;
    }

    auto oJsonRoot = oKWHandler.GetJsonObject();
    auto oGridStructure = oJsonRoot.GetObj("GridStructure");
    if (!oGridStructure.IsValid())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find GridStructure");
        return false;
    }

    for (auto &oGrid : oGridStructure.GetChildren())
    {
        if (oGrid.GetType() == CPLJSONObject::Type::Object)
        {
            auto osGridName = oGrid.GetString("GridName");
            auto oDataFields = oGrid.GetObj("DataField");
            auto oDimensions = oGrid.GetObj("Dimension");
            std::map<std::string, std::pair<int, int>>
                oMapDimensionNameToIndexAndSize;
            int i = 0;
            for (auto &oDimension : oDimensions.GetChildren())
            {
                if (oDimension.GetType() == CPLJSONObject::Type::Object)
                {
                    auto osDimensionName =
                        oDimension.GetString("DimensionName");
                    int nSize = oDimension.GetInteger("Size");
                    oMapDimensionNameToIndexAndSize[osDimensionName] =
                        std::make_pair(i, nSize);
                    ++i;
                }
            }

            for (auto &oDataField : oDataFields.GetChildren())
            {
                if (oDataField.GetType() == CPLJSONObject::Type::Object)
                {
                    auto osDataFieldName =
                        oDataField.GetString("DataFieldName");
                    auto oDimList = oDataField.GetArray("DimList");
                    GridMetadata oMetadata;
                    bool bValid = oDimList.Size() > 0;
                    for (int j = 0; j < oDimList.Size(); ++j)
                    {
                        const auto osDimensionName = oDimList[j].ToString();
                        const auto oIter = oMapDimensionNameToIndexAndSize.find(
                            osDimensionName.c_str());
                        if (oIter == oMapDimensionNameToIndexAndSize.end())
                        {
                            bValid = false;
                            break;
                        }
                        Dimension oDim;
                        oDim.osName = osDimensionName;
                        oDim.nDimIndex = oIter->second.first;
                        oDim.nSize = oIter->second.second;
                        oMetadata.aoDimensions.push_back(oDim);
                    }
                    if (bValid)
                    {
                        oMetadata.osProjection = oGrid.GetString("Projection");
                        oMetadata.osGridOrigin = oGrid.GetString("GridOrigin");

                        const auto oProjParams = oGrid.GetArray("ProjParams");
                        for (int j = 0; j < oProjParams.Size(); ++j)
                            oMetadata.adfProjParams.push_back(
                                oProjParams[j].ToDouble());

                        const auto oUpperLeftPointMtrs =
                            oGrid.GetArray("UpperLeftPointMtrs");
                        for (int j = 0; j < oUpperLeftPointMtrs.Size(); ++j)
                            oMetadata.adfUpperLeftPointMeters.push_back(
                                oUpperLeftPointMtrs[j].ToDouble());

                        const auto oLowerRightMtrs =
                            oGrid.GetArray("LowerRightMtrs");
                        for (int j = 0; j < oLowerRightMtrs.Size(); ++j)
                            oMetadata.adfLowerRightPointMeters.push_back(
                                oLowerRightMtrs[j].ToDouble());

                        m_oMapSubdatasetNameToMetadata["//HDFEOS/GRIDS/" +
                                                       osGridName +
                                                       "/Data_Fields/" +
                                                       osDataFieldName] =
                            oMetadata;
                    }
                }
            }
        }
    }

    return true;
}

/************************************************************************/
/*                           GetMetadata()                              */
/************************************************************************/

bool HDF5EOSParser::GetMetadata(const char *pszSubdatasetName,
                                GridMetadata &gridMetadataOut) const
{
    const auto oIter = m_oMapSubdatasetNameToMetadata.find(pszSubdatasetName);
    if (oIter == m_oMapSubdatasetNameToMetadata.end())
        return false;
    gridMetadataOut = oIter->second;
    return true;
}

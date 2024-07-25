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
#ifdef DEBUG
    CPLDebug("HDF5EOS", "%s", osResult.c_str());
#endif
    if (!oKWHandler.Parse(osResult.c_str()))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot parse HDFEOS INFORMATION.StructMetadata.0 with "
                 "NASAKeywordHandler");
        return false;
    }

    auto oJsonRoot = oKWHandler.GetJsonObject();
    auto oGridStructure = oJsonRoot.GetObj("GridStructure");
    auto oSwathStructure = oJsonRoot.GetObj("SwathStructure");
    bool bOK = false;
    // An empty
    // GROUP=GridStructure
    // END_GROUP=GridStructure
    // will generate 2 keys (_type and END_GROUP)
    if (oGridStructure.IsValid() && oGridStructure.GetChildren().size() > 2)
    {
        bOK = true;
        m_eDataModel = DataModel::GRID;
        ParseGridStructure(oGridStructure);
    }
    else if (oSwathStructure.IsValid() &&
             oSwathStructure.GetChildren().size() > 2)
    {
        bOK = true;
        m_eDataModel = DataModel::SWATH;
        ParseSwathStructure(oSwathStructure);
    }

    return bOK;
}

/************************************************************************/
/*                     GetGTCPProjectionCode()                          */
/************************************************************************/

static int GetGTCPProjectionCode(const std::string &osProjection)
{
    const char *const apszGCTPProjections[] = {
        "HE5_GCTP_GEO",    "HE5_GCTP_UTM",    "HE5_GCTP_SPCS",
        "HE5_GCTP_ALBERS", "HE5_GCTP_LAMCC",  "HE5_GCTP_MERCAT",
        "HE5_GCTP_PS",     "HE5_GCTP_POLYC",  "HE5_GCTP_EQUIDC",
        "HE5_GCTP_TM",     "HE5_GCTP_STEREO", "HE5_GCTP_LAMAZ",
        "HE5_GCTP_AZMEQD", "HE5_GCTP_GNOMON", "HE5_GCTP_ORTHO",
        "HE5_GCTP_GVNSP",  "HE5_GCTP_SNSOID", "HE5_GCTP_EQRECT",
        "HE5_GCTP_MILLER", "HE5_GCTP_VGRINT", "HE5_GCTP_HOM",
        "HE5_GCTP_ROBIN",  "HE5_GCTP_SOM",    "HE5_GCTP_ALASKA",
        "HE5_GCTP_GOOD",   "HE5_GCTP_MOLL",   "HE5_GCTP_IMOLL",
        "HE5_GCTP_HAMMER", "HE5_GCTP_WAGIV",  "HE5_GCTP_WAGVII",
        "HE5_GCTP_OBLEQA"};
    // HE5_GCTP_CEA, HE5_GCTP_BCEA, HE5_GCTP_ISINUS not taken
    // into account.
    for (int i = 0; i < static_cast<int>(CPL_ARRAYSIZE(apszGCTPProjections));
         ++i)
    {
        if (osProjection == apszGCTPProjections[i])
        {
            return i;
        }
    }
    return -1;
}

/************************************************************************/
/*                        ParseGridStructure()                          */
/************************************************************************/

void HDF5EOSParser::ParseGridStructure(const CPLJSONObject &oGridStructure)
{
    for (const auto &oGrid : oGridStructure.GetChildren())
    {
        if (oGrid.GetType() == CPLJSONObject::Type::Object)
        {
            const auto osGridName = oGrid.GetString("GridName");
            const auto oDataFields = oGrid.GetObj("DataField");
            const auto oDimensions = oGrid.GetObj("Dimension");
            std::map<std::string, int> oMapDimensionNameToSize;
            auto poGridMetadata = std::make_unique<GridMetadata>();
            poGridMetadata->osGridName = osGridName;
            for (const auto &oDimension : oDimensions.GetChildren())
            {
                if (oDimension.GetType() == CPLJSONObject::Type::Object)
                {
                    const auto osDimensionName =
                        oDimension.GetString("DimensionName");
                    int nSize = oDimension.GetInteger("Size");
                    oMapDimensionNameToSize[osDimensionName] = nSize;
                    Dimension oDim;
                    oDim.osName = osDimensionName;
                    oDim.nSize = nSize;
                    poGridMetadata->aoDimensions.push_back(oDim);
                }
            }

            // Happens for example for products following
            // AMSR-E/AMSR2 Unified L3 Daily 12.5 km Brightness Temperatures,
            // Sea Ice Concentration, Motion & Snow Depth Polar Grids
            // (https://nsidc.org/sites/default/files/au_si12-v001-userguide_1.pdf)
            // such as
            // https://n5eil01u.ecs.nsidc.org/AMSA/AU_SI12.001/2012.07.02/AMSR_U2_L3_SeaIce12km_B04_20120702.he5
            const int nXDim = oGrid.GetInteger("XDim", 0);
            const int nYDim = oGrid.GetInteger("YDim", 0);
            if (poGridMetadata->aoDimensions.empty() && nXDim > 0 && nYDim > 0)
            {
                // Check that all data fields have a DimList=(YDim,XDim)
                // property. This may be unneeded, but at least if we meet
                // this condition, that should be a strong hint that the first
                // dimension is Y, and the second X.
                bool bDimListIsYDimXDim = true;
                for (const auto &oDataField : oDataFields.GetChildren())
                {
                    if (oDataField.GetType() == CPLJSONObject::Type::Object)
                    {
                        const auto oDimList = oDataField.GetArray("DimList");
                        if (!(oDimList.Size() == 2 &&
                              oDimList[0].ToString() == "YDim" &&
                              oDimList[1].ToString() == "XDim"))
                        {
                            bDimListIsYDimXDim = false;
                            break;
                        }
                    }
                }
                if (bDimListIsYDimXDim)
                {
                    {
                        const std::string osDimensionName("YDim");
                        oMapDimensionNameToSize[osDimensionName] = nYDim;
                        Dimension oDim;
                        oDim.osName = osDimensionName;
                        oDim.nSize = nYDim;
                        poGridMetadata->aoDimensions.push_back(oDim);
                    }
                    {
                        const std::string osDimensionName("XDim");
                        oMapDimensionNameToSize[osDimensionName] = nXDim;
                        Dimension oDim;
                        oDim.osName = osDimensionName;
                        oDim.nSize = nXDim;
                        poGridMetadata->aoDimensions.push_back(oDim);
                    }
                }
            }

            poGridMetadata->osProjection = oGrid.GetString("Projection");
            poGridMetadata->nProjCode =
                GetGTCPProjectionCode(poGridMetadata->osProjection);
            poGridMetadata->osGridOrigin = oGrid.GetString("GridOrigin");
            poGridMetadata->nZone = oGrid.GetInteger("ZoneCode", -1);
            poGridMetadata->nSphereCode = oGrid.GetInteger("SphereCode", -1);

            const auto oProjParams = oGrid.GetArray("ProjParams");
            for (int j = 0; j < oProjParams.Size(); ++j)
                poGridMetadata->adfProjParams.push_back(
                    oProjParams[j].ToDouble());

            const auto oUpperLeftPointMtrs =
                oGrid.GetArray("UpperLeftPointMtrs");
            for (int j = 0; j < oUpperLeftPointMtrs.Size(); ++j)
                poGridMetadata->adfUpperLeftPointMeters.push_back(
                    oUpperLeftPointMtrs[j].ToDouble());

            const auto oLowerRightMtrs = oGrid.GetArray("LowerRightMtrs");
            for (int j = 0; j < oLowerRightMtrs.Size(); ++j)
                poGridMetadata->adfLowerRightPointMeters.push_back(
                    oLowerRightMtrs[j].ToDouble());

            m_oMapGridNameToGridMetadata[osGridName] =
                std::move(poGridMetadata);
            const auto poGridMetadataRef =
                m_oMapGridNameToGridMetadata[osGridName].get();

            for (const auto &oDataField : oDataFields.GetChildren())
            {
                if (oDataField.GetType() == CPLJSONObject::Type::Object)
                {
                    const auto osDataFieldName =
                        oDataField.GetString("DataFieldName");
                    const auto oDimList = oDataField.GetArray("DimList");
                    GridDataFieldMetadata oDataFieldMetadata;
                    bool bValid = oDimList.Size() > 0;
                    for (int j = 0; j < oDimList.Size(); ++j)
                    {
                        const auto osDimensionName = oDimList[j].ToString();
                        const auto oIter = oMapDimensionNameToSize.find(
                            osDimensionName.c_str());
                        if (oIter == oMapDimensionNameToSize.end())
                        {
                            bValid = false;
                            break;
                        }
                        Dimension oDim;
                        oDim.osName = osDimensionName;
                        oDim.nSize = oIter->second;
                        oDataFieldMetadata.aoDimensions.push_back(oDim);
                    }
                    if (bValid)
                    {
                        oDataFieldMetadata.poGridMetadata = poGridMetadataRef;
                        m_oMapSubdatasetNameToGridDataFieldMetadata
                            ["//HDFEOS/GRIDS/" + osGridName + "/Data_Fields/" +
                             osDataFieldName] = std::move(oDataFieldMetadata);
                    }
                }
            }
        }
    }
}

/************************************************************************/
/*                        GetGridMetadata()                             */
/************************************************************************/

bool HDF5EOSParser::GetGridMetadata(const std::string &osGridName,
                                    GridMetadata &gridMetadataOut) const
{
    const auto oIter = m_oMapGridNameToGridMetadata.find(osGridName);
    if (oIter == m_oMapGridNameToGridMetadata.end())
        return false;
    gridMetadataOut = *(oIter->second);
    return true;
}

/************************************************************************/
/*                     GetGridDataFieldMetadata()                       */
/************************************************************************/

bool HDF5EOSParser::GetGridDataFieldMetadata(
    const char *pszSubdatasetName,
    GridDataFieldMetadata &gridDataFieldMetadataOut) const
{
    const auto oIter =
        m_oMapSubdatasetNameToGridDataFieldMetadata.find(pszSubdatasetName);
    if (oIter == m_oMapSubdatasetNameToGridDataFieldMetadata.end())
        return false;
    gridDataFieldMetadataOut = oIter->second;
    return true;
}

/************************************************************************/
/*                        ParseSwathStructure()                         */
/************************************************************************/

void HDF5EOSParser::ParseSwathStructure(const CPLJSONObject &oSwathStructure)
{
    for (const auto &oSwath : oSwathStructure.GetChildren())
    {
        if (oSwath.GetType() == CPLJSONObject::Type::Object)
        {
            const auto osSwathName = oSwath.GetString("SwathName");

            const auto oDimensions = oSwath.GetObj("Dimension");
            std::map<std::string, int> oMapDimensionNameToSize;
            auto poSwathMetadata = std::make_unique<SwathMetadata>();
            poSwathMetadata->osSwathName = osSwathName;
            for (const auto &oDimension : oDimensions.GetChildren())
            {
                if (oDimension.GetType() == CPLJSONObject::Type::Object)
                {
                    auto osDimensionName =
                        oDimension.GetString("DimensionName");
                    int nSize = oDimension.GetInteger("Size");
                    oMapDimensionNameToSize[osDimensionName] = nSize;
                    Dimension oDim;
                    oDim.osName = std::move(osDimensionName);
                    oDim.nSize = nSize;
                    poSwathMetadata->aoDimensions.emplace_back(std::move(oDim));
                }
            }

            m_oMapSwathNameToSwathMetadata[osSwathName] =
                std::move(poSwathMetadata);
            const auto poSwathMetadataRef =
                m_oMapSwathNameToSwathMetadata[osSwathName].get();

            struct DimensionMap
            {
                std::string osGeoDimName;
                std::string osDataDimName;
                int nOffset = 0;
                int nIncrement = 1;
            };

            std::vector<DimensionMap> aoDimensionMaps;
            std::map<std::string, std::string> oMapDataDimensionToGeoDimension;

            const auto jsonDimensionMaps = oSwath.GetObj("DimensionMap");
            for (const auto &jsonDimensionMap : jsonDimensionMaps.GetChildren())
            {
                if (jsonDimensionMap.GetType() == CPLJSONObject::Type::Object)
                {
                    DimensionMap oDimensionMap;
                    oDimensionMap.osGeoDimName =
                        jsonDimensionMap.GetString("GeoDimension");
                    oDimensionMap.osDataDimName =
                        jsonDimensionMap.GetString("DataDimension");
                    oDimensionMap.nOffset =
                        jsonDimensionMap.GetInteger("Offset", 0);
                    oDimensionMap.nIncrement =
                        jsonDimensionMap.GetInteger("Increment", 1);
                    oMapDataDimensionToGeoDimension[oDimensionMap
                                                        .osDataDimName] =
                        oDimensionMap.osGeoDimName;
                    aoDimensionMaps.emplace_back(oDimensionMap);
                }
            }

            const auto oGeoFields = oSwath.GetObj("GeoField");
            std::vector<Dimension> aoLongitudeDimensions;
            std::vector<Dimension> aoLatitudeDimensions;
            for (const auto &oGeoField : oGeoFields.GetChildren())
            {
                if (oGeoField.GetType() == CPLJSONObject::Type::Object)
                {
                    auto osGeoFieldName = oGeoField.GetString("GeoFieldName");
                    auto oDimList = oGeoField.GetArray("DimList");
                    bool bValid = true;
                    std::vector<Dimension> aoDimensions;
                    for (int j = 0; j < oDimList.Size(); ++j)
                    {
                        const auto osDimensionName = oDimList[j].ToString();
                        const auto oIter = oMapDimensionNameToSize.find(
                            osDimensionName.c_str());
                        if (oIter == oMapDimensionNameToSize.end())
                        {
                            bValid = false;
                            break;
                        }
                        Dimension oDim;
                        oDim.osName = osDimensionName;
                        oDim.nSize = oIter->second;
                        aoDimensions.push_back(oDim);
                        if (oMapDataDimensionToGeoDimension.find(
                                osDimensionName) ==
                            oMapDataDimensionToGeoDimension.end())
                        {
                            // Create a fake dimension map for this dim
                            DimensionMap oDimensionMap;
                            oDimensionMap.osGeoDimName = osDimensionName;
                            oDimensionMap.osDataDimName = osDimensionName;
                            oDimensionMap.nOffset = 0;
                            oDimensionMap.nIncrement = 1;
                            oMapDataDimensionToGeoDimension[osDimensionName] =
                                osDimensionName;
                            aoDimensionMaps.emplace_back(oDimensionMap);
                        }
                    }
                    if (bValid)
                    {
                        SwathGeolocationFieldMetadata oMetadata;
                        oMetadata.poSwathMetadata = poSwathMetadataRef;

                        if (osGeoFieldName == "Longitude")
                            aoLongitudeDimensions = aoDimensions;
                        else if (osGeoFieldName == "Latitude")
                            aoLatitudeDimensions = aoDimensions;

                        oMetadata.aoDimensions = std::move(aoDimensions);

                        const std::string osSubdatasetName =
                            "//HDFEOS/SWATHS/" + osSwathName +
                            "/Geolocation_Fields/" + osGeoFieldName;
                        m_oMapSubdatasetNameToSwathGeolocationFieldMetadata
                            [osSubdatasetName] = std::move(oMetadata);
                    }
                }
            }

            const auto oDataFields = oSwath.GetObj("DataField");
            for (const auto &oDataField : oDataFields.GetChildren())
            {
                if (oDataField.GetType() == CPLJSONObject::Type::Object)
                {
                    const auto osDataFieldName =
                        oDataField.GetString("DataFieldName");
                    const auto oDimList = oDataField.GetArray("DimList");
                    SwathDataFieldMetadata oMetadata;
                    oMetadata.poSwathMetadata = poSwathMetadataRef;
                    bool bValid = oDimList.Size() > 0;
                    for (int j = 0; j < oDimList.Size(); ++j)
                    {
                        const auto osDimensionName = oDimList[j].ToString();
                        const auto oIter = oMapDimensionNameToSize.find(
                            osDimensionName.c_str());
                        if (oIter == oMapDimensionNameToSize.end())
                        {
                            bValid = false;
                            break;
                        }
                        Dimension oDim;
                        oDim.osName = osDimensionName;
                        oDim.nSize = oIter->second;
                        oMetadata.aoDimensions.push_back(oDim);
                    }
                    if (bValid)
                    {
                        if (oMetadata.aoDimensions.size() >= 2 &&
                            aoLongitudeDimensions.size() == 2 &&
                            aoLongitudeDimensions == aoLatitudeDimensions)
                        {
                            int i = 0;
                            std::string osDataXDimName;
                            std::string osDataYDimName;
                            for (const auto &oDimSwath : oMetadata.aoDimensions)
                            {
                                auto oIter =
                                    oMapDataDimensionToGeoDimension.find(
                                        oDimSwath.osName);
                                if (oIter !=
                                    oMapDataDimensionToGeoDimension.end())
                                {
                                    const auto &osGeoDimName = oIter->second;
                                    if (osGeoDimName ==
                                        aoLongitudeDimensions[0].osName)
                                    {
                                        osDataYDimName = oDimSwath.osName;
                                        oMetadata.iYDim = i;
                                    }
                                    else if (osGeoDimName ==
                                             aoLongitudeDimensions[1].osName)
                                    {
                                        osDataXDimName = oDimSwath.osName;
                                        oMetadata.iXDim = i;
                                    }
                                }
                                else
                                {
                                    oMetadata.iOtherDim = i;
                                }
                                ++i;
                            }
                            if (oMetadata.iXDim >= 0 && oMetadata.iYDim >= 0)
                            {
                                oMetadata.osLongitudeSubdataset =
                                    "//HDFEOS/SWATHS/" + osSwathName +
                                    "/Geolocation_Fields/Longitude";
                                oMetadata.osLatitudeSubdataset =
                                    "//HDFEOS/SWATHS/" + osSwathName +
                                    "/Geolocation_Fields/Latitude";

                                for (const auto &oDimMap : aoDimensionMaps)
                                {
                                    if (oDimMap.osDataDimName == osDataYDimName)
                                    {
                                        oMetadata.nLineOffset = oDimMap.nOffset;
                                        oMetadata.nLineStep =
                                            oDimMap.nIncrement;
                                    }
                                    else if (oDimMap.osDataDimName ==
                                             osDataXDimName)
                                    {
                                        oMetadata.nPixelOffset =
                                            oDimMap.nOffset;
                                        oMetadata.nPixelStep =
                                            oDimMap.nIncrement;
                                    }
                                }
                            }
                        }

                        m_oMapSubdatasetNameToSwathDataFieldMetadata
                            ["//HDFEOS/SWATHS/" + osSwathName +
                             "/Data_Fields/" + osDataFieldName] =
                                std::move(oMetadata);
                    }
                }
            }
        }
    }
}

/************************************************************************/
/*                        GetSwathMetadata()                            */
/************************************************************************/

bool HDF5EOSParser::GetSwathMetadata(const std::string &osSwathName,
                                     SwathMetadata &swathMetadataOut) const
{
    const auto oIter = m_oMapSwathNameToSwathMetadata.find(osSwathName);
    if (oIter == m_oMapSwathNameToSwathMetadata.end())
        return false;
    swathMetadataOut = *(oIter->second.get());
    return true;
}

/************************************************************************/
/*                      GetSwathDataFieldMetadata()                     */
/************************************************************************/

bool HDF5EOSParser::GetSwathDataFieldMetadata(
    const char *pszSubdatasetName,
    SwathDataFieldMetadata &swathDataFieldMetadataOut) const
{
    const auto oIter =
        m_oMapSubdatasetNameToSwathDataFieldMetadata.find(pszSubdatasetName);
    if (oIter == m_oMapSubdatasetNameToSwathDataFieldMetadata.end())
        return false;
    swathDataFieldMetadataOut = oIter->second;
    return true;
}

/************************************************************************/
/*                    GetSwathGeolocationFieldMetadata()                */
/************************************************************************/

bool HDF5EOSParser::GetSwathGeolocationFieldMetadata(
    const char *pszSubdatasetName,
    SwathGeolocationFieldMetadata &swathGeolocationFieldMetadataOut) const
{
    const auto oIter = m_oMapSubdatasetNameToSwathGeolocationFieldMetadata.find(
        pszSubdatasetName);
    if (oIter == m_oMapSubdatasetNameToSwathGeolocationFieldMetadata.end())
        return false;
    swathGeolocationFieldMetadataOut = oIter->second;
    return true;
}

/************************************************************************/
/*                        GetGeoTransform()                             */
/************************************************************************/

bool HDF5EOSParser::GridMetadata::GetGeoTransform(
    double adfGeoTransform[6]) const
{
    if (nProjCode >= 0 && osGridOrigin == "HE5_HDFE_GD_UL" &&
        adfUpperLeftPointMeters.size() == 2 &&
        adfLowerRightPointMeters.size() == 2)
    {
        int nRasterXSize = 0;
        int nRasterYSize = 0;

        for (const auto &oDim : aoDimensions)
        {
            if (oDim.osName == "XDim")
                nRasterXSize = oDim.nSize;
            else if (oDim.osName == "YDim")
                nRasterYSize = oDim.nSize;
        }
        if (nRasterXSize <= 0 || nRasterYSize <= 0)
            return false;
        if (nProjCode == 0)  // GEO
        {
            adfGeoTransform[0] = CPLPackedDMSToDec(adfUpperLeftPointMeters[0]);
            adfGeoTransform[1] =
                (CPLPackedDMSToDec(adfLowerRightPointMeters[0]) -
                 CPLPackedDMSToDec(adfUpperLeftPointMeters[0])) /
                nRasterXSize;
            adfGeoTransform[2] = 0;
            adfGeoTransform[3] = CPLPackedDMSToDec(adfUpperLeftPointMeters[1]);
            adfGeoTransform[4] = 0;
            adfGeoTransform[5] =
                (CPLPackedDMSToDec(adfLowerRightPointMeters[1]) -
                 CPLPackedDMSToDec(adfUpperLeftPointMeters[1])) /
                nRasterYSize;
        }
        else
        {
            adfGeoTransform[0] = adfUpperLeftPointMeters[0];
            adfGeoTransform[1] =
                (adfLowerRightPointMeters[0] - adfUpperLeftPointMeters[0]) /
                nRasterXSize;
            adfGeoTransform[2] = 0;
            adfGeoTransform[3] = adfUpperLeftPointMeters[1];
            adfGeoTransform[4] = 0;
            adfGeoTransform[5] =
                (adfLowerRightPointMeters[1] - adfUpperLeftPointMeters[1]) /
                nRasterYSize;
        }
        return true;
    }
    return false;
}

/************************************************************************/
/*                              GetSRS()                                */
/************************************************************************/

std::unique_ptr<OGRSpatialReference> HDF5EOSParser::GridMetadata::GetSRS() const
{
    std::vector<double> l_adfProjParams = adfProjParams;
    l_adfProjParams.resize(15);
    auto poSRS = std::make_unique<OGRSpatialReference>();
    poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    if (poSRS->importFromUSGS(nProjCode, nZone, l_adfProjParams.data(),
                              nSphereCode) == OGRERR_NONE)
    {
        return poSRS;
    }
    return nullptr;
}

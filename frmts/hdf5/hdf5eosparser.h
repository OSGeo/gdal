/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  Header file for HDF5 HDFEOS parser
 * Author:   Even Rouault
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef HDF5EOSPARSER_H_INCLUDED
#define HDF5EOSPARSER_H_INCLUDED

#include "hdf5_api.h"

#include "cpl_json.h"
#include "gdal_priv.h"
#include "ogr_spatialref.h"

#include <map>
#include <string>
#include <vector>

/************************************************************************/
/*                             HDF5EOSParser                            */
/************************************************************************/

class HDF5EOSParser
{
  public:
    HDF5EOSParser() = default;

    enum class DataModel
    {
        INVALID,
        GRID,
        SWATH,
    };

    struct Dimension
    {
        std::string osName{};
        int nSize = 0;

        inline bool operator==(const Dimension &otherDim) const
        {
            return osName == otherDim.osName;
        }
    };

    struct GridMetadata
    {
        std::string osGridName{};
        std::vector<Dimension> aoDimensions{};  // all dimensions of the grid
        std::string osProjection{};             // e.g HE5_GCTP_SNSOID
        int nProjCode = -1;          // GTCP numeric value for osProjection
        std::string osGridOrigin{};  // e.g HE5_HDFE_GD_UL
        std::vector<double>
            adfProjParams{};  // e.g (6371007.181000,0,0,0,0,0,0,0,0,0,0,0,0)
        int nZone = 0;        // for HE5_GCTP_UTM and HE5_GCTP_SPCS
        int nSphereCode = 0;
        std::vector<double>
            adfUpperLeftPointMeters{};  // e.g (-1111950.519667,5559752.598333)
        std::vector<double>
            adfLowerRightPointMeters{};  // e.g (0.000000,4447802.078667)

        bool GetGeoTransform(GDALGeoTransform &gt) const;
        std::unique_ptr<OGRSpatialReference> GetSRS() const;
    };

    struct GridDataFieldMetadata
    {
        std::vector<Dimension> aoDimensions{};  // dimensions of the data field
        const GridMetadata *poGridMetadata = nullptr;
    };

    struct SwathMetadata
    {
        std::string osSwathName{};
        std::vector<Dimension> aoDimensions{};  // all dimensions of the swath
    };

    struct SwathFieldMetadata
    {
        std::vector<Dimension>
            aoDimensions{};  // dimensions of the geolocation/data field
        const SwathMetadata *poSwathMetadata = nullptr;

        int iXDim = -1;
        int iYDim = -1;
        int iOtherDim = -1;

        std::string osLongitudeSubdataset{};
        std::string osLatitudeSubdataset{};
        int nLineOffset = 0;
        int nLineStep = 0;
        int nPixelOffset = 0;
        int nPixelStep = 0;
    };

    static bool HasHDFEOS(hid_t hRoot);
    bool Parse(hid_t hRoot);

    DataModel GetDataModel() const
    {
        return m_eDataModel;
    }

    bool GetGridMetadata(const std::string &osGridName,
                         GridMetadata &gridMetadataOut) const;
    bool GetGridDataFieldMetadata(
        const char *pszSubdatasetName,
        GridDataFieldMetadata &gridDataFieldMetadataOut) const;
    bool GetSwathMetadata(const std::string &osSwathName,
                          SwathMetadata &swathMetadataOut) const;
    bool GetSwathFieldMetadata(const char *pszSubdatasetName,
                               SwathFieldMetadata &swathFieldMetadataOut) const;

  private:
    DataModel m_eDataModel = DataModel::INVALID;
    std::map<std::string, std::unique_ptr<GridMetadata>>
        m_oMapGridNameToGridMetadata{};
    std::map<std::string, GridDataFieldMetadata>
        m_oMapSubdatasetNameToGridDataFieldMetadata{};
    std::map<std::string, std::unique_ptr<SwathMetadata>>
        m_oMapSwathNameToSwathMetadata{};
    std::map<std::string, SwathFieldMetadata>
        m_oMapSubdatasetNameToSwathFieldMetadata{};

    void ParseGridStructure(const CPLJSONObject &oGridStructure);
    void ParseSwathStructure(const CPLJSONObject &oSwathStructure);
};

#endif  // HDF5EOSPARSER_H_INCLUDED

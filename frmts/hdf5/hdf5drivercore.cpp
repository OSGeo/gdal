/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  HDF5 driver
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault, <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "hdf5drivercore.h"

#include "gdal_frmts.h"
#include "gdalplugindriverproxy.h"
#include "gdalsubdatasetinfo.h"

#include <algorithm>
#include <cctype>

/************************************************************************/
/*                        HDF5DatasetIdentify()                         */
/************************************************************************/

int HDF5DatasetIdentify(GDALOpenInfo *poOpenInfo)

{
    if ((poOpenInfo->nOpenFlags & GDAL_OF_MULTIDIM_RASTER) &&
        STARTS_WITH(poOpenInfo->pszFilename, "HDF5:"))
    {
        return TRUE;
    }

    // Is it an HDF5 file?
    constexpr char achSignature[] = "\211HDF\r\n\032\n";

    if (!poOpenInfo->pabyHeader)
        return FALSE;

    const CPLString osExt(poOpenInfo->osExtension);

    const auto IsRecognizedByNetCDFDriver = [&osExt, poOpenInfo]()
    {
        if ((EQUAL(osExt, "NC") || EQUAL(osExt, "CDF") || EQUAL(osExt, "NC4") ||
             EQUAL(osExt, "gmac")) &&
            GDALGetDriverByName("netCDF") != nullptr)
        {
            const char *const apszAllowedDriver[] = {"netCDF", nullptr};
            CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);
            return std::unique_ptr<GDALDataset>(GDALDataset::Open(
                       poOpenInfo->pszFilename,
                       GDAL_OF_RASTER | GDAL_OF_MULTIDIM_RASTER |
                           GDAL_OF_VECTOR,
                       apszAllowedDriver, nullptr, nullptr)) != nullptr;
        }
        return false;
    };

    if (memcmp(poOpenInfo->pabyHeader, achSignature, 8) == 0 ||
        (poOpenInfo->nHeaderBytes > 512 + 8 &&
         memcmp(poOpenInfo->pabyHeader + 512, achSignature, 8) == 0))
    {
        if (poOpenInfo->IsSingleAllowedDriver("HDF5"))
        {
            return TRUE;
        }

        // The tests to avoid opening KEA and BAG drivers are not
        // necessary when drivers are built in the core lib, as they
        // are registered after HDF5, but in the case of plugins, we
        // cannot do assumptions about the registration order.

        // Avoid opening kea files if the kea driver is available.
        if (EQUAL(osExt, "KEA") && GDALGetDriverByName("KEA") != nullptr)
        {
            return FALSE;
        }

        // Avoid opening BAG files if the bag driver is available.
        if (EQUAL(osExt, "BAG") && GDALGetDriverByName("BAG") != nullptr)
        {
            return FALSE;
        }

        // Avoid opening NC files if the netCDF driver is available and
        // they are recognized by it.
        if (IsRecognizedByNetCDFDriver())
        {
            return FALSE;
        }

        return TRUE;
    }

    if (memcmp(poOpenInfo->pabyHeader, "<HDF_UserBlock>", 15) == 0)
    {
        return TRUE;
    }

    // The HDF5 signature can be at offsets 512, 1024, 2048, etc.
    if (poOpenInfo->fpL != nullptr &&
        (EQUAL(osExt, "h5") || EQUAL(osExt, "hdf5") || EQUAL(osExt, "nc") ||
         EQUAL(osExt, "cdf") || EQUAL(osExt, "nc4") ||
         poOpenInfo->IsSingleAllowedDriver("HDF5")))
    {
        vsi_l_offset nOffset = 512;
        for (int i = 0; i < 64; i++)
        {
            GByte abyBuf[8];
            if (VSIFSeekL(poOpenInfo->fpL, nOffset, SEEK_SET) != 0 ||
                VSIFReadL(abyBuf, 1, 8, poOpenInfo->fpL) != 8)
            {
                break;
            }
            if (memcmp(abyBuf, achSignature, 8) == 0)
            {
                if (poOpenInfo->IsSingleAllowedDriver("HDF5"))
                {
                    return TRUE;
                }
                // Avoid opening NC files if the netCDF driver is available and
                // they are recognized by it.
                if (IsRecognizedByNetCDFDriver())
                {
                    return FALSE;
                }

                return TRUE;
            }
            nOffset *= 2;
        }
    }

    return FALSE;
}

/************************************************************************/
/*                      HDF5ImageDatasetIdentify()                      */
/************************************************************************/

int HDF5ImageDatasetIdentify(GDALOpenInfo *poOpenInfo)

{
    if (!STARTS_WITH_CI(poOpenInfo->pszFilename, "HDF5:"))
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                    HDF5DriverGetSubdatasetInfo()                     */
/************************************************************************/

struct HDF5DriverSubdatasetInfo final : public GDALSubdatasetInfo
{
  public:
    explicit HDF5DriverSubdatasetInfo(const std::string &fileName)
        : GDALSubdatasetInfo(fileName)
    {
    }

    // GDALSubdatasetInfo interface
  private:
    void parseFileName() override;
};

void HDF5DriverSubdatasetInfo::parseFileName()
{

    if (!STARTS_WITH_CI(m_fileName.c_str(), "HDF5:"))
    {
        return;
    }

    CPLStringList aosParts{CSLTokenizeString2(m_fileName.c_str(), ":", 0)};
    const int iPartsCount{CSLCount(aosParts)};

    if (iPartsCount >= 3)
    {

        m_driverPrefixComponent = aosParts[0];

        std::string part1{aosParts[1]};
        if (!part1.empty() && part1[0] == '"')
        {
            part1 = part1.substr(1);
        }

        int subdatasetIndex{2};
        const bool hasDriveLetter{
            part1.length() == 1 &&
            std::isalpha(static_cast<unsigned char>(part1.at(0))) &&
            (strlen(aosParts[2]) > 1 &&
             (aosParts[2][0] == '\\' ||
              (aosParts[2][0] == '/' && aosParts[2][1] != '/')))};

        const bool hasProtocol{part1 == "/vsicurl/http" ||
                               part1 == "/vsicurl/https" ||
                               part1 == "/vsicurl_streaming/http" ||
                               part1 == "/vsicurl_streaming/https"};

        m_pathComponent = aosParts[1];

        if (hasDriveLetter || hasProtocol)
        {
            m_pathComponent.append(":");
            m_pathComponent.append(aosParts[2]);
            subdatasetIndex++;
        }

        if (iPartsCount > subdatasetIndex)
        {
            m_subdatasetComponent = aosParts[subdatasetIndex];

            // Append any remaining part
            for (int i = subdatasetIndex + 1; i < iPartsCount; ++i)
            {
                m_subdatasetComponent.append(":");
                m_subdatasetComponent.append(aosParts[i]);
            }
        }
    }
}

static GDALSubdatasetInfo *HDF5DriverGetSubdatasetInfo(const char *pszFileName)
{
    if (STARTS_WITH_CI(pszFileName, "HDF5:"))
    {
        std::unique_ptr<GDALSubdatasetInfo> info =
            std::make_unique<HDF5DriverSubdatasetInfo>(pszFileName);
        if (!info->GetSubdatasetComponent().empty() &&
            !info->GetPathComponent().empty())
        {
            return info.release();
        }
    }
    return nullptr;
}

/************************************************************************/
/*                            IdentifySxx()                             */
/************************************************************************/

static bool IdentifySxx(GDALOpenInfo *poOpenInfo, const char *pszDriverName,
                        const char *pszConfigOption,
                        const char *pszMainGroupName)
{
    if (STARTS_WITH(poOpenInfo->pszFilename, pszDriverName) &&
        poOpenInfo->pszFilename[strlen(pszDriverName)] == ':')
        return TRUE;

    // Is it an HDF5 file?
    static const char achSignature[] = "\211HDF\r\n\032\n";

    if (poOpenInfo->pabyHeader == nullptr ||
        memcmp(poOpenInfo->pabyHeader, achSignature, 8) != 0)
        return FALSE;

    if (poOpenInfo->IsSingleAllowedDriver(pszDriverName))
    {
        return TRUE;
    }

    // GDAL_Sxxx_IDENTIFY can be set to NO only for tests, to test that
    // HDF5Dataset::Open() can redirect to Sxxx if the below logic fails
    if (CPLTestBool(CPLGetConfigOption(pszConfigOption, "YES")))
    {
        // The below identification logic may be a bit fragile...
        // Works at least on:
        // - /vsis3/noaa-s102-pds/ed2.1.0/national_bathymetric_source/boston/dcf2/tiles/102US00_US4MA1GC.h5
        // - https://datahub.admiralty.co.uk/portal/sharing/rest/content/items/6fd07bde26124d48820b6dee60695389/data (S-102_Liverpool_Trial_Cells.zip)
        const int nLenMainGroup = static_cast<int>(strlen(pszMainGroupName));
        const int nLenGroupF = static_cast<int>(strlen("Group_F"));
        const int nLenProductSpecification =
            static_cast<int>(strlen("productSpecification"));
        bool bFoundMainGroup = false;
        bool bFoundGroupF = false;
        bool bFoundProductSpecification = false;
        for (int iTry = 0; iTry < 2; ++iTry)
        {
            for (int i = 0; i <= poOpenInfo->nHeaderBytes - nLenGroupF; ++i)
            {
                if (i <= poOpenInfo->nHeaderBytes - nLenMainGroup &&
                    poOpenInfo->pabyHeader[i] == pszMainGroupName[0] &&
                    memcmp(poOpenInfo->pabyHeader + i, pszMainGroupName,
                           nLenMainGroup) == 0)
                {
                    bFoundMainGroup = true;
                    if (bFoundGroupF)
                        return true;
                }
                if (poOpenInfo->pabyHeader[i] == 'G' &&
                    memcmp(poOpenInfo->pabyHeader + i, "Group_F", nLenGroupF) ==
                        0)
                {
                    bFoundGroupF = true;
                    if (bFoundMainGroup)
                        return true;
                }
                if (i <= poOpenInfo->nHeaderBytes - nLenProductSpecification &&
                    poOpenInfo->pabyHeader[i] == 'p' &&
                    memcmp(poOpenInfo->pabyHeader + i, "productSpecification",
                           nLenProductSpecification) == 0)
                {
                    // For 102DE00OS08J.H5
                    bFoundProductSpecification = true;
                }
            }
            if (!(iTry == 0 && bFoundProductSpecification &&
                  poOpenInfo->nHeaderBytes == 1024 &&
                  poOpenInfo->TryToIngest(4096)))
            {
                break;
            }
        }
    }

    return false;
}

/************************************************************************/
/*                        S102DatasetIdentify()                         */
/************************************************************************/

int S102DatasetIdentify(GDALOpenInfo *poOpenInfo)

{
    return IdentifySxx(poOpenInfo, "S102", "GDAL_S102_IDENTIFY",
                       "BathymetryCoverage");
}

/************************************************************************/
/*                        S104DatasetIdentify()                         */
/************************************************************************/

int S104DatasetIdentify(GDALOpenInfo *poOpenInfo)

{
    return IdentifySxx(poOpenInfo, "S104", "GDAL_S104_IDENTIFY", "WaterLevel");
}

/************************************************************************/
/*                        S111DatasetIdentify()                         */
/************************************************************************/

int S111DatasetIdentify(GDALOpenInfo *poOpenInfo)

{
    return IdentifySxx(poOpenInfo, "S111", "GDAL_S111_IDENTIFY",
                       "SurfaceCurrent");
}

/************************************************************************/
/*                         BAGDatasetIdentify()                         */
/************************************************************************/

int BAGDatasetIdentify(GDALOpenInfo *poOpenInfo)

{
    if (STARTS_WITH(poOpenInfo->pszFilename, "BAG:"))
        return TRUE;

    // Is it an HDF5 file?
    static const char achSignature[] = "\211HDF\r\n\032\n";

    if (poOpenInfo->pabyHeader == nullptr ||
        memcmp(poOpenInfo->pabyHeader, achSignature, 8) != 0)
        return FALSE;

    // Does it have the extension .bag?
    if (!poOpenInfo->IsExtensionEqualToCI("bag"))
    {
        if (poOpenInfo->IsSingleAllowedDriver("BAG"))
        {
            return TRUE;
        }
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                    HDF5DriverSetCommonMetadata()                     */
/************************************************************************/

void HDF5DriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(HDF5_DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "Hierarchical Data Format Release 5");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/hdf5.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "h5 hdf5");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->SetMetadataItem(GDAL_DCAP_MULTIDIM_RASTER, "YES");

    poDriver->pfnIdentify = HDF5DatasetIdentify;
    poDriver->pfnGetSubdatasetInfoFunc = HDF5DriverGetSubdatasetInfo;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
}

/************************************************************************/
/*                  HDF5ImageDriverSetCommonMetadata()                  */
/************************************************************************/

void HDF5ImageDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(HDF5_IMAGE_DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "HDF5 Dataset");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/hdf5.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnIdentify = HDF5ImageDatasetIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
}

/************************************************************************/
/*                     BAGDriverSetCommonMetadata()                     */
/************************************************************************/

void BAGDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(BAG_DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Bathymetry Attributed Grid");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/bag.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "bag");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Float32");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "   <Option name='MODE' type='string-select' default='AUTO'>"
        "       <Value>AUTO</Value>"
        "       <Value>LOW_RES_GRID</Value>"
        "       <Value>LIST_SUPERGRIDS</Value>"
        "       <Value>RESAMPLED_GRID</Value>"
        "       <Value>INTERPOLATED</Value>"
        "   </Option>"
        "   <Option name='SUPERGRIDS_INDICES' type='string' description="
        "'Tuple(s) (y1,x1),(y2,x2),...  of supergrids, by indices, to expose "
        "as subdatasets'/>"
        "   <Option name='MINX' type='float' description='Minimum X value of "
        "area of interest'/>"
        "   <Option name='MINY' type='float' description='Minimum Y value of "
        "area of interest'/>"
        "   <Option name='MAXX' type='float' description='Maximum X value of "
        "area of interest'/>"
        "   <Option name='MAXY' type='float' description='Maximum Y value of "
        "area of interest'/>"
        "   <Option name='RESX' type='float' description="
        "'Horizontal resolution. Only used for "
        "MODE=RESAMPLED_GRID/INTERPOLATED'/>"
        "   <Option name='RESY' type='float' description="
        "'Vertical resolution (positive value). Only used for "
        "MODE=RESAMPLED_GRID/INTERPOLATED'/>"
        "   <Option name='RES_STRATEGY' type='string-select' description="
        "'Which strategy to apply to select the resampled grid resolution. "
        "Only used for MODE=RESAMPLED_GRID/INTERPOLATED' default='AUTO'>"
        "       <Value>AUTO</Value>"
        "       <Value>MIN</Value>"
        "       <Value>MAX</Value>"
        "       <Value>MEAN</Value>"
        "   </Option>"
        "   <Option name='RES_FILTER_MIN' type='float' description="
        "'Minimum resolution of supergrids to take into account (excluded "
        "bound). "
        "Only used for MODE=RESAMPLED_GRID, INTERPOLATED or LIST_SUPERGRIDS' "
        "default='0'/>"
        "   <Option name='RES_FILTER_MAX' type='float' description="
        "'Maximum resolution of supergrids to take into account (included "
        "bound). "
        "Only used for MODE=RESAMPLED_GRID, INTERPOLATED or LIST_SUPERGRIDS' "
        "default='inf'/>"
        "   <Option name='VALUE_POPULATION' type='string-select' description="
        "'Which value population strategy to apply to compute the resampled "
        "cell "
        "values. Only used for MODE=RESAMPLED_GRID' default='MAX'>"
        "       <Value>MIN</Value>"
        "       <Value>MAX</Value>"
        "       <Value>MEAN</Value>"
        "       <Value>COUNT</Value>"
        "   </Option>"
        "   <Option name='SUPERGRIDS_MASK' type='boolean' description="
        "'Whether the dataset should consist of a mask band indicating if a "
        "supergrid node matches each target pixel. Only used for "
        "MODE=RESAMPLED_GRID' default='NO'/>"
        "   <Option name='NODATA_VALUE' type='float' default='1000000'/>"
        "   <Option name='REPORT_VERTCRS' type='boolean' default='YES'/>"
        "</OpenOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "  <Option name='VAR_*' type='string' description="
        "'Value to substitute to a variable in the template'/>"
        "  <Option name='TEMPLATE' type='string' description="
        "'.xml template to use'/>"
        "  <Option name='BAG_VERSION' type='string' description="
        "'Version to write in the Bag Version attribute' default='1.6.2'/>"
        "  <Option name='COMPRESS' type='string-select' default='DEFLATE'>"
        "    <Value>NONE</Value>"
        "    <Value>DEFLATE</Value>"
        "  </Option>"
        "  <Option name='ZLEVEL' type='int' "
        "description='DEFLATE compression level 1-9' default='6' />"
        "  <Option name='BLOCK_SIZE' type='int' description='Chunk size' />"
        "</CreationOptionList>");

    poDriver->SetMetadataItem(GDAL_DCAP_MULTIDIM_RASTER, "YES");

    poDriver->pfnIdentify = BAGDatasetIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                    S102DriverSetCommonMetadata()                     */
/************************************************************************/

void S102DriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(S102_DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIDIM_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "S-102 Bathymetric Surface Product");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/s102.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "h5");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_SUBDATASETS, "YES");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "   <Option name='DEPTH_OR_ELEVATION' type='string-select' "
        "default='DEPTH'>"
        "       <Value>DEPTH</Value>"
        "       <Value>ELEVATION</Value>"
        "   </Option>"
        "   <Option name='NORTH_UP' type='boolean' default='YES' "
        "description='Whether the top line of the dataset should be the "
        "northern-most one'/>"
        "</OpenOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "  <Option name='VERTICAL_DATUM' type='string' description="
        "'Vertical datum abbreviation or code (required)'/>"
        "  <Option name='ISSUE_DATE' type='string' description="
        "'Issue date as YYYYMMDD'/>"
        "  <Option name='ISSUE_TIME' type='string' description="
        "'Issue time as hhmmssZ or hhmmss[+-]HHMM'/>"
        "  <Option name='HORIZONTAL_POSITION_UNCERTAINTY' type='float' "
        "description='Horizontal position uncertainty in meter'/>"
        "  <Option name='VERTICAL_UNCERTAINTY' type='float' "
        "description='Vertical uncertainty in meter'/>"
        "  <Option name='QUALITY_DATASET' type='string' description="
        "'Path to a dataset with the quality of bathymetric coverage'/>"
        "  <Option name='COMPRESS' type='string-select' default='DEFLATE'>"
        "    <Value>NONE</Value>"
        "    <Value>DEFLATE</Value>"
        "  </Option>"
        "  <Option name='ZLEVEL' type='int' "
        "description='DEFLATE compression level 1-9' default='6' />"
        "  <Option name='BLOCK_SIZE' type='int' description='Chunk size' />"
        "</CreationOptionList>");

    poDriver->pfnIdentify = S102DatasetIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                    S104DriverSetCommonMetadata()                     */
/************************************************************************/

void S104DriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(S104_DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIDIM_RASTER, "YES");
    poDriver->SetMetadataItem(
        GDAL_DMD_LONGNAME,
        "S-104 Water Level Information for Surface Navigation Product");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/s104.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "h5");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_SUBDATASETS, "YES");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "   <Option name='NORTH_UP' type='boolean' default='YES' "
        "description='Whether the top line of the dataset should be the "
        "northern-most one'/>"
        "</OpenOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "  <Option name='TIME_POINT' type='string' description="
        "'Timestamp as YYYYMMDDTHHMMSSZ format (required)'/>"
        "  <Option name='VERTICAL_DATUM' type='string' description="
        "'Vertical datum abbreviation or code (required)'/>"
        "  <Option name='VERTICAL_CS' type='string-select' description="
        "'Vertical coordinate system (required).'>"
        "    <Value alias='6498'>DEPTH</Value>"
        "    <Value alias='6499'>HEIGHT</Value>"
        "  </Option>"
        "  <Option name='WATER_LEVEL_TREND_THRESHOLD' type='float' description="
        "'Critical value used to determine steady water level trend (required)."
        "Units are meters/hour (m/hr)'/>"
        "  <Option name='DATA_DYNAMICITY' type='string-select' description="
        "'Classification of data according to the relationship between the "
        "time of its collection, generation, or calculation of generation "
        "parameters, in relation to the time of publication of the dataset "
        "(required).'>"
        "    <Value alias='1'>observation</Value>"
        "    <Value alias='2'>astronomicalPrediction</Value>"
        "    <Value alias='3'>analysisOrHybrid</Value>"
        "    <Value alias='5'>hydrodynamicForecast</Value>"
        "  </Option>"
        "  <Option name='DATASETS' type='string' description="
        "'Comma separated list of datasets at different timestamps.'/>"
        "  <Option name='DATASETS_TIME_POINT' type='string' description="
        "'Comma separated list of the time point value of each dataset of "
        "DATASETS.'/>"
        "  <Option name='GEOGRAPHIC_IDENTIFIER' type='string' description="
        "'Description, or location code from list agreed by data producers'/>"
        "  <Option name='ISSUE_DATE' type='string' description="
        "'Issue date as YYYYMMDD'/>"
        "  <Option name='ISSUE_TIME' type='string' description="
        "'Issue time as hhmmssZ or hhmmss[+-]HHMM'/>"
        "  <Option name='TREND_INTERVAL' type='integer' "
        "description='Interval, in minutes, over which trend at a a particular "
        "time is calculated'/>"
        "  <Option name='DATASET_DELIVERY_INTERVAL' type='string' description="
        "'Expected time interval between availability of successive datasets "
        "for time-varying data. Must be formatted as PnYnMnDTnHnMnS "
        "(ISO8601 duration)'/>"
        "  <Option name='TIME_RECORD_INTERVAL' type='integer' description="
        "'Interval in seconds between time records.'/>"
        "  <Option name='COMMON_POINT_RULE' type='string-select' description="
        "'Procedure used for evaluating the coverage at a position that falls "
        "on the boundary or in an area of overlap between geographic objects' "
        "default='all'>"
        "    <Value alias='1'>average</Value>"
        "    <Value alias='2'>low</Value>"
        "    <Value alias='3'>high</Value>"
        "    <Value alias='4'>all</Value>"
        "  </Option>"
        "  <Option name='UNCERTAINTY' type='float' "
        "description='Uncertainty of depth values in meter'/>"
        "  <Option name='HORIZONTAL_POSITION_UNCERTAINTY' type='float' "
        "description='Horizontal position uncertainty in meter'/>"
        "  <Option name='VERTICAL_UNCERTAINTY' type='float' "
        "description='Vertical uncertainty in meter'/>"
        "  <Option name='TIME_UNCERTAINTY' type='float' "
        "description='Time uncertainty in second'/>"
        "  <Option name='COMPRESS' type='string-select' default='DEFLATE'>"
        "    <Value>NONE</Value>"
        "    <Value>DEFLATE</Value>"
        "  </Option>"
        "  <Option name='ZLEVEL' type='int' "
        "description='DEFLATE compression level 1-9' default='6' />"
        "  <Option name='BLOCK_SIZE' type='int' description='Chunk size' />"
        "</CreationOptionList>");

    poDriver->pfnIdentify = S104DatasetIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                    S111DriverSetCommonMetadata()                     */
/************************************************************************/

void S111DriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(S111_DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIDIM_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "S-111 Surface Currents Product");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/s111.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "h5");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_SUBDATASETS, "YES");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "   <Option name='NORTH_UP' type='boolean' default='YES' "
        "description='Whether the top line of the dataset should be the "
        "northern-most one'/>"
        "</OpenOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "  <Option name='TIME_POINT' type='string' description="
        "'Timestamp as YYYYMMDDTHHMMSSZ format (required)'/>"
        "  <Option name='DEPTH_TYPE' type='string-select' description="
        "'Type of depth (required). When selecting heightOrDepth, "
        "the interpretation depends on the VERTICAL_CS value.'>"
        "    <Value alias='1'>heightOrDepth</Value>"
        "    <Value alias='2'>layerAverage</Value>"
        "  </Option>"
        "  <Option name='VERTICAL_DATUM' type='string' description="
        "'Vertical datum abbreviation or code (required if "
        "DEPTH_TYPE=heightOrDepth)'/>"
        "  <Option name='VERTICAL_CS' type='string-select' description="
        "'Vertical coordinate system (required if DEPTH_TYPE=heightOrDepth).'>"
        "    <Value alias='6498'>DEPTH</Value>"
        "    <Value alias='6499'>HEIGHT</Value>"
        "  </Option>"
        "  <Option name='SURFACE_CURRENT_DEPTH' type='float' description="
        "'Depth/height value or layer thickness (m) (required)'/>"
        "  <Option name='DATA_DYNAMICITY' type='string-select' description="
        "'Classification of data according to the relationship between the "
        "time of its collection, generation, or calculation of generation "
        "parameters, in relation to the time of publication of the dataset "
        "(required).'>"
        "    <Value alias='1'>observation</Value>"
        "    <Value alias='2'>astronomicalPrediction</Value>"
        "    <Value alias='3'>analysisOrHybrid</Value>"
        "    <Value alias='5'>hydrodynamicForecast</Value>"
        "  </Option>"
        "  <Option name='DATASETS' type='string' description="
        "'Comma separated list of datasets at different timestamps.'/>"
        "  <Option name='DATASETS_TIME_POINT' type='string' description="
        "'Comma separated list of the time point value of each dataset of "
        "DATASETS.'/>"
        "  <Option name='GEOGRAPHIC_IDENTIFIER' type='string' description="
        "'Description, or location code from list agreed by data producers'/>"
        "  <Option name='ISSUE_DATE' type='string' description="
        "'Issue date as YYYYMMDD'/>"
        "  <Option name='ISSUE_TIME' type='string' description="
        "'Issue time as hhmmssZ or hhmmss[+-]HHMM'/>"
        "  <Option name='DATASET_DELIVERY_INTERVAL' type='string' description="
        "'Expected time interval between availability of successive datasets "
        "for time-varying data. Must be formatted as PnYnMnDTnHnMnS "
        "(ISO8601 duration)'/>"
        "  <Option name='TIME_RECORD_INTERVAL' type='integer' description="
        "'Interval in seconds between time records.'/>"
        "  <Option name='COMMON_POINT_RULE' type='string-select' description="
        "'Procedure used for evaluating the coverage at a position that falls "
        "on the boundary or in an area of overlap between geographic objects' "
        "default='high'>"
        "    <Value alias='1'>average</Value>"
        "    <Value alias='2'>low</Value>"
        "    <Value alias='3'>high</Value>"
        "    <Value alias='4'>all</Value>"
        "  </Option>"
        "  <Option name='UNCERTAINTY_SPEED' type='float' "
        "description='Uncertainty of speeds in knot'/>"
        "  <Option name='UNCERTAINTY_DIRECTION' type='float' "
        "description='Uncertainty of direction angles in degree'/>"
        "  <Option name='HORIZONTAL_POSITION_UNCERTAINTY' type='float' "
        "description='Horizontal position uncertainty in meter'/>"
        "  <Option name='VERTICAL_UNCERTAINTY' type='float' "
        "description='Vertical uncertainty in meter'/>"
        "  <Option name='TIME_UNCERTAINTY' type='float' "
        "description='Time uncertainty in second'/>"
        "  <Option name='COMPRESS' type='string-select' default='DEFLATE'>"
        "    <Value>NONE</Value>"
        "    <Value>DEFLATE</Value>"
        "  </Option>"
        "  <Option name='ZLEVEL' type='int' "
        "description='DEFLATE compression level 1-9' default='6' />"
        "  <Option name='BLOCK_SIZE' type='int' description='Chunk size' />"
        "</CreationOptionList>");

    poDriver->pfnIdentify = S111DatasetIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                     DeclareDeferredHDF5Plugin()                      */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredHDF5Plugin()
{
    if (GDALGetDriverByName(HDF5_DRIVER_NAME) != nullptr)
    {
        return;
    }
    {
        auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
        poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                                  PLUGIN_INSTALLATION_MESSAGE);
#endif
        HDF5DriverSetCommonMetadata(poDriver);
        GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
    }
    {
        auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
        poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                                  PLUGIN_INSTALLATION_MESSAGE);
#endif
        HDF5ImageDriverSetCommonMetadata(poDriver);
        GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
    }
    {
        auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
        poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                                  PLUGIN_INSTALLATION_MESSAGE);
#endif
        BAGDriverSetCommonMetadata(poDriver);
        GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
    }
    {
        auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
        poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                                  PLUGIN_INSTALLATION_MESSAGE);
#endif
        S102DriverSetCommonMetadata(poDriver);
        GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
    }
    {
        auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
        poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                                  PLUGIN_INSTALLATION_MESSAGE);
#endif
        S104DriverSetCommonMetadata(poDriver);
        GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
    }
    {
        auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
        poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                                  PLUGIN_INSTALLATION_MESSAGE);
#endif
        S111DriverSetCommonMetadata(poDriver);
        GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
    }
}
#endif

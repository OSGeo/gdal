/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  netCDF driver
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault, <even.rouault at spatialys.com>
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

#include "ogrsf_frmts.h"
#include "gdal_priv.h"

#include "netcdfdrivercore.h"

#include <algorithm>
#include <cctype>
#include <string_view>

#ifdef HAS_NETCDF_H
#include "netcdfdataset.h"
#ifdef NETCDF_HAS_NC2
#define NETCDF_CORE_HAS_NC2 1
#endif
#else
// We don't have an easy way to guess that without accessing netcdf.h, so
// assume it is present
#ifndef NETCDF_CORE_HAS_NC2
#define NETCDF_CORE_HAS_NC2 1
#endif
#endif

/************************************************************************/
/*                      netCDFIdentifyFormat()                          */
/************************************************************************/

NetCDFFormatEnum netCDFIdentifyFormat(GDALOpenInfo *poOpenInfo, bool bCheckExt)
{
    // Does this appear to be a netcdf file? If so, which format?
    // http://www.unidata.ucar.edu/software/netcdf/docs/faq.html#fv1_5

    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "NETCDF:"))
        return NCDF_FORMAT_UNKNOWN;
    if (poOpenInfo->nHeaderBytes < 4)
        return NCDF_FORMAT_NONE;
    const char *pszHeader =
        reinterpret_cast<const char *>(poOpenInfo->pabyHeader);

#ifdef ENABLE_NCDUMP
    if (poOpenInfo->fpL != nullptr && STARTS_WITH(pszHeader, "netcdf ") &&
        strstr(pszHeader, "dimensions:") && strstr(pszHeader, "variables:"))
    {
        if (strstr(pszHeader, "// NC4C"))
            return NCDF_FORMAT_NC4C;
        else if (strstr(pszHeader, "// NC4"))
            return NCDF_FORMAT_NC4;
        else
            return NCDF_FORMAT_NC;
    }
#endif  // ENABLE_NCDUMP

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    // We don't necessarily want to catch bugs in libnetcdf ...
    if (CPLGetConfigOption("DISABLE_OPEN_REAL_NETCDF_FILES", nullptr))
    {
        return NCDF_FORMAT_NONE;
    }
#endif

    if (STARTS_WITH_CI(pszHeader, "CDF\001"))
    {
        // In case the netCDF driver is registered before the GMT driver,
        // avoid opening GMT files.
        if (GDALGetDriverByName("GMT") != nullptr)
        {
            bool bFoundZ = false;
            bool bFoundDimension = false;
            constexpr const char *DIMENSION = "dimension";
            constexpr int DIMENSION_LEN =
                int(std::char_traits<char>::length(DIMENSION));
            static_assert(DIMENSION_LEN == 9);
            const std::string_view header(pszHeader, poOpenInfo->nHeaderBytes);
            for (int i = 0;
                 i < static_cast<int>(header.size()) - (1 + DIMENSION_LEN + 1);
                 i++)
            {
                if (header[i] == 1 && header[i + 1] == 'z' &&
                    header[i + 2] == 0)
                    bFoundZ = true;
                else if (header[i] == DIMENSION_LEN &&
                         header.substr(i + 1, DIMENSION_LEN) == DIMENSION &&
                         header[i + DIMENSION_LEN + 1] == 0)
                    bFoundDimension = true;
            }
            if (bFoundZ && bFoundDimension)
                return NCDF_FORMAT_UNKNOWN;
        }

        return NCDF_FORMAT_NC;
    }

    if (STARTS_WITH_CI(pszHeader, "CDF\002"))
    {
        return NCDF_FORMAT_NC2;
    }

    constexpr char HDF5_SIG[] = "\211HDF\r\n\032\n";
    constexpr int HDF5_SIG_LEN = int(std::char_traits<char>::length(HDF5_SIG));
    static_assert(HDF5_SIG_LEN == 8);
    // First non-zero offset at which the HDF5 signature can be found.
    constexpr int HDF5_SIG_OFFSET = 512;
    if (STARTS_WITH_CI(pszHeader, HDF5_SIG) ||
        (poOpenInfo->nHeaderBytes > HDF5_SIG_OFFSET + HDF5_SIG_LEN &&
         memcmp(pszHeader + HDF5_SIG_OFFSET, HDF5_SIG, HDF5_SIG_LEN) == 0))
    {
        // Requires netCDF-4/HDF5 support in libnetcdf (not just libnetcdf-v4).
        // If HDF5 is not supported in GDAL, this driver will try to open the
        // file Else, make sure this driver does not try to open HDF5 files If
        // user really wants to open with this driver, use NETCDF:file.h5
        // format.  This check should be relaxed, but there is no clear way to
        // make a difference.

// Check for HDF5 support in GDAL.
#ifdef HAVE_HDF5
        if (bCheckExt)
        {
            // Check by default.
            const char *pszExtension = CPLGetExtension(poOpenInfo->pszFilename);
            if (!(EQUAL(pszExtension, "nc") || EQUAL(pszExtension, "cdf") ||
                  EQUAL(pszExtension, "nc2") || EQUAL(pszExtension, "nc4") ||
                  EQUAL(pszExtension, "nc3") || EQUAL(pszExtension, "grd") ||
                  EQUAL(pszExtension, "gmac")))
            {
                if (GDALGetDriverByName("HDF5") != nullptr)
                {
                    return NCDF_FORMAT_HDF5;
                }
            }
        }
#endif

        return NCDF_FORMAT_NC4;
    }
    else if (STARTS_WITH_CI(pszHeader, "\016\003\023\001"))
    {
        // Requires HDF4 support in libnetcdf, but if HF4 is supported by GDAL
        // don't try to open.
        // If user really wants to open with this driver, use NETCDF:file.hdf
        // syntax.

// Check for HDF4 support in GDAL.
#ifdef HAVE_HDF4
        if (bCheckExt && GDALGetDriverByName("HDF4") != nullptr)
        {
            // Check by default.
            // Always treat as HDF4 file.
            return NCDF_FORMAT_HDF4;
        }
#endif

// Check for HDF4 support in libnetcdf.
#ifdef NETCDF_HAS_HDF4
        return NCDF_FORMAT_NC4;
#else
        return NCDF_FORMAT_HDF4;
#endif
    }

    // The HDF5 signature of netCDF 4 files can be at offsets 512, 1024, 2048,
    // etc.
    const char *pszExtension = CPLGetExtension(poOpenInfo->pszFilename);
    if (poOpenInfo->fpL != nullptr &&
        (!bCheckExt || EQUAL(pszExtension, "nc") ||
         EQUAL(pszExtension, "cdf") || EQUAL(pszExtension, "nc4")))
    {
        vsi_l_offset nOffset = HDF5_SIG_OFFSET;
        for (int i = 0; i < 64; i++)
        {
            GByte abyBuf[HDF5_SIG_LEN];
            if (VSIFSeekL(poOpenInfo->fpL, nOffset, SEEK_SET) != 0 ||
                VSIFReadL(abyBuf, 1, HDF5_SIG_LEN, poOpenInfo->fpL) !=
                    HDF5_SIG_LEN)
            {
                break;
            }
            if (memcmp(abyBuf, HDF5_SIG, HDF5_SIG_LEN) == 0)
            {
                return NCDF_FORMAT_NC4;
            }
            nOffset *= 2;
        }
    }

    return NCDF_FORMAT_NONE;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

static int netCDFDatasetIdentify(GDALOpenInfo *poOpenInfo)

{
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "NETCDF:"))
    {
        return TRUE;
    }
    const NetCDFFormatEnum eTmpFormat =
        netCDFIdentifyFormat(poOpenInfo,
                             /* bCheckExt = */ true);
    if (NCDF_FORMAT_NC == eTmpFormat || NCDF_FORMAT_NC2 == eTmpFormat ||
        NCDF_FORMAT_NC4 == eTmpFormat || NCDF_FORMAT_NC4C == eTmpFormat)
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                    NCDFDriverGetSubdatasetInfo()                     */
/************************************************************************/

struct NCDFDriverSubdatasetInfo : public GDALSubdatasetInfo
{
  public:
    explicit NCDFDriverSubdatasetInfo(const std::string &fileName)
        : GDALSubdatasetInfo(fileName)
    {
    }

    // GDALSubdatasetInfo interface
  private:
    void parseFileName() override
    {

        if (!STARTS_WITH_CI(m_fileName.c_str(), "NETCDF:"))
        {
            return;
        }

        CPLStringList aosParts{CSLTokenizeString2(m_fileName.c_str(), ":", 0)};
        const int iPartsCount{CSLCount(aosParts)};

        if (iPartsCount >= 3)
        {

            m_driverPrefixComponent = aosParts[0];

            int subdatasetIndex{2};

            std::string part1{aosParts[1]};
            if (!part1.empty() && part1[0] == '"')
            {
                part1 = part1.substr(1);
            }

            const bool hasDriveLetter{
                (strlen(aosParts[2]) > 1 &&
                 (aosParts[2][0] == '\\' || aosParts[2][0] == '/')) &&
                part1.length() == 1 &&
                std::isalpha(static_cast<unsigned char>(part1.at(0)))};

            const bool hasProtocol{part1 == "/vsicurl/http" ||
                                   part1 == "/vsicurl/https" ||
                                   part1 == "/vsicurl_streaming/http" ||
                                   part1 == "/vsicurl_streaming/https" ||
                                   part1 == "http" || part1 == "https"};

            m_pathComponent = aosParts[1];
            if (hasDriveLetter || hasProtocol)
            {
                m_pathComponent.append(":");
                m_pathComponent.append(aosParts[2]);
                subdatasetIndex++;
            }

            // Check for bogus paths
            if (subdatasetIndex < iPartsCount)
            {
                m_subdatasetComponent = aosParts[subdatasetIndex];

                // Append any remaining part
                for (int i = subdatasetIndex + 1; i < iPartsCount; ++i)
                {
                    m_subdatasetComponent.append(":");
                    m_subdatasetComponent.append(aosParts[i]);
                }
            }

            // Remove quotes from subdataset component
            if (!m_subdatasetComponent.empty() &&
                m_subdatasetComponent[0] == '"')
            {
                m_subdatasetComponent = m_subdatasetComponent.substr(1);
            }
            if (!m_subdatasetComponent.empty() &&
                m_subdatasetComponent.rfind('"') ==
                    m_subdatasetComponent.length() - 1)
            {
                m_subdatasetComponent.pop_back();
            }
        }
    }
};

static GDALSubdatasetInfo *NCDFDriverGetSubdatasetInfo(const char *pszFileName)
{
    if (STARTS_WITH_CI(pszFileName, "NETCDF:"))
    {
        std::unique_ptr<GDALSubdatasetInfo> info =
            std::make_unique<NCDFDriverSubdatasetInfo>(pszFileName);
        // Subdataset component can be empty, path cannot.
        if (!info->GetPathComponent().empty())
        {
            return info.release();
        }
    }
    return nullptr;
}

/************************************************************************/
/*                   netCDFDriverSetCommonMetadata()                    */
/************************************************************************/

void netCDFDriverSetCommonMetadata(GDALDriver *poDriver)
{
    // Set the driver details.
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Network Common Data Format");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/netcdf.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "nc");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "   <Option name='HONOUR_VALID_RANGE' type='boolean' scope='raster' "
        "description='Whether to set to nodata pixel values outside of the "
        "validity range' default='YES'/>"
        "   <Option name='IGNORE_XY_AXIS_NAME_CHECKS' type='boolean' "
        "scope='raster' "
        "description='Whether X/Y dimensions should be always considered as "
        "geospatial axis, even if the lack conventional attributes confirming "
        "it.'"
        " default='NO'/>"
        "   <Option name='VARIABLES_AS_BANDS' type='boolean' scope='raster' "
        "description='Whether 2D variables that share the same indexing "
        "dimensions "
        "should be exposed as several bands of a same dataset instead of "
        "several "
        "subdatasets.' default='NO'/>"
        "   <Option name='ASSUME_LONGLAT' type='boolean' scope='raster' "
        "description='Whether when all else has failed for determining a CRS, "
        "a "
        "meaningful geotransform has been found, and is within the  "
        "bounds -180,360 -90,90, assume OGC:CRS84.' default='NO'/>"
        "   <Option name='PRESERVE_AXIS_UNIT_IN_CRS' type='boolean' "
        "scope='raster' description='Whether unusual linear axis unit (km) "
        "should be kept as such, instead of being normalized to metre' "
        "default='NO'/>"
        "</OpenOptionList>");
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONDATATYPES,
        "Byte Int8 UInt16 Int16 UInt32 Int32 Int64 UInt64 "
        "Float32 Float64 "
        "CInt16 CInt32 CFloat32 CFloat64");
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "   <Option name='FORMAT' type='string-select' default='NC'>"
        "     <Value>NC</Value>"
#if NETCDF_CORE_HAS_NC2
        "     <Value>NC2</Value>"
#endif
        "     <Value>NC4</Value>"
        "     <Value>NC4C</Value>"
        "   </Option>"
        "   <Option name='COMPRESS' type='string-select' scope='raster' "
        "default='NONE'>"
        "     <Value>NONE</Value>"
        "     <Value>DEFLATE</Value>"
        "   </Option>"
        "   <Option name='ZLEVEL' type='int' scope='raster' "
        "description='DEFLATE compression level 1-9' default='1'/>"
        "   <Option name='WRITE_BOTTOMUP' type='boolean' scope='raster' "
        "default='YES'>"
        "   </Option>"
        "   <Option name='WRITE_GDAL_TAGS' type='boolean' default='YES'>"
        "   </Option>"
        "   <Option name='WRITE_LONLAT' type='string-select' scope='raster'>"
        "     <Value>YES</Value>"
        "     <Value>NO</Value>"
        "     <Value>IF_NEEDED</Value>"
        "   </Option>"
        "   <Option name='TYPE_LONLAT' type='string-select' scope='raster'>"
        "     <Value>float</Value>"
        "     <Value>double</Value>"
        "   </Option>"
        "   <Option name='PIXELTYPE' type='string-select' scope='raster' "
        "description='(deprecated, use Int8 datatype) only used in Create()'>"
        "       <Value>DEFAULT</Value>"
        "       <Value>SIGNEDBYTE</Value>"
        "   </Option>"
        "   <Option name='CHUNKING' type='boolean' scope='raster' "
        "default='YES' description='define chunking when creating netcdf4 "
        "file'/>"
        "   <Option name='MULTIPLE_LAYERS' type='string-select' scope='vector' "
        "description='Behaviour regarding multiple vector layer creation' "
        "default='NO'>"
        "       <Value>NO</Value>"
        "       <Value>SEPARATE_FILES</Value>"
        "       <Value>SEPARATE_GROUPS</Value>"
        "   </Option>"
        "   <Option name='GEOMETRY_ENCODING' type='string' scope='vector' "
        "default='CF_1.8' description='Specifies the type of geometry encoding "
        "when creating a netCDF dataset'>"
        "       <Value>WKT</Value>"
        "       <Value>CF_1.8</Value>"
        "   </Option>"
        "   <Option name='CONFIG_FILE' type='string' scope='vector' "
        "description='Path to a XML configuration file (or content inlined)'/>"
        "   <Option name='WRITE_GDAL_VERSION' type='boolean' default='YES'/>"
        "   <Option name='WRITE_GDAL_HISTORY' type='boolean' default='YES'/>"
        "   <Option name='BAND_NAMES' type='string' scope='raster' />"
        "</CreationOptionList>");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");

    poDriver->SetMetadataItem(
        GDAL_DS_LAYER_CREATIONOPTIONLIST,
        "<LayerCreationOptionList>"
        "   <Option name='RECORD_DIM_NAME' type='string' description='Name of "
        "the unlimited dimension' default='record'/>"
        "   <Option name='STRING_DEFAULT_WIDTH' type='int' description='"
        "For non-NC4 format, "
        "default width of strings. Default is 10 in autogrow mode, 80 "
        "otherwise.'/>"
        "   <Option name='WKT_DEFAULT_WIDTH' type='int' description='"
        "For non-NC4 format, "
        "default width of WKT strings. Default is 1000 in autogrow mode, 10000 "
        "otherwise.'/>"
        "   <Option name='AUTOGROW_STRINGS' type='boolean' "
        "description='Whether to auto-grow non-bounded string fields of "
        "bidimensional char variable' default='YES'/>"
        "   <Option name='USE_STRING_IN_NC4' type='boolean' "
        "description='Whether to use NetCDF string type for strings in NC4 "
        "format. If NO, bidimensional char variable are used' default='YES'/>"
#if 0
"   <Option name='NCDUMP_COMPAT' type='boolean' description='When USE_STRING_IN_NC4=YEs, whether to use empty string instead of null string to avoid crashes with ncdump' default='NO'/>"
#endif
        "   <Option name='FEATURE_TYPE' type='string-select' description='CF "
        "FeatureType' default='AUTO'>"
        "       <Value>AUTO</Value>"
        "       <Value>POINT</Value>"
        "       <Value>PROFILE</Value>"
        "   </Option>"
        "   <Option name='BUFFER_SIZE' type='int' default='' "
        "description='Specifies the soft limit of buffer translation in bytes. "
        "Minimum size is 4096. Does not apply to datasets with CF version less "
        "than 1.8.'/>"
        "   <Option name='GROUPLESS_WRITE_BACK' type='boolean' default='NO' "
        "description='Enables or disables array building write back for "
        "CF-1.8.'/>"
        "   <Option name='PROFILE_DIM_NAME' type='string' description='Name of "
        "the profile dimension and variable' default='profile'/>"
        "   <Option name='PROFILE_DIM_INIT_SIZE' type='string' "
        "description='Initial size of profile dimension (default 100), or "
        "UNLIMITED for NC4 files'/>"
        "   <Option name='PROFILE_VARIABLES' type='string' description='Comma "
        "separated list of field names that must be indexed by the profile "
        "dimension'/>"
        "</LayerCreationOptionList>");

    // Make driver config and capabilities available.
#if NETCDF_CORE_HAS_NC2
    poDriver->SetMetadataItem("NETCDF_HAS_NC2", "YES");
#endif
    poDriver->SetMetadataItem("NETCDF_HAS_NC4", "YES");
#ifdef NETCDF_HAS_HDF4
    poDriver->SetMetadataItem("NETCDF_HAS_HDF4", "YES");
#endif
#ifdef HAVE_HDF4
    poDriver->SetMetadataItem("GDAL_HAS_HDF4", "YES");
#endif
#ifdef HAVE_HDF5
    poDriver->SetMetadataItem("GDAL_HAS_HDF5", "YES");
#endif
    poDriver->SetMetadataItem("NETCDF_HAS_NETCDF_MEM", "YES");

#ifdef ENABLE_NCDUMP
    poDriver->SetMetadataItem("ENABLE_NCDUMP", "YES");
#endif

    poDriver->SetMetadataItem(GDAL_DCAP_MULTIDIM_RASTER, "YES");

    poDriver->SetMetadataItem(
        GDAL_DMD_MULTIDIM_DATASET_CREATIONOPTIONLIST,
        "<MultiDimDatasetCreationOptionList>"
        "   <Option name='FORMAT' type='string-select' default='NC4'>"
        "     <Value>NC</Value>"
#if NETCDF_CORE_HAS_NC2
        "     <Value>NC2</Value>"
#endif
        "     <Value>NC4</Value>"
        "     <Value>NC4C</Value>"
        "   </Option>"
        "   <Option name='CONVENTIONS' type='string' default='CF-1.6' "
        "description='Value of the Conventions attribute'/>"
        "</MultiDimDatasetCreationOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DMD_MULTIDIM_DIMENSION_CREATIONOPTIONLIST,
        "<MultiDimDimensionCreationOptionList>"
        "   <Option name='UNLIMITED' type='boolean' description='Whether the "
        "dimension should be unlimited' default='false'/>"
        "</MultiDimDimensionCreationOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DMD_MULTIDIM_ARRAY_CREATIONOPTIONLIST,
        "<MultiDimArrayCreationOptionList>"
        "   <Option name='BLOCKSIZE' type='int' description='Block size in "
        "pixels'/>"
        "   <Option name='COMPRESS' type='string-select' default='NONE'>"
        "     <Value>NONE</Value>"
        "     <Value>DEFLATE</Value>"
        "   </Option>"
        "   <Option name='ZLEVEL' type='int' description='DEFLATE compression "
        "level 1-9' default='1'/>"
        "   <Option name='NC_TYPE' type='string-select' default='netCDF data "
        "type'>"
        "     <Value>AUTO</Value>"
        "     <Value>NC_BYTE</Value>"
        "     <Value>NC_INT64</Value>"
        "     <Value>NC_UINT64</Value>"
        "   </Option>"
        "</MultiDimArrayCreationOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DMD_MULTIDIM_ARRAY_OPENOPTIONLIST,
        "<MultiDimArrayOpenOptionList>"
        "   <Option name='USE_DEFAULT_FILL_AS_NODATA' type='boolean' "
        "description='Whether the default fill value should be used as nodata "
        "when there is no _FillValue or missing_value attribute' default='NO'/>"
        "</MultiDimArrayOpenOptionList>");

    poDriver->SetMetadataItem(GDAL_DMD_MULTIDIM_ATTRIBUTE_CREATIONOPTIONLIST,
                              "<MultiDimAttributeCreationOptionList>"
                              "   <Option name='NC_TYPE' type='string-select' "
                              "default='netCDF data type'>"
                              "     <Value>AUTO</Value>"
                              "     <Value>NC_BYTE</Value>"
                              "     <Value>NC_CHAR</Value>"
                              "     <Value>NC_INT64</Value>"
                              "     <Value>NC_UINT64</Value>"
                              "   </Option>"
                              "</MultiDimAttributeCreationOptionList>");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES,
                              "Integer Integer64 Real String Date DateTime");
    poDriver->SetMetadataItem(GDAL_DMD_CREATION_FIELD_DEFN_FLAGS,
                              "Comment AlternativeName");

    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->pfnIdentify = netCDFDatasetIdentify;
    poDriver->pfnGetSubdatasetInfoFunc = NCDFDriverGetSubdatasetInfo;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_MULTIDIMENSIONAL, "YES");
}

/************************************************************************/
/*                    DeclareDeferredNetCDFPlugin()                     */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredNetCDFPlugin()
{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
    {
        return;
    }
    auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
    poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                              PLUGIN_INSTALLATION_MESSAGE);
#endif
    netCDFDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif

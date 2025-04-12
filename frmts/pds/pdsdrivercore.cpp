/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Planetary drivers
 * Author:   Even Rouault
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "pdsdrivercore.h"

#include "nasakeywordhandler.h"

/************************************************************************/
/*                     GetVICARLabelOffsetFromPDS3()                    */
/************************************************************************/

vsi_l_offset GetVICARLabelOffsetFromPDS3(const char *pszHdr, VSILFILE *fp,
                                         std::string &osVICARHeader)
{
    const char *pszPDSVersionID = strstr(pszHdr, "PDS_VERSION_ID");
    int nOffset = 0;
    if (pszPDSVersionID)
        nOffset = static_cast<int>(pszPDSVersionID - pszHdr);

    NASAKeywordHandler oKeywords;
    if (oKeywords.Ingest(fp, nOffset))
    {
        const int nRecordBytes =
            atoi(oKeywords.GetKeyword("RECORD_BYTES", "0"));
        const int nImageHeader =
            atoi(oKeywords.GetKeyword("^IMAGE_HEADER", "0"));
        if (nRecordBytes > 0 && nImageHeader > 0)
        {
            const auto nImgHeaderOffset =
                static_cast<vsi_l_offset>(nImageHeader - 1) * nRecordBytes;
            osVICARHeader.resize(1024);
            size_t nMemb;
            if (VSIFSeekL(fp, nImgHeaderOffset, SEEK_SET) == 0 &&
                (nMemb = VSIFReadL(&osVICARHeader[0], 1, osVICARHeader.size(),
                                   fp)) != 0 &&
                osVICARHeader.find("LBLSIZE") != std::string::npos)
            {
                osVICARHeader.resize(nMemb);
                return nImgHeaderOffset;
            }
        }
    }
    return 0;
}

/************************************************************************/
/*                     PDSDriverIdentify()                              */
/************************************************************************/

int PDSDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->pabyHeader == nullptr || poOpenInfo->fpL == nullptr)
        return FALSE;

    const char *pszHdr = reinterpret_cast<char *>(poOpenInfo->pabyHeader);
    if (strstr(pszHdr, "PDS_VERSION_ID") == nullptr &&
        strstr(pszHdr, "ODL_VERSION_ID") == nullptr)
    {
        return FALSE;
    }

    // Some PDS3 images include a VICAR header pointed by ^IMAGE_HEADER.
    // If the user sets GDAL_TRY_PDS3_WITH_VICAR=YES, then we will gracefully
    // hand over the file to the VICAR dataset.
    std::string unused;
    if (CPLTestBool(CPLGetConfigOption("GDAL_TRY_PDS3_WITH_VICAR", "NO")) &&
        !STARTS_WITH(poOpenInfo->pszFilename, "/vsisubfile/") &&
        GetVICARLabelOffsetFromPDS3(pszHdr, poOpenInfo->fpL, unused) > 0)
    {
        CPLDebug("PDS3", "File is detected to have a VICAR header. "
                         "Handing it over to the VICAR driver");
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                      PDSDriverSetCommonMetadata()                    */
/************************************************************************/

void PDSDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(PDS_DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "NASA Planetary Data System");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/pds.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnIdentify = PDSDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
}

/************************************************************************/
/*                         PDS4DriverIdentify()                         */
/************************************************************************/

int PDS4DriverIdentify(GDALOpenInfo *poOpenInfo)
{
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "PDS4:"))
        return TRUE;
    if (poOpenInfo->nHeaderBytes == 0)
        return FALSE;

    const auto HasProductSomethingRootElement = [](const char *pszStr)
    {
        return strstr(pszStr, "Product_Observational") != nullptr ||
               strstr(pszStr, "Product_Ancillary") != nullptr ||
               strstr(pszStr, "Product_Collection") != nullptr;
    };
    const auto HasPDS4Schema = [](const char *pszStr)
    { return strstr(pszStr, "://pds.nasa.gov/pds4/pds/v1") != nullptr; };

    for (int i = 0; i < 2; ++i)
    {
        const char *pszHeader =
            reinterpret_cast<const char *>(poOpenInfo->pabyHeader);
        int nMatches = 0;
        if (HasProductSomethingRootElement(pszHeader))
            nMatches++;
        if (HasPDS4Schema(pszHeader))
            nMatches++;
        if (nMatches == 2)
        {
            return TRUE;
        }
        if (i == 0)
        {
            if (nMatches == 0 || poOpenInfo->nHeaderBytes >= 8192)
                break;
            // If we have found one of the 2 matching elements to identify
            // PDS4 products, but have only ingested the default 1024 bytes,
            // then try to ingest more.
            poOpenInfo->TryToIngest(8192);
        }
    }
    return FALSE;
}

/************************************************************************/
/*                      PDS4DriverSetCommonMetadata()                   */
/************************************************************************/

void PDS4DriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(PDS4_DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DELETE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_REORDER_FIELDS, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_ALTER_FIELD_DEFN_FLAGS,
                              "Name Type WidthPrecision");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");

    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "NASA Planetary Data System 4");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/pds4.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "xml");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
                              "Byte Int8 UInt16 Int16 UInt32 Int32 Float32 "
                              "Float64 CFloat32 CFloat64");
    poDriver->SetMetadataItem(GDAL_DMD_OPENOPTIONLIST, "<OpenOptionList/>");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_SUBDATASETS, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='LAT' type='string' scope='vector' description="
        "'Name of a field containing a Latitude value' default='Latitude'/>"
        "  <Option name='LONG' type='string' scope='vector' description="
        "'Name of a field containing a Longitude value' default='Longitude'/>"
        "  <Option name='ALT' type='string' scope='vector' description="
        "'Name of a field containing a Altitude value' default='Altitude'/>"
        "  <Option name='WKT' type='string' scope='vector' description="
        "'Name of a field containing a geometry encoded in the WKT format' "
        "default='WKT'/>"
        "  <Option name='KEEP_GEOM_COLUMNS' scope='vector' type='boolean' "
        "description="
        "'whether to add original x/y/geometry columns as regular fields.' "
        "default='NO' />"
        "</OpenOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "  <Option name='IMAGE_FILENAME' type='string' scope='raster' "
        "description="
        "'Image filename'/>"
        "  <Option name='IMAGE_EXTENSION' type='string' scope='raster' "
        "description="
        "'Extension of the binary raw/geotiff file'/>"
        "  <Option name='CREATE_LABEL_ONLY' scope='raster' type='boolean' "
        "description="
        "'whether to create only the XML label when converting from an "
        "existing raw format.' default='NO' />"
        "  <Option name='IMAGE_FORMAT' type='string-select' scope='raster' "
        "description='Format of the image file' default='RAW'>"
        "     <Value>RAW</Value>"
        "     <Value>GEOTIFF</Value>"
        "  </Option>"
#ifdef notdef
        "  <Option name='GEOTIFF_OPTIONS' type='string' scope='raster' "
        "description='Comma separated list of KEY=VALUE tuples to forward "
        "to the GeoTIFF driver'/>"
#endif
        "  <Option name='INTERLEAVE' type='string-select' scope='raster' "
        "description="
        "'Pixel organization' default='BSQ'>"
        "     <Value>BSQ</Value>"
        "     <Value>BIP</Value>"
        "     <Value>BIL</Value>"
        "  </Option>"
        "  <Option name='VAR_*' type='string' scope='raster,vector' "
        "description="
        "'Value to substitute to a variable in the template'/>"
        "  <Option name='TEMPLATE' type='string' scope='raster,vector' "
        "description="
        "'.xml template to use'/>"
        "  <Option name='USE_SRC_LABEL' type='boolean' scope='raster' "
        "description='Whether to use source label in PDS4 to PDS4 conversions' "
        "default='YES'/>"
        "  <Option name='LATITUDE_TYPE' type='string-select' "
        "scope='raster,vector' "
        "description='Value of latitude_type' default='Planetocentric'>"
        "     <Value>Planetocentric</Value>"
        "     <Value>Planetographic</Value>"
        "  </Option>"
        "  <Option name='LONGITUDE_DIRECTION' type='string-select' "
        "scope='raster,vector' "
        "description='Value of longitude_direction' "
        "default='Positive East'>"
        "     <Value>Positive East</Value>"
        "     <Value>Positive West</Value>"
        "  </Option>"
        "  <Option name='RADII' type='string' scope='raster,vector' "
        "description='Value of form "
        "semi_major_radius,semi_minor_radius to override the ones of the SRS'/>"
        "  <Option name='ARRAY_TYPE' type='string-select' scope='raster' "
        "description='Name of the "
        "Array XML element' default='Array_3D_Image'>"
        "     <Value>Array</Value>"
        "     <Value>Array_2D</Value>"
        "     <Value>Array_2D_Image</Value>"
        "     <Value>Array_2D_Map</Value>"
        "     <Value>Array_2D_Spectrum</Value>"
        "     <Value>Array_3D</Value>"
        "     <Value>Array_3D_Image</Value>"
        "     <Value>Array_3D_Movie</Value>"
        "     <Value>Array_3D_Spectrum</Value>"
        "  </Option>"
        "  <Option name='ARRAY_IDENTIFIER' type='string' scope='raster' "
        "description='Identifier to put in the Array element'/>"
        "  <Option name='UNIT' type='string' scope='raster' "
        "description='Name of the unit of the array elements'/>"
        "  <Option name='BOUNDING_DEGREES' type='string' scope='raster,vector' "
        "description='Manually set bounding box with the syntax "
        "west_lon,south_lat,east_lon,north_lat'/>"
        "</CreationOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DS_LAYER_CREATIONOPTIONLIST,
        "<LayerCreationOptionList>"
        "  <Option name='TABLE_TYPE' type='string-select' description='Type of "
        "table' default='DELIMITED'>"
        "     <Value>DELIMITED</Value>"
        "     <Value>CHARACTER</Value>"
        "     <Value>BINARY</Value>"
        "  </Option>"
        "  <Option name='LINE_ENDING' type='string-select' description="
        "'end-of-line sequence. Only applies for "
        "TABLE_TYPE=DELIMITED/CHARACTER' "
        "default='CRLF'>"
        "    <Value>CRLF</Value>"
        "    <Value>LF</Value>"
        "  </Option>"
        "  <Option name='GEOM_COLUMNS' type='string-select' description='How "
        "geometry is encoded' default='AUTO'>"
        "     <Value>AUTO</Value>"
        "     <Value>WKT</Value>"
        "     <Value>LONG_LAT</Value>"
        "  </Option>"
        "  <Option name='CREATE_VRT' type='boolean' description='Whether to "
        "generate "
        "a OGR VRT file. Only applies for TABLE_TYPE=DELIMITED' default='YES'/>"
        "  <Option name='LAT' type='string' description="
        "'Name of a field containing a Latitude value' default='Latitude'/>"
        "  <Option name='LONG' type='string' description="
        "'Name of a field containing a Longitude value' default='Longitude'/>"
        "  <Option name='ALT' type='string' description="
        "'Name of a field containing a Altitude value' default='Altitude'/>"
        "  <Option name='WKT' type='string' description="
        "'Name of a field containing a WKT value' default='WKT'/>"
        "  <Option name='SAME_DIRECTORY' type='boolean' description="
        "'Whether table files should be created in the same "
        "directory, or in a subdirectory' default='NO'/>"
        "</LayerCreationOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONFIELDDATATYPES,
        "Integer Integer64 Real String Date DateTime Time");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATASUBTYPES, "Boolean");

    poDriver->pfnIdentify = PDS4DriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");

    poDriver->SetMetadataItem(GDAL_DCAP_UPDATE, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_UPDATE_ITEMS, "Features");
}

/************************************************************************/
/*                         ISIS2DriverIdentify()                        */
/************************************************************************/

int ISIS2DriverIdentify(GDALOpenInfo *poOpenInfo)
{
    if (poOpenInfo->pabyHeader == nullptr)
        return FALSE;

    if (strstr((const char *)poOpenInfo->pabyHeader, "^QUBE") == nullptr)
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                      ISIS2DriverSetCommonMetadata()                  */
/************************************************************************/

void ISIS2DriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(ISIS2_DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "USGS Astrogeology ISIS cube (Version 2)");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/isis2.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnIdentify = ISIS2DriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
}

/************************************************************************/
/*                         ISIS3DriverIdentify()                        */
/************************************************************************/

int ISIS3DriverIdentify(GDALOpenInfo *poOpenInfo)
{
    if (poOpenInfo->fpL != nullptr && poOpenInfo->pabyHeader != nullptr &&
        strstr((const char *)poOpenInfo->pabyHeader, "IsisCube") != nullptr)
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                      ISIS3DriverSetCommonMetadata()                  */
/************************************************************************/

void ISIS3DriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(ISIS3_DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "USGS Astrogeology ISIS cube (Version 3)");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/isis3.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "lbl cub");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
                              "Byte UInt16 Int16 Float32");
    poDriver->SetMetadataItem(GDAL_DMD_OPENOPTIONLIST, "<OpenOptionList/>");
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "  <Option name='DATA_LOCATION' type='string-select' "
        "description='Location of pixel data' default='LABEL'>"
        "     <Value>LABEL</Value>"
        "     <Value>EXTERNAL</Value>"
        "     <Value>GEOTIFF</Value>"
        "  </Option>"
        "  <Option name='GEOTIFF_AS_REGULAR_EXTERNAL' type='boolean' "
        "description='Whether the GeoTIFF file, if uncompressed, should be "
        "registered as a regular raw file' default='YES'/>"
        "  <Option name='GEOTIFF_OPTIONS' type='string' "
        "description='Comma separated list of KEY=VALUE tuples to forward "
        "to the GeoTIFF driver'/>"
        "  <Option name='EXTERNAL_FILENAME' type='string' "
        "description='Override default external filename. "
        "Only for DATA_LOCATION=EXTERNAL or GEOTIFF'/>"
        "  <Option name='TILED' type='boolean' "
        "description='Whether the pixel data should be tiled' default='NO'/>"
        "  <Option name='BLOCKXSIZE' type='int' "
        "description='Tile width' default='256'/>"
        "  <Option name='BLOCKYSIZE' type='int' "
        "description='Tile height' default='256'/>"
        "  <Option name='COMMENT' type='string' "
        "description='Comment to add into the label'/>"
        "  <Option name='LATITUDE_TYPE' type='string-select' "
        "description='Value of Mapping.LatitudeType' default='Planetocentric'>"
        "     <Value>Planetocentric</Value>"
        "     <Value>Planetographic</Value>"
        "  </Option>"
        "  <Option name='LONGITUDE_DIRECTION' type='string-select' "
        "description='Value of Mapping.LongitudeDirection' "
        "default='PositiveEast'>"
        "     <Value>PositiveEast</Value>"
        "     <Value>PositiveWest</Value>"
        "  </Option>"
        "  <Option name='TARGET_NAME' type='string' description='Value of "
        "Mapping.TargetName'/>"
        "  <Option name='FORCE_360' type='boolean' "
        "description='Whether to force longitudes in [0,360] range' "
        "default='NO'/>"
        "  <Option name='WRITE_BOUNDING_DEGREES' type='boolean' "
        "description='Whether to write Min/MaximumLong/Latitude values' "
        "default='YES'/>"
        "  <Option name='BOUNDING_DEGREES' type='string' "
        "description='Manually set bounding box with the syntax "
        "min_long,min_lat,max_long,max_lat'/>"
        "  <Option name='USE_SRC_LABEL' type='boolean' "
        "description='Whether to use source label in ISIS3 to ISIS3 "
        "conversions' "
        "default='YES'/>"
        "  <Option name='USE_SRC_MAPPING' type='boolean' "
        "description='Whether to use Mapping group from source label in "
        "ISIS3 to ISIS3 conversions' "
        "default='NO'/>"
        "  <Option name='USE_SRC_HISTORY' type='boolean' "
        "description='Whether to use content pointed by the History object in "
        "ISIS3 to ISIS3 conversions' "
        "default='YES'/>"
        "  <Option name='ADD_GDAL_HISTORY' type='boolean' "
        "description='Whether to add GDAL specific history in the content "
        "pointed "
        "by the History object in "
        "ISIS3 to ISIS3 conversions' "
        "default='YES'/>"
        "  <Option name='GDAL_HISTORY' type='string' "
        "description='Manually defined GDAL history. Must be formatted as "
        "ISIS3 "
        "PDL. If not specified, it is automatically composed.'/>"
        "</CreationOptionList>");

    poDriver->pfnIdentify = ISIS3DriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                     VICARGetLabelOffset()                            */
/************************************************************************/

vsi_l_offset VICARGetLabelOffset(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->pabyHeader == nullptr || poOpenInfo->fpL == nullptr)
        return static_cast<vsi_l_offset>(-1);

    const auto HasFoundVICARKeywords = [](const char *pszHeader)
    {
        return strstr(pszHeader, "LBLSIZE") != nullptr &&
               strstr(pszHeader, "FORMAT") != nullptr &&
               strstr(pszHeader, "NL") != nullptr &&
               strstr(pszHeader, "NS") != nullptr &&
               strstr(pszHeader, "NB") != nullptr;
    };

    std::string osHeader;
    const char *pszHeader =
        reinterpret_cast<const char *>(poOpenInfo->pabyHeader);
    // Some PDS3 images include a VICAR header pointed by ^IMAGE_HEADER.
    // If the user sets GDAL_TRY_PDS3_WITH_VICAR=YES, then we will gracefully
    // hand over the file to the VICAR dataset.
    vsi_l_offset nOffset = 0;
    const bool bTryPDS3WithVicar =
        CPLTestBool(CPLGetConfigOption("GDAL_TRY_PDS3_WITH_VICAR", "NO")) &&
        !STARTS_WITH(poOpenInfo->pszFilename, "/vsisubfile/");
    if (bTryPDS3WithVicar && (nOffset = GetVICARLabelOffsetFromPDS3(
                                  pszHeader, poOpenInfo->fpL, osHeader)) > 0)
    {
        pszHeader = osHeader.c_str();
    }

    if ((poOpenInfo->nOpenFlags & GDAL_OF_RASTER) == 0 &&
        (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) != 0)
    {
        // If opening in vector-only mode, then check when have NBB != 0
        const char *pszNBB = strstr(pszHeader, "NBB");
        if (pszNBB == nullptr)
            return static_cast<vsi_l_offset>(-1);
        const char *pszEqualSign = strchr(pszNBB, '=');
        if (pszEqualSign == nullptr)
            return static_cast<vsi_l_offset>(-1);
        if (atoi(pszEqualSign + 1) == 0)
            return static_cast<vsi_l_offset>(-1);
    }

    if (HasFoundVICARKeywords(pszHeader))
    {
        // If we find VICAR keywords, but the file starts with PDS_VERSION_ID,
        // it might be a PDS3 image that includes a VICAR header. Check if
        // this is the case.
        if (nOffset == 0 && STARTS_WITH(pszHeader, "PDS_VERSION_ID"))
        {
            if (!bTryPDS3WithVicar &&
                (!GDALGetDriverByName("PDS") ||
                 poOpenInfo->IsSingleAllowedDriver("VICAR")))
            {
                const auto nOffset2 = GetVICARLabelOffsetFromPDS3(
                    pszHeader, poOpenInfo->fpL, osHeader);
                if (nOffset2 > 0 && HasFoundVICARKeywords(osHeader.c_str()))
                {
                    return nOffset2;
                }
            }
        }
        return nOffset;
    }
    return static_cast<vsi_l_offset>(-1);
}

/************************************************************************/
/*                         VICARDriverIdentify()                        */
/************************************************************************/

static int VICARDriverIdentify(GDALOpenInfo *poOpenInfo)
{
    return VICARGetLabelOffset(poOpenInfo) != static_cast<vsi_l_offset>(-1);
}

/************************************************************************/
/*                      VICARDriverSetCommonMetadata()                  */
/************************************************************************/

void VICARDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(VICAR_DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "MIPL VICAR file");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/vicar.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
                              "Byte Int16 Int32 Float32 Float64 CFloat32");
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "  <Option name='GEOREF_FORMAT' type='string-select' "
        "description='How to encode georeferencing information' "
        "default='MIPL'>"
        "     <Value>MIPL</Value>"
        "     <Value>GEOTIFF</Value>"
        "  </Option>"
        "  <Option name='COORDINATE_SYSTEM_NAME' type='string-select' "
        "description='Value of MAP.COORDINATE_SYSTEM_NAME' "
        "default='PLANETOCENTRIC'>"
        "     <Value>PLANETOCENTRIC</Value>"
        "     <Value>PLANETOGRAPHIC</Value>"
        "  </Option>"
        "  <Option name='POSITIVE_LONGITUDE_DIRECTION' type='string-select' "
        "description='Value of MAP.POSITIVE_LONGITUDE_DIRECTION' "
        "default='EAST'>"
        "     <Value>EAST</Value>"
        "     <Value>WEST</Value>"
        "  </Option>"
        "  <Option name='TARGET_NAME' type='string' description='Value of "
        "MAP.TARGET_NAME'/>"
        "  <Option name='USE_SRC_LABEL' type='boolean' "
        "description='Whether to use source label in VICAR to VICAR "
        "conversions' "
        "default='YES'/>"
        "  <Option name='USE_SRC_MAP' type='boolean' "
        "description='Whether to use MAP property from source label in "
        "VICAR to VICAR conversions' "
        "default='NO'/>"
        "  <Option name='LABEL' type='string' "
        "description='Label to use, either as a JSON string or a filename "
        "containing one'/>"
        "  <Option name='COMPRESS' type='string-select' "
        "description='Compression method' default='NONE'>"
        "     <Value>NONE</Value>"
        "     <Value>BASIC</Value>"
        "     <Value>BASIC2</Value>"
        "  </Option>"
        "</CreationOptionList>");

    poDriver->pfnIdentify = VICARDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                     DeclareDeferredPDSPlugin()                       */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredPDSPlugin()
{
    if (GDALGetDriverByName(PDS_DRIVER_NAME) != nullptr)
    {
        return;
    }
    {
        auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
        poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                                  PLUGIN_INSTALLATION_MESSAGE);
#endif
        PDSDriverSetCommonMetadata(poDriver);
        GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
    }
    {
        auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
        poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                                  PLUGIN_INSTALLATION_MESSAGE);
#endif
        PDS4DriverSetCommonMetadata(poDriver);
        GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
    }
    {
        auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
        poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                                  PLUGIN_INSTALLATION_MESSAGE);
#endif
        ISIS2DriverSetCommonMetadata(poDriver);
        GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
    }
    {
        auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
        poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                                  PLUGIN_INSTALLATION_MESSAGE);
#endif
        ISIS3DriverSetCommonMetadata(poDriver);
        GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
    }
    {
        auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
        poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                                  PLUGIN_INSTALLATION_MESSAGE);
#endif
        VICARDriverSetCommonMetadata(poDriver);
        GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
    }
}
#endif

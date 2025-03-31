/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRShapeDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogrshape.h"

#include <cstring>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_port.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "ogrsf_frmts.h"

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

static int OGRShapeDriverIdentify(GDALOpenInfo *poOpenInfo)
{
    // Files not ending with .shp, .shx, .dbf, .shz or .shp.zip are not
    // handled by this driver.
    if (!poOpenInfo->bStatOK)
        return FALSE;
    if (poOpenInfo->bIsDirectory)
    {
        if (STARTS_WITH(poOpenInfo->pszFilename, "/vsizip/") &&
            (strstr(poOpenInfo->pszFilename, ".shp.zip") ||
             strstr(poOpenInfo->pszFilename, ".SHP.ZIP")))
        {
            return TRUE;
        }

        return GDAL_IDENTIFY_UNKNOWN;  // Unsure.
    }
    if (poOpenInfo->fpL == nullptr)
    {
        return FALSE;
    }
    const std::string &osExt = poOpenInfo->osExtension;
    if (EQUAL(osExt.c_str(), "SHP") || EQUAL(osExt.c_str(), "SHX"))
    {
        return poOpenInfo->nHeaderBytes >= 4 &&
               (memcmp(poOpenInfo->pabyHeader, "\x00\x00\x27\x0A", 4) == 0 ||
                memcmp(poOpenInfo->pabyHeader, "\x00\x00\x27\x0D", 4) == 0);
    }
    if (EQUAL(osExt.c_str(), "DBF"))
    {
        if (poOpenInfo->nHeaderBytes < 32)
            return FALSE;
        const GByte *pabyBuf = poOpenInfo->pabyHeader;
        const unsigned int nHeadLen = pabyBuf[8] + pabyBuf[9] * 256;
        const unsigned int nRecordLength = pabyBuf[10] + pabyBuf[11] * 256;
        if (nHeadLen < 32)
            return FALSE;
        // The header length of some .dbf files can be a non-multiple of 32
        // See https://trac.osgeo.org/gdal/ticket/6035
        // Hopefully there are not so many .dbf files around that are not real
        // DBFs
        // if( (nHeadLen % 32) != 0 && (nHeadLen % 32) != 1 )
        //     return FALSE;
        const unsigned int nFields = (nHeadLen - 32) / 32;
        if (nRecordLength < nFields)
            return FALSE;
        return TRUE;
    }
    if (EQUAL(osExt.c_str(), "shz") ||
        (EQUAL(osExt.c_str(), "zip") &&
         (CPLString(poOpenInfo->pszFilename).endsWith(".shp.zip") ||
          CPLString(poOpenInfo->pszFilename).endsWith(".SHP.ZIP"))))
    {
        return poOpenInfo->nHeaderBytes >= 4 &&
               memcmp(poOpenInfo->pabyHeader, "\x50\x4B\x03\x04", 4) == 0;
    }
#ifdef DEBUG
    // For AFL, so that .cur_input is detected as the archive filename.
    if (!STARTS_WITH(poOpenInfo->pszFilename, "/vsitar/") &&
        EQUAL(CPLGetFilename(poOpenInfo->pszFilename), ".cur_input"))
    {
        return GDAL_IDENTIFY_UNKNOWN;
    }
#endif
    return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRShapeDriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (OGRShapeDriverIdentify(poOpenInfo) == FALSE)
        return nullptr;

#ifdef DEBUG
    // For AFL, so that .cur_input is detected as the archive filename.
    if (poOpenInfo->fpL != nullptr &&
        !STARTS_WITH(poOpenInfo->pszFilename, "/vsitar/") &&
        EQUAL(CPLGetFilename(poOpenInfo->pszFilename), ".cur_input"))
    {
        GDALOpenInfo oOpenInfo(
            (CPLString("/vsitar/") + poOpenInfo->pszFilename).c_str(),
            poOpenInfo->nOpenFlags);
        oOpenInfo.papszOpenOptions = poOpenInfo->papszOpenOptions;
        return OGRShapeDriverOpen(&oOpenInfo);
    }
#endif

    CPLString osExt(CPLGetExtensionSafe(poOpenInfo->pszFilename));
    if (!STARTS_WITH(poOpenInfo->pszFilename, "/vsizip/") &&
        (EQUAL(osExt, "shz") ||
         (EQUAL(osExt, "zip") &&
          (CPLString(poOpenInfo->pszFilename).endsWith(".shp.zip") ||
           CPLString(poOpenInfo->pszFilename).endsWith(".SHP.ZIP")))))
    {
        GDALOpenInfo oOpenInfo(
            (CPLString("/vsizip/{") + poOpenInfo->pszFilename + '}').c_str(),
            GA_ReadOnly);
        if (OGRShapeDriverIdentify(&oOpenInfo) == FALSE)
            return nullptr;
        oOpenInfo.eAccess = poOpenInfo->eAccess;
        auto poDS = std::make_unique<OGRShapeDataSource>();

        if (!poDS->OpenZip(&oOpenInfo, poOpenInfo->pszFilename))
        {
            return nullptr;
        }

        return poDS.release();
    }

    auto poDS = std::make_unique<OGRShapeDataSource>();

    if (!poDS->Open(poOpenInfo, true))
    {
        return nullptr;
    }

    return poDS.release();
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset *OGRShapeDriverCreate(const char *pszName, int /* nBands */,
                                         int /* nXSize */, int /* nYSize */,
                                         GDALDataType /* eDT */,
                                         char ** /* papszOptions */)
{
    bool bSingleNewFile = false;
    const CPLString osExt(CPLGetExtensionSafe(pszName));

    /* -------------------------------------------------------------------- */
    /*      Is the target a valid existing directory?                       */
    /* -------------------------------------------------------------------- */
    VSIStatBufL stat;
    if (VSIStatL(pszName, &stat) == 0)
    {
        if (!VSI_ISDIR(stat.st_mode))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s is not a directory.",
                     pszName);

            return nullptr;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Does it end in the extension .shp indicating the user likely    */
    /*      wants to create a single file set?                              */
    /* -------------------------------------------------------------------- */
    else if (EQUAL(osExt, "shp") || EQUAL(osExt, "dbf"))
    {
        bSingleNewFile = true;
    }

    else if (EQUAL(osExt, "shz") ||
             (EQUAL(osExt, "zip") && (CPLString(pszName).endsWith(".shp.zip") ||
                                      CPLString(pszName).endsWith(".SHP.ZIP"))))
    {
        auto poDS = std::make_unique<OGRShapeDataSource>();

        if (!poDS->CreateZip(pszName))
        {
            return nullptr;
        }

        return poDS.release();
    }

    /* -------------------------------------------------------------------- */
    /*      Otherwise try to create a new directory.                        */
    /* -------------------------------------------------------------------- */
    else
    {
        if (VSIMkdir(pszName, 0755) != 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to create directory %s "
                     "for shapefile datastore.",
                     pszName);

            return nullptr;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Return a new OGRDataSource()                                    */
    /* -------------------------------------------------------------------- */
    auto poDS = std::make_unique<OGRShapeDataSource>();

    GDALOpenInfo oOpenInfo(pszName, GA_Update);
    if (!poDS->Open(&oOpenInfo, false, bSingleNewFile))
    {
        return nullptr;
    }

    return poDS.release();
}

/************************************************************************/
/*                           Delete()                                   */
/************************************************************************/

static CPLErr OGRShapeDriverDelete(const char *pszDataSource)

{
    VSIStatBufL sStatBuf;

    if (VSIStatL(pszDataSource, &sStatBuf) != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s does not appear to be a file or directory.",
                 pszDataSource);

        return CE_Failure;
    }

    const CPLString osExt(CPLGetExtensionSafe(pszDataSource));
    if (VSI_ISREG(sStatBuf.st_mode) &&
        (EQUAL(osExt, "shz") ||
         (EQUAL(osExt, "zip") &&
          (CPLString(pszDataSource).endsWith(".shp.zip") ||
           CPLString(pszDataSource).endsWith(".SHP.ZIP")))))
    {
        VSIUnlink(pszDataSource);
        return CE_None;
    }

    const char *const *papszExtensions =
        OGRShapeDataSource::GetExtensionsForDeletion();

    if (VSI_ISREG(sStatBuf.st_mode) &&
        (EQUAL(osExt, "shp") || EQUAL(osExt, "shx") || EQUAL(osExt, "dbf")))
    {
        for (int iExt = 0; papszExtensions[iExt] != nullptr; iExt++)
        {
            const std::string osFile =
                CPLResetExtensionSafe(pszDataSource, papszExtensions[iExt]);
            if (VSIStatL(osFile.c_str(), &sStatBuf) == 0)
                VSIUnlink(osFile.c_str());
        }
    }
    else if (VSI_ISDIR(sStatBuf.st_mode))
    {
        const CPLStringList aosDirEntries(VSIReadDir(pszDataSource));

        for (const char *pszEntry : cpl::Iterate(aosDirEntries))
        {
            if (CSLFindString(papszExtensions,
                              CPLGetExtensionSafe(pszEntry).c_str()) != -1)
            {
                VSIUnlink(CPLFormFilenameSafe(pszDataSource, pszEntry, nullptr)
                              .c_str());
            }
        }

        VSIRmdir(pszDataSource);
    }

    return CE_None;
}

/************************************************************************/
/*                          RegisterOGRShape()                          */
/************************************************************************/

void RegisterOGRShape()

{
    if (GDALGetDriverByName("ESRI Shapefile") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("ESRI Shapefile");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DELETE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DELETE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_REORDER_FIELDS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MEASURED_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_GEOMETRY_FLAGS,
                              "EquatesMultiAndSingleLineStringDuringWrite "
                              "EquatesMultiAndSinglePolygonDuringWrite");

    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "ESRI Shapefile");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "shp");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "shp dbf shz shp.zip");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/vector/shapefile.html");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");
    poDriver->SetMetadataItem(GDAL_DMD_NUMERIC_FIELD_WIDTH_INCLUDES_SIGN,
                              "YES");
    poDriver->SetMetadataItem(
        GDAL_DMD_NUMERIC_FIELD_WIDTH_INCLUDES_DECIMAL_SEPARATOR, "YES");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='ENCODING' type='string' description='to override the "
        "encoding interpretation of the DBF with any encoding supported by "
        "CPLRecode or to \"\" to avoid any recoding'/>"
        "  <Option name='DBF_DATE_LAST_UPDATE' type='string' "
        "description='Modification date to write in DBF header with YYYY-MM-DD "
        "format'/>"
        "  <Option name='ADJUST_TYPE' type='boolean' description='Whether to "
        "read whole .dbf to adjust Real->Integer/Integer64 or "
        "Integer64->Integer field types if possible' default='NO'/>"
        "  <Option name='ADJUST_GEOM_TYPE' type='string-select' "
        "description='Whether and how to adjust layer geometry type from "
        "actual shapes' default='FIRST_SHAPE'>"
        "    <Value>NO</Value>"
        "    <Value>FIRST_SHAPE</Value>"
        "    <Value>ALL_SHAPES</Value>"
        "  </Option>"
        "  <Option name='AUTO_REPACK' type='boolean' description='Whether the "
        "shapefile should be automatically repacked when needed' "
        "default='YES'/>"
        "  <Option name='DBF_EOF_CHAR' type='boolean' description='Whether to "
        "write the 0x1A end-of-file character in DBF files' default='YES'/>"
        "</OpenOptionList>");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST,
                              "<CreationOptionList/>");
    poDriver->SetMetadataItem(
        GDAL_DS_LAYER_CREATIONOPTIONLIST,
        "<LayerCreationOptionList>"
        "  <Option name='SHPT' type='string-select' description='type of "
        "shape' default='automatically detected'>"
        "    <Value>POINT</Value>"
        "    <Value>ARC</Value>"
        "    <Value>POLYGON</Value>"
        "    <Value>MULTIPOINT</Value>"
        "    <Value>POINTZ</Value>"
        "    <Value>ARCZ</Value>"
        "    <Value>POLYGONZ</Value>"
        "    <Value>MULTIPOINTZ</Value>"
        "    <Value>POINTM</Value>"
        "    <Value>ARCM</Value>"
        "    <Value>POLYGONM</Value>"
        "    <Value>MULTIPOINTM</Value>"
        "    <Value>POINTZM</Value>"
        "    <Value>ARCZM</Value>"
        "    <Value>POLYGONZM</Value>"
        "    <Value>MULTIPOINTZM</Value>"
        "    <Value>MULTIPATCH</Value>"
        "    <Value>NONE</Value>"
        "    <Value>NULL</Value>"
        "  </Option>"
        "  <Option name='2GB_LIMIT' type='boolean' description='Restrict .shp "
        "and .dbf to 2GB' default='NO'/>"
        "  <Option name='ENCODING' type='string' description='DBF encoding' "
        "default='LDID/87'/>"
        "  <Option name='RESIZE' type='boolean' description='To resize fields "
        "to their optimal size.' default='NO'/>"
        "  <Option name='SPATIAL_INDEX' type='boolean' description='To create "
        "a spatial index.' default='NO'/>"
        "  <Option name='DBF_DATE_LAST_UPDATE' type='string' "
        "description='Modification date to write in DBF header with YYYY-MM-DD "
        "format'/>"
        "  <Option name='AUTO_REPACK' type='boolean' description='Whether the "
        "shapefile should be automatically repacked when needed' "
        "default='YES'/>"
        "  <Option name='DBF_EOF_CHAR' type='boolean' description='Whether to "
        "write the 0x1A end-of-file character in DBF files' default='YES'/>"
        "</LayerCreationOptionList>");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES,
                              "Integer Integer64 Real String Date");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATASUBTYPES, "Boolean");
    poDriver->SetMetadataItem(GDAL_DMD_CREATION_FIELD_DEFN_FLAGS,
                              "WidthPrecision");
    poDriver->SetMetadataItem(GDAL_DMD_ALTER_FIELD_DEFN_FLAGS,
                              "Name Type WidthPrecision");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_RENAME_LAYERS, "YES");

    poDriver->SetMetadataItem(GDAL_DMD_ALTER_GEOM_FIELD_DEFN_FLAGS, "SRS");

    poDriver->SetMetadataItem(GDAL_DCAP_UPDATE, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_UPDATE_ITEMS, "Features");

    poDriver->pfnOpen = OGRShapeDriverOpen;
    poDriver->pfnIdentify = OGRShapeDriverIdentify;
    poDriver->pfnCreate = OGRShapeDriverCreate;
    poDriver->pfnDelete = OGRShapeDriverDelete;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}

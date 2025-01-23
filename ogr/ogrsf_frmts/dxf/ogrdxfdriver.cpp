/******************************************************************************
 *
 * Project:  DXF Translator
 * Purpose:  Implements OGRDXFDriver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_dxf.h"
#include "cpl_conv.h"

/************************************************************************/
/*                       OGRDXFDriverIdentify()                         */
/************************************************************************/

static int OGRDXFDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->fpL == nullptr || poOpenInfo->nHeaderBytes == 0)
        return FALSE;
    if (poOpenInfo->IsExtensionEqualToCI("dxf"))
        return TRUE;
    const char *pszIter = (const char *)poOpenInfo->pabyHeader;
    bool bFoundZero = false;
    int i = 0;  // Used after for.
    for (; pszIter[i]; i++)
    {
        if (pszIter[i] == '0')
        {
            int j = i - 1;  // Used after for.
            for (; j >= 0; j--)
            {
                if (pszIter[j] != ' ')
                    break;
            }
            if (j < 0 || pszIter[j] == '\n' || pszIter[j] == '\r')
            {
                bFoundZero = true;
                break;
            }
        }
    }
    if (!bFoundZero)
        return FALSE;
    i++;
    while (pszIter[i] == ' ')
        i++;
    while (pszIter[i] == '\n' || pszIter[i] == '\r')
        i++;
    if (!STARTS_WITH_CI(pszIter + i, "SECTION"))
        return FALSE;
    i += static_cast<int>(strlen("SECTION"));
    return pszIter[i] == '\n' || pszIter[i] == '\r';
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRDXFDriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (!OGRDXFDriverIdentify(poOpenInfo))
        return nullptr;

    OGRDXFDataSource *poDS = new OGRDXFDataSource();

    if (!poDS->Open(poOpenInfo->pszFilename, false,
                    poOpenInfo->papszOpenOptions))
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

static GDALDataset *
OGRDXFDriverCreate(const char *pszName, CPL_UNUSED int nBands,
                   CPL_UNUSED int nXSize, CPL_UNUSED int nYSize,
                   CPL_UNUSED GDALDataType eDT, char **papszOptions)
{
    OGRDXFWriterDS *poDS = new OGRDXFWriterDS();

    if (poDS->Open(pszName, papszOptions))
        return poDS;
    else
    {
        delete poDS;
        return nullptr;
    }
}

/************************************************************************/
/*                           RegisterOGRDXF()                           */
/************************************************************************/

void RegisterOGRDXF()

{
    if (GDALGetDriverByName("DXF") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("DXF");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "AutoCAD DXF");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "dxf");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/dxf.html");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "  <Option name='HEADER' type='string' description='Template header "
        "file' default='header.dxf'/>"
        "  <Option name='TRAILER' type='string' description='Template trailer "
        "file' default='trailer.dxf'/>"
        "  <Option name='FIRST_ENTITY' type='int' description='Identifier of "
        "first entity'/>"
        "  <Option name='INSUNITS' type='string-select' "
        "description='Drawing units for the model space ($INSUNITS system "
        "variable)' default='AUTO'>"
        "    <Value>AUTO</Value>"
        "    <Value>HEADER_VALUE</Value>"
        "    <Value alias='0'>UNITLESS</Value>"
        "    <Value alias='1'>INCHES</Value>"
        "    <Value alias='2'>FEET</Value>"
        "    <Value alias='4'>MILLIMETERS</Value>"
        "    <Value alias='5'>CENTIMETERS</Value>"
        "    <Value alias='6'>METERS</Value>"
        "    <Value alias='21'>US_SURVEY_FEET</Value>"
        "  </Option>"
        "  <Option name='MEASUREMENT' type='string-select' "
        "description='Whether the current drawing uses imperial or metric "
        "hatch "
        "pattern and linetype ($MEASUREMENT system variable)' "
        "default='HEADER_VALUE'>"
        "    <Value>HEADER_VALUE</Value>"
        "    <Value alias='0'>IMPERIAL</Value>"
        "    <Value alias='1'>METRIC</Value>"
        "  </Option>"
        "</CreationOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='CLOSED_LINE_AS_POLYGON' type='boolean' description="
        "'Whether to expose closed POLYLINE/LWPOLYLINE as polygons' "
        "default='NO'/>"
        "  <Option name='INLINE_BLOCKS' type='boolean' description="
        "'Whether INSERT entities are exploded with the geometry of the BLOCK "
        "they reference' default='YES'/>"
        "  <Option name='MERGE_BLOCK_GEOMETRIES' type='boolean' description="
        "'Whether blocks should be merged into a compound geometry' "
        "default='YES'/>"
        "  <Option name='TRANSLATE_ESCAPE_SEQUENCES' type='boolean' "
        "description="
        "'Whether character escapes are honored where applicable, and MTEXT "
        "control sequences are stripped' default='YES'/>"
        "  <Option name='INCLUDE_RAW_CODE_VALUES' type='boolean' description="
        "'Whether a RawCodeValues field should be added to contain all group "
        "codes and values' default='NO'/>"
        "  <Option name='3D_EXTENSIBLE_MODE' type='boolean' description="
        "'Whether to include ASM entities with the raw ASM data stored in a "
        "field' default='NO'/>"
        "  <Option name='HATCH_TOLEARNCE' type='float' description="
        "'Tolerance used when looking for the next component to add to the "
        "hatch boundary.'/>"
        "  <Option name='ENCODING' type='string' description="
        "'Encoding name, as supported by iconv, to override $DWGCODEPAGE'/>"
        "</OpenOptionList>");

    poDriver->SetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST,
                              "<LayerCreationOptionList/>");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_FEATURE_STYLES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_FEATURE_STYLES_READ, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_FEATURE_STYLES_WRITE, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES");

    poDriver->pfnOpen = OGRDXFDriverOpen;
    poDriver->pfnIdentify = OGRDXFDriverIdentify;
    poDriver->pfnCreate = OGRDXFDriverCreate;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}

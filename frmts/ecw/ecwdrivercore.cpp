/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  ECW driver
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

// ncsjpcbuffer.h needs the min and max macros.
#undef NOMINMAX

#include "ecwdrivercore.h"

#include "ecwsdk_headers.h"

constexpr unsigned char jpc_header[] = {0xff, 0x4f, 0xff,
                                        0x51};  // SOC + RSIZ markers
constexpr unsigned char jp2_header[] = {0x00, 0x00, 0x00, 0x0c, 0x6a, 0x50,
                                        0x20, 0x20, 0x0d, 0x0a, 0x87, 0x0a};

/* Needed for v4.3 and v5.0 */
#if !defined(NCS_ECWSDK_VERSION_STRING) && defined(NCS_ECWJP2_VERSION_STRING)
#define NCS_ECWSDK_VERSION_STRING NCS_ECWJP2_VERSION_STRING
#endif

/************************************************************************/
/*                           IdentifyECW()                              */
/*                                                                      */
/*      Identify method that only supports ECW files.                   */
/************************************************************************/

int ECWDatasetIdentifyECW(GDALOpenInfo *poOpenInfo)

{
    /* -------------------------------------------------------------------- */
    /*      This has to either be a file on disk ending in .ecw or a        */
    /*      ecwp: protocol url.                                             */
    /* -------------------------------------------------------------------- */
    if ((!EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "ecw") ||
         poOpenInfo->nHeaderBytes == 0) &&
        !STARTS_WITH_CI(poOpenInfo->pszFilename, "ecwp:") &&
        !STARTS_WITH_CI(poOpenInfo->pszFilename, "ecwps:"))
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                    ECWDriverSetCommonMetadata()                      */
/************************************************************************/

void ECWDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(ECW_DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");

    CPLString osLongName = "ERDAS Compressed Wavelets (SDK ";

#ifdef NCS_ECWSDK_VERSION_STRING
    osLongName += NCS_ECWSDK_VERSION_STRING;
#else
    osLongName += "3.x";
#endif
    osLongName += ")";

    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, osLongName);
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/ecw.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "ecw");

    poDriver->pfnIdentify = ECWDatasetIdentifyECW;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
#ifdef HAVE_COMPRESS
    // The create method does not work with SDK 3.3 ( crash in
    // CNCSJP2FileView::WriteLineBIL() due to m_pFile being nullptr ).
#if ECWSDK_VERSION >= 50
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
#endif
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
#if ECWSDK_VERSION >= 50
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Byte UInt16");
#else
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Byte");
#endif
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "   <Option name='TARGET' type='float' description='Compression "
        "Percentage' />"
        "   <Option name='PROJ' type='string' description='ECW Projection "
        "Name'/>"
        "   <Option name='DATUM' type='string' description='ECW Datum Name' />"

#if ECWSDK_VERSION < 40
        "   <Option name='LARGE_OK' type='boolean' description='Enable "
        "compressing 500+MB files'/>"
#else
        "   <Option name='ECW_ENCODE_KEY' type='string' description='OEM "
        "Compress Key from ERDAS.'/>"
        "   <Option name='ECW_ENCODE_COMPANY' type='string' description='OEM "
        "Company Name.'/>"
#endif

#if ECWSDK_VERSION >= 50
        "   <Option name='ECW_FORMAT_VERSION' type='integer' description='ECW "
        "format version (2 or 3).' default='2'/>"
#endif

        "</CreationOptionList>");
#else
    // In read-only mode, we support VirtualIO. This is not the case
    // for ECWCreateCopyECW().
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
#endif
}

/************************************************************************/
/*                        IdentifyJPEG2000()                            */
/*                                                                      */
/*          Identify method that only supports JPEG2000 files.          */
/************************************************************************/

int ECWDatasetIdentifyJPEG2000(GDALOpenInfo *poOpenInfo)

{
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "J2K_SUBFILE:"))
        return TRUE;

    else if (poOpenInfo->nHeaderBytes >= 16 &&
             (memcmp(poOpenInfo->pabyHeader, jpc_header, sizeof(jpc_header)) ==
                  0 ||
              memcmp(poOpenInfo->pabyHeader, jp2_header, sizeof(jp2_header)) ==
                  0))
        return TRUE;

    else
        return FALSE;
}

/************************************************************************/
/*                 JP2ECWDriverSetCommonMetadata()                      */
/************************************************************************/

void JP2ECWDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(JP2ECW_DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");

    CPLString osLongName = "ERDAS JPEG2000 (SDK ";

#ifdef NCS_ECWSDK_VERSION_STRING
    osLongName += NCS_ECWSDK_VERSION_STRING;
#else
    osLongName += "3.x";
#endif
    osLongName += ")";

    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, osLongName);
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/jp2ecw.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "jp2");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnIdentify = ECWDatasetIdentifyJPEG2000;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "   <Option name='1BIT_ALPHA_PROMOTION' type='boolean' "
        "description='Whether a 1-bit alpha channel should be promoted to "
        "8-bit' default='YES'/>"
        "   <Option name='OPEN_REMOTE_GML' type='boolean' description='Whether "
        "to load remote vector layers referenced by a link in a GMLJP2 v2 box' "
        "default='NO'/>"
        "   <Option name='GEOREF_SOURCES' type='string' description='Comma "
        "separated list made with values "
        "INTERNAL/GMLJP2/GEOJP2/WORLDFILE/PAM/NONE that describe the priority "
        "order for georeferencing' default='PAM,GEOJP2,GMLJP2,WORLDFILE'/>"
        "</OpenOptionList>");

#ifdef HAVE_COMPRESS
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONDATATYPES,
        "Byte UInt16 Int16 UInt32 Int32 "
        "Float32 "
#if ECWSDK_VERSION >= 40
        // Crashes for sure with 3.3. Didn't try other versions
        "Float64"
#endif
    );
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "   <Option name='TARGET' type='float' description='Compression "
        "Percentage' />"
        "   <Option name='PROJ' type='string' description='ECW Projection "
        "Name'/>"
        "   <Option name='DATUM' type='string' description='ECW Datum Name' />"
        "   <Option name='UNITS' type='string-select' description='ECW "
        "Projection Units'>"
        "       <Value>METERS</Value>"
        "       <Value>FEET</Value>"
        "   </Option>"

#if ECWSDK_VERSION < 40
        "   <Option name='LARGE_OK' type='boolean' description='Enable "
        "compressing 500+MB files'/>"
#else
        "   <Option name='ECW_ENCODE_KEY' type='string' description='OEM "
        "Compress Key from ERDAS.'/>"
        "   <Option name='ECW_ENCODE_COMPANY' type='string' description='OEM "
        "Company Name.'/>"
#endif

        "   <Option name='GeoJP2' type='boolean' description='defaults to ON'/>"
        "   <Option name='GMLJP2' type='boolean' description='defaults to ON'/>"
        "   <Option name='GMLJP2V2_DEF' type='string' description='Definition "
        "file to describe how a GMLJP2 v2 box should be generated. If set to "
        "YES, a minimal instance will be created'/>"
        "   <Option name='PROFILE' type='string-select'>"
        "       <Value>BASELINE_0</Value>"
        "       <Value>BASELINE_1</Value>"
        "       <Value>BASELINE_2</Value>"
        "       <Value>NPJE</Value>"
        "       <Value>EPJE</Value>"
        "   </Option>"
        "   <Option name='PROGRESSION' type='string-select'>"
        "       <Value>LRCP</Value>"
        "       <Value>RLCP</Value>"
        "       <Value>RPCL</Value>"
        "   </Option>"
        "   <Option name='CODESTREAM_ONLY' type='boolean' description='No JP2 "
        "wrapper'/>"
        "   <Option name='NBITS' type='int' description='Bits (precision) for "
        "sub-byte files (1-7), sub-uint16 (9-15)'/>"
        "   <Option name='LEVELS' type='int'/>"
        "   <Option name='LAYERS' type='int'/>"
        "   <Option name='PRECINCT_WIDTH' type='int'/>"
        "   <Option name='PRECINCT_HEIGHT' type='int'/>"
        "   <Option name='TILE_WIDTH' type='int'/>"
        "   <Option name='TILE_HEIGHT' type='int'/>"
        "   <Option name='INCLUDE_SOP' type='boolean'/>"
        "   <Option name='INCLUDE_EPH' type='boolean'/>"
        "   <Option name='DECOMPRESS_LAYERS' type='int'/>"
        "   <Option name='DECOMPRESS_RECONSTRUCTION_PARAMETER' type='float'/>"
        "   <Option name='WRITE_METADATA' type='boolean' description='Whether "
        "metadata should be written, in a dedicated JP2 XML box' default='NO'/>"
        "   <Option name='MAIN_MD_DOMAIN_ONLY' type='boolean' "
        "description='(Only if WRITE_METADATA=YES) Whether only metadata from "
        "the main domain should be written' default='NO'/>"
        "</CreationOptionList>");
#endif
}

/************************************************************************/
/*                    DeclareDeferredECWPlugin()                        */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredECWPlugin()
{
    if (GDALGetDriverByName(ECW_DRIVER_NAME) != nullptr)
    {
        return;
    }
    {
        auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
        poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                                  PLUGIN_INSTALLATION_MESSAGE);
#endif
        ECWDriverSetCommonMetadata(poDriver);
        GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
    }
    {
        auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
        poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                                  PLUGIN_INSTALLATION_MESSAGE);
#endif
        JP2ECWDriverSetCommonMetadata(poDriver);
        GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
    }
}
#endif

/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  JP2KAK driver
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

#include "jp2kakdrivercore.h"

#include "jp2kak_headers.h"

/************************************************************************/
/*                   JP2KAKDriverSetCommonMetadata()                    */
/************************************************************************/

void JP2KAKDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(
        GDAL_DMD_LONGNAME, "JPEG-2000 (based on Kakadu " KDU_CORE_VERSION ")");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/jp2kak.html");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
                              "Byte Int16 UInt16 Int32 UInt32");
    poDriver->SetMetadataItem(GDAL_DMD_MIMETYPE, "image/jp2");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "jp2 j2k");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "   <Option name='1BIT_ALPHA_PROMOTION' type='boolean' description="
        "'Whether a 1-bit alpha channel should be promoted to 8-bit' "
        "default='YES'/>"
        "   <Option name='OPEN_REMOTE_GML' type='boolean' description="
        "'Whether to load remote vector layers referenced by "
        "a link in a GMLJP2 v2 box' default='NO'/>"
        "   <Option name='GEOREF_SOURCES' type='string' description="
        "'Comma separated list made with values "
        "INTERNAL/GMLJP2/GEOJP2/WORLDFILE/PAM/NONE that describe the priority "
        "order "
        "for georeferencing' default='PAM,GEOJP2,GMLJP2,WORLDFILE'/>"
        "</OpenOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "   <Option name='CODEC' type='string-select' "
        "default='according to file extension. If unknown, default to JP2'>"
        "       <Value>JP2</Value>"
        "       <Value>J2K</Value>"
        "   </Option>"
        "   <Option name='QUALITY' type='integer' description="
        "'0.01-100, 100 is lossless'/>"
        "   <Option name='BLOCKXSIZE' type='int' description='Tile Width'/>"
        "   <Option name='BLOCKYSIZE' type='int' description='Tile Height'/>"
        "   <Option name='GeoJP2' type='boolean' description='defaults to ON'/>"
        "   <Option name='GMLJP2' type='boolean' description='defaults to ON'/>"
        "   <Option name='GMLJP2V2_DEF' type='string' description="
        "'Definition file to describe how a GMLJP2 v2 box should be generated. "
        "If set to YES, a minimal instance will be created'/>"
        "   <Option name='LAYERS' type='integer'/>"
#ifdef KDU_HAS_ROI_RECT
        "   <Option name='ROI' type='string'/>"
#endif
        "   <Option name='COMSEG' type='boolean' />"
        "   <Option name='FLUSH' type='boolean' />"
        "   <Option name='NBITS' type='int' description="
        "'BITS (precision) for sub-byte files (1-7), sub-uint16 (9-15)'/>"
        "   <Option name='RATE' type='string' description='bit-rates separated "
        "by commas'/>"
        "   <Option name='Creversible' type='boolean'/>"
        "   <Option name='Corder' type='string'/>"
        "   <Option name='Cprecincts' type='string'/>"
        "   <Option name='Cmodes' type='string'/>"
        "   <Option name='Clevels' type='string'/>"
        "   <Option name='ORGgen_plt' type='string'/>"
        "   <Option name='ORGgen_tlm' type='string'/>"
        "   <Option name='ORGtparts' type='string'/>"
        "   <Option name='Qguard' type='integer'/>"
        "   <Option name='Sprofile' type='string'/>"
        "   <Option name='Rshift' type='string'/>"
        "   <Option name='Rlevels' type='string'/>"
        "   <Option name='Rweight' type='string'/>"
        "</CreationOptionList>");

    poDriver->pfnIdentify = JP2KAKDatasetIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int JP2KAKDatasetIdentify(GDALOpenInfo *poOpenInfo)

{
    // Check header.
    if (poOpenInfo->nHeaderBytes < static_cast<int>(sizeof(jp2_header)))
    {
        if ((STARTS_WITH_CI(poOpenInfo->pszFilename, "http://") ||
             STARTS_WITH_CI(poOpenInfo->pszFilename, "https://") ||
             STARTS_WITH_CI(poOpenInfo->pszFilename, "jpip://")) &&
            EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "jp2"))
        {
#ifdef USE_JPIP
            return TRUE;
#else
            return FALSE;
#endif
        }
        else if (STARTS_WITH_CI(poOpenInfo->pszFilename, "J2K_SUBFILE:"))
            return TRUE;
        else
            return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      Any extension is supported for JP2 files.  Only selected        */
    /*      extensions are supported for JPC files since the standard       */
    /*      prefix is so short (two bytes).                                 */
    /* -------------------------------------------------------------------- */
    if (memcmp(poOpenInfo->pabyHeader, jp2_header, sizeof(jp2_header)) == 0)
        return TRUE;
    else if (memcmp(poOpenInfo->pabyHeader, jpc_header, sizeof(jpc_header)) ==
             0)
    {
        const char *const pszExtension =
            CPLGetExtension(poOpenInfo->pszFilename);
        if (EQUAL(pszExtension, "jpc") || EQUAL(pszExtension, "j2k") ||
            EQUAL(pszExtension, "jp2") || EQUAL(pszExtension, "jpx") ||
            EQUAL(pszExtension, "j2c") || EQUAL(pszExtension, "jhc"))
            return TRUE;

        // We also want to handle jpc datastreams vis /vsisubfile.
        if (strstr(poOpenInfo->pszFilename, "vsisubfile") != nullptr)
            return TRUE;
    }

    return FALSE;
}

/************************************************************************/
/*                    DeclareDeferredJP2KAKPlugin()                     */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredJP2KAKPlugin()
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
    JP2KAKDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif

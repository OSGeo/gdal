/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  OPENJPEG driver
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

#include "openjpegdrivercore.h"

/* This file is to be used with openjpeg 2.1 or later */
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wdocumentation"
#endif

#include <openjpeg.h>
#include <opj_config.h>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#define IS_OPENJPEG_OR_LATER(major, minor, patch)                              \
    ((OPJ_VERSION_MAJOR * 10000 + OPJ_VERSION_MINOR * 100 +                    \
      OPJ_VERSION_BUILD) >= ((major)*10000 + (minor)*100 + (patch)))

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

#ifndef jpc_header_defined
#define jpc_header_defined
static const unsigned char jpc_header[] = {0xff, 0x4f, 0xff,
                                           0x51};  // SOC + RSIZ markers
static const unsigned char jp2_box_jp[] = {0x6a, 0x50, 0x20, 0x20}; /* 'jP  ' */
#endif

static int Identify(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->nHeaderBytes >= 16 &&
        (memcmp(poOpenInfo->pabyHeader, jpc_header, sizeof(jpc_header)) == 0 ||
         memcmp(poOpenInfo->pabyHeader + 4, jp2_box_jp, sizeof(jp2_box_jp)) ==
             0))
        return TRUE;

    else
        return FALSE;
}

/************************************************************************/
/*                  OPENJPEGDriverSetCommonMetadata()                   */
/************************************************************************/

void OPENJPEGDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "JPEG-2000 driver based on JP2OpenJPEG library");

    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/raster/jp2openjpeg.html");
    poDriver->SetMetadataItem(GDAL_DMD_MIMETYPE, "image/jp2");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "jp2");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "jp2 j2k");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
                              "Byte Int16 UInt16 Int32 UInt32");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
#if IS_OPENJPEG_OR_LATER(2, 5, 0)
        "   <Option name='STRICT' type='boolean' description='Whether "
        "strict/pedantic decoding should be adopted. Set to NO to allow "
        "decoding broken files' default='YES'/>"
#endif
        "   <Option name='1BIT_ALPHA_PROMOTION' type='boolean' "
        "description='Whether a 1-bit alpha channel should be promoted to "
        "8-bit' default='YES'/>"
        "   <Option name='OPEN_REMOTE_GML' type='boolean' "
        "description='Whether "
        "to load remote vector layers referenced by a link in a GMLJP2 v2 "
        "box' "
        "default='NO'/>"
        "   <Option name='GEOREF_SOURCES' type='string' description='Comma "
        "separated list made with values "
        "INTERNAL/GMLJP2/GEOJP2/WORLDFILE/PAM/NONE that describe the "
        "priority "
        "order for georeferencing' default='PAM,GEOJP2,GMLJP2,WORLDFILE'/>"
        "   <Option name='USE_TILE_AS_BLOCK' type='boolean' "
        "description='Whether to always use the JPEG-2000 block size as "
        "the "
        "GDAL block size' default='NO'/>"
        "</OpenOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "   <Option name='CODEC' type='string-select' default='according "
        "to "
        "file extension. If unknown, default to J2K'>"
        "       <Value>JP2</Value>"
        "       <Value>J2K</Value>"
        "   </Option>"
        "   <Option name='GeoJP2' type='boolean' description='Whether to "
        "emit "
        "a GeoJP2 box' default='YES'/>"
        "   <Option name='GMLJP2' type='boolean' description='Whether to "
        "emit "
        "a GMLJP2 v1 box' default='YES'/>"
        "   <Option name='GMLJP2V2_DEF' type='string' "
        "description='Definition "
        "file to describe how a GMLJP2 v2 box should be generated. If set "
        "to "
        "YES, a minimal instance will be created'/>"
        "   <Option name='QUALITY' type='string' description='Single "
        "quality "
        "value or comma separated list of increasing quality values for "
        "several layers, each in the 0-100 range' default='25'/>"
        "   <Option name='REVERSIBLE' type='boolean' description='True if "
        "the "
        "compression is reversible' default='false'/>"
        "   <Option name='RESOLUTIONS' type='int' description='Number of "
        "resolutions.' min='1' max='30'/>"
        "   <Option name='BLOCKXSIZE' type='int' description='Tile Width' "
        "default='1024'/>"
        "   <Option name='BLOCKYSIZE' type='int' description='Tile Height' "
        "default='1024'/>"
        "   <Option name='PROGRESSION' type='string-select' default='LRCP'>"
        "       <Value>LRCP</Value>"
        "       <Value>RLCP</Value>"
        "       <Value>RPCL</Value>"
        "       <Value>PCRL</Value>"
        "       <Value>CPRL</Value>"
        "   </Option>"
        "   <Option name='SOP' type='boolean' description='True to insert "
        "SOP "
        "markers' default='false'/>"
        "   <Option name='EPH' type='boolean' description='True to insert "
        "EPH "
        "markers' default='false'/>"
        "   <Option name='YCBCR420' type='boolean' description='if RGB "
        "must be "
        "resampled to YCbCr 4:2:0' default='false'/>"
        "   <Option name='YCC' type='boolean' description='if RGB must be "
        "transformed to YCC color space (lossless MCT transform)' "
        "default='YES'/>"
        "   <Option name='NBITS' type='int' description='Bits (precision) "
        "for "
        "sub-byte files (1-7), sub-uint16 (9-15), sub-uint32 (17-31)'/>"
        "   <Option name='1BIT_ALPHA' type='boolean' description='Whether "
        "to "
        "encode the alpha channel as a 1-bit channel' default='NO'/>"
        "   <Option name='ALPHA' type='boolean' description='Whether to "
        "force "
        "encoding last channel as alpha channel' default='NO'/>"
        "   <Option name='PROFILE' type='string-select' description='Which "
        "codestream profile to use' default='AUTO'>"
        "       <Value>AUTO</Value>"
        "       <Value>UNRESTRICTED</Value>"
        "       <Value>PROFILE_1</Value>"
        "   </Option>"
        "   <Option name='INSPIRE_TG' type='boolean' description='Whether "
        "to "
        "use features that comply with Inspire Orthoimagery Technical "
        "Guidelines' default='NO'/>"
        "   <Option name='JPX' type='boolean' description='Whether to "
        "advertise JPX features when a GMLJP2 box is written (or use JPX "
        "branding if GMLJP2 v2)' default='YES'/>"
        "   <Option name='GEOBOXES_AFTER_JP2C' type='boolean' "
        "description='Whether to place GeoJP2/GMLJP2 boxes after the "
        "code-stream' default='NO'/>"
        "   <Option name='PRECINCTS' type='string' description='Precincts "
        "size "
        "as a string of the form {w,h},{w,h},... with power-of-two "
        "values'/>"
        "   <Option name='TILEPARTS' type='string-select' "
        "description='Whether "
        "to generate tile-parts and according to which criterion' "
        "default='DISABLED'>"
        "       <Value>DISABLED</Value>"
        "       <Value>RESOLUTIONS</Value>"
        "       <Value>LAYERS</Value>"
        "       <Value>COMPONENTS</Value>"
        "   </Option>"
        "   <Option name='CODEBLOCK_WIDTH' type='int' "
        "description='Codeblock "
        "width' default='64' min='4' max='1024'/>"
        "   <Option name='CODEBLOCK_HEIGHT' type='int' "
        "description='Codeblock "
        "height' default='64' min='4' max='1024'/>"
        "   <Option name='CT_COMPONENTS' type='int' min='3' max='4' "
        "description='If there is one color table, number of color table "
        "components to write. Autodetected if not specified.'/>"
        "   <Option name='WRITE_METADATA' type='boolean' "
        "description='Whether "
        "metadata should be written, in a dedicated JP2 XML box' "
        "default='NO'/>"
        "   <Option name='MAIN_MD_DOMAIN_ONLY' type='boolean' "
        "description='(Only if WRITE_METADATA=YES) Whether only metadata "
        "from "
        "the main domain should be written' default='NO'/>"
        "   <Option name='USE_SRC_CODESTREAM' type='boolean' "
        "description='When "
        "source dataset is JPEG2000, whether to reuse the codestream of "
        "the "
        "source dataset unmodified' default='NO'/>"
        "   <Option name='CODEBLOCK_STYLE' type='string' "
        "description='Comma-separated combination of BYPASS, RESET, "
        "TERMALL, "
        "VSC, PREDICTABLE, SEGSYM or value between 0 and 63'/>"
#if IS_OPENJPEG_OR_LATER(2, 4, 0)
        "   <Option name='PLT' type='boolean' description='True to insert "
        "PLT "
        "marker segments' default='false'/>"
#endif
#if IS_OPENJPEG_OR_LATER(2, 5, 0)
        "   <Option name='TLM' type='boolean' description='True to insert "
        "TLM "
        "marker segments' default='false'/>"
#endif
        "   <Option name='COMMENT' type='string' description='Content of "
        "the "
        "comment (COM) marker'/>"
        "</CreationOptionList>");

    poDriver->pfnIdentify = Identify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                  DeclareDeferredOPENJPEGPlugin()                     */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredOPENJPEGPlugin()
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
    OPENJPEGDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif

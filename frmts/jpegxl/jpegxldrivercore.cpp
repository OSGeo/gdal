/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  JPEGXL driver
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

#include "jpegxldrivercore.h"

/************************************************************************/
/*                      IsJPEGXLContainer()                             */
/************************************************************************/

bool IsJPEGXLContainer(GDALOpenInfo *poOpenInfo)
{
    constexpr const GByte abyJXLContainerSignature[] = {
        0x00, 0x00, 0x00, 0x0C, 'J', 'X', 'L', ' ', 0x0D, 0x0A, 0x87, 0x0A};
    return (poOpenInfo->nHeaderBytes >=
                static_cast<int>(sizeof(abyJXLContainerSignature)) &&
            memcmp(poOpenInfo->pabyHeader, abyJXLContainerSignature,
                   sizeof(abyJXLContainerSignature)) == 0);
}

/************************************************************************/
/*                JPEGXLDatasetIdentifyPartial()                        */
/************************************************************************/

static int JPEGXLDatasetIdentifyPartial(GDALOpenInfo *poOpenInfo)
{
    if (poOpenInfo->fpL == nullptr)
        return false;

    if (EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "jxl"))
        return true;

    // See
    // https://github.com/libjxl/libjxl/blob/c98f133f3f5e456caaa2ba00bc920e923b713abc/lib/jxl/decode.cc#L107-L138

    // JPEG XL codestream
    if (poOpenInfo->nHeaderBytes >= 2 && poOpenInfo->pabyHeader[0] == 0xff &&
        poOpenInfo->pabyHeader[1] == 0x0a)
    {
        // Two bytes is not enough to reliably identify
        // JPEGXLDataset::Identify() does a bit more work then
        return GDAL_IDENTIFY_UNKNOWN;
    }

    return IsJPEGXLContainer(poOpenInfo);
}

/************************************************************************/
/*                   JPEGXLDriverSetCommonMetadata()                    */
/************************************************************************/

void JPEGXLDriverSetCommonMetadata(GDALDriver *poDriver)
{
    // Set the driver details.
    poDriver->SetDescription(DRIVER_NAME);

    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "JPEG-XL");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/jpegxl.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "jxl");
    poDriver->SetMetadataItem(GDAL_DMD_MIMETYPE, "image/jxl");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
                              "Byte UInt16 Float32");

#ifdef HAVE_JXL_BOX_API
    const char *pszOpenOptions =
        "<OpenOptionList>\n"
        "   <Option name='APPLY_ORIENTATION' type='boolean' "
        "description='whether to take into account EXIF Orientation to "
        "rotate/flip the image' default='NO'/>\n"
        "</OpenOptionList>\n";
    poDriver->SetMetadataItem(GDAL_DMD_OPENOPTIONLIST, pszOpenOptions);
#endif

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>\n"
        "   <Option name='LOSSLESS' type='boolean' description='Whether JPEGXL "
        "compression should be lossless' default='YES'/>"
        "   <Option name='LOSSLESS_COPY' type='string-select' "
        "description='Whether conversion should be lossless' default='AUTO'>"
        "     <Value>AUTO</Value>"
        "     <Value>YES</Value>"
        "     <Value>NO</Value>"
        "   </Option>"
        "   <Option name='EFFORT' type='int' description='Level of effort "
        "1(fast)-9(slow)' default='5'/>"
        "   <Option name='DISTANCE' type='float' description='Distance level "
        "for lossy compression (0=mathematically lossless, 1.0=visually "
        "lossless, usual range [0.5,3])' default='1.0' min='0.1' max='15.0'/>"
#ifdef HAVE_JxlEncoderSetExtraChannelDistance
        "  <Option name='ALPHA_DISTANCE' type='float' "
        "description='Distance level for alpha channel "
        "(-1=same as non-alpha channels, "
        "0=mathematically lossless, 1.0=visually lossless, "
        "usual range [0.5,3])' default='-1' min='-1' max='15.0'/>"
#endif
        "   <Option name='QUALITY' type='float' description='Alternative "
        "setting to DISTANCE to specify lossy compression, roughly matching "
        "libjpeg quality setting in the [0,100] range' default='90' max='100'/>"
        "   <Option name='NBITS' type='int' description='BITS for sub-byte "
        "files (1-7), sub-uint16_t (9-15)'/>"
        "   <Option name='SOURCE_ICC_PROFILE' description='ICC profile encoded "
        "in Base64' type='string'/>\n"
#ifdef HAVE_JXL_THREADS
        "   <Option name='NUM_THREADS' type='string' description='Number of "
        "worker threads for compression. Can be set to ALL_CPUS' "
        "default='ALL_CPUS'/>"
#endif
#ifdef HAVE_JXL_BOX_API
        "   <Option name='WRITE_EXIF_METADATA' type='boolean' "
        "description='Whether to write EXIF_ metadata in a Exif box' "
        "default='YES'/>"
        "   <Option name='WRITE_XMP' type='boolean' description='Whether to "
        "write xml:XMP metadata in a xml box' default='YES'/>"
        "   <Option name='WRITE_GEOJP2' type='boolean' description='Whether to "
        "write georeferencing in a jumb.uuid box' default='YES'/>"
        "   <Option name='COMPRESS_BOXES' type='boolean' description='Whether "
        "to decompress Exif/XMP/GeoJP2 boxes' default='NO'/>"
#endif
        "</CreationOptionList>\n");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

#ifdef HAVE_JxlEncoderInitExtraChannelInfo
    poDriver->SetMetadataItem("JXL_ENCODER_SUPPORT_EXTRA_CHANNELS", "YES");
#endif

    poDriver->pfnIdentify = JPEGXLDatasetIdentifyPartial;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                    DeclareDeferredJPEGXLPlugin()                     */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredJPEGXLPlugin()
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
    JPEGXLDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif

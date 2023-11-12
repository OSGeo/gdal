/******************************************************************************
 * Project:  GDAL
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 * Purpose:
 * JPEG-2000 driver based on Lurawave library, driver developed by SatCen
 *
 ******************************************************************************
 * Copyright (c) 2016, SatCen - European Union Satellite Centre
 * Copyright (c) 2014-2016, Even Rouault
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

#include "jp2luradrivercore.h"

/************************************************************************/
/*                     JP2LuraDriverIdentify()                          */
/************************************************************************/

int JP2LuraDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "JP2Lura:"))
        return true;

    // Check magic number
    return poOpenInfo->fpL != nullptr && poOpenInfo->nHeaderBytes >= 4 &&
           poOpenInfo->pabyHeader[0] == 0x76 &&
           poOpenInfo->pabyHeader[1] == 0x2f &&
           poOpenInfo->pabyHeader[2] == 0x31 &&
           poOpenInfo->pabyHeader[3] == 0x01;
}

/************************************************************************/
/*                   JP2LuraDriverSetCommonMetadata()                   */
/************************************************************************/

void JP2LuraDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "JPEG-2000 driver based on Lurawave library");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/raster/jp2lura.html");
    poDriver->SetMetadataItem(GDAL_DMD_MIMETYPE, "image/jp2");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "jp2");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "jp2 j2f j2k");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
                              "Byte Int16 UInt16 Int32 UInt32 Float32");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "   <Option name='OPEN_REMOTE_GML' type='boolean' description="
        "'Whether to load remote vector layers referenced by a link in a "
        "GMLJP2 v2 box' default='NO'/>"
        "   <Option name='GEOREF_SOURCES' type='string' description="
        "'Comma separated list made with values INTERNAL/GMLJP2/GEOJP2/"
        "WORLDFILE/PAM/NONE that describe the priority order for "
        "georeferencing' default='PAM,GEOJP2,GMLJP2,WORLDFILE'/>"
        "</OpenOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "   <Option name='CODEC' type='string-select' description="
        "'Codec to use. Default according to file extension. "
        "If unknown, default to JP2'>"
        "       <Value>JP2</Value>"
        "       <Value alias='J2K'>Codestream</Value>"
        "   </Option>"
        "   <Option name='JPX' type='boolean' description="
        "'Whether to advertise JPX features when a GMLJP2 box is written "
        "(or use JPX branding if GMLJP2 v2)' default='YES'/>"
        "   <Option name='GeoJP2' type='boolean' description="
        "'Whether to emit a GeoJP2 box' default='NO'/>"
        "   <Option name='GMLJP2' type='boolean' description="
        "'Whether to emit a GMLJP2 v1 box' default='YES'/>"
        "   <Option name='GMLJP2V2_DEF' type='string' description="
        "'Definition file to describe how a GMLJP2 v2 box should be "
        "generated. "
        "If set to YES, a minimal instance will be created'/>"
        "   <Option name='SPLIT_IEEE754' type='boolean' description="
        "'Whether encoding of Float32 bands as 3 bands with IEEE754 sign "
        "bit, "
        "exponent, mantissa values (non standard extension)' default='NO'/>"
        "   <Option name='QUALITY_STYLE' type='string-select' description="
        "'This property tag is used to set the quality mode to be used "
        "during "
        "lossy compression.For normal images and situations (1:1 pixel "
        "display,"
        " ~50 cm viewing distance) we recommend Small or PSNR. For quality "
        "measurement only PSNR should be used' default='PSNR'>"
        "       <Value>PSNR</Value>"
        "       <Value>XXSmall</Value>"
        "       <Value>XSmall</Value>"
        "       <Value>Small</Value>"
        "       <Value>Medium</Value>"
        "       <Value>Large</Value>"
        "       <Value>XLarge</Value>"
        "       <Value>XXLarge</Value>"
        "   </Option>"
        "   <Option name='SPEED_MODE' type='string-select' description="
        "'This property tag is used to set the speed mode to be used "
        "during lossy compression. The following modes are defined' "
        "default='Fast'>"
        "       <Value>Fast</Value>"
        "       <Value>Accurate</Value>"
        "   </Option>"
        "   <Option name='RATE' type='int' description='"
        "When specifying this value, the target compressed file size will "
        "be "
        "the uncompressed file size divided by RATE. In general the "
        "achieved rate will be exactly the requested size or a few bytes "
        "lower. Will force use of irreversible wavelet. "
        "Default value: 0 (maximum quality).' default='0'/>"
        "   <Option name='QUALITY' type='int' description="
        "'Compression to a particular quality is possible only when using "
        "the 9-7 filter with the standard expounded quantization and no "
        "regions"
        "of interest. A compression quality may be specified between 1 "
        "(low) "
        "and 100 (high). The size of the resulting JPEG2000 file will "
        "depend "
        "of the image content. Only used for irreversible compression. "
        "The compression quality cannot be used together "
        "the property RATE. Default value: 0 (maximum quality).' "
        "min='0' max='100' default='0'/>"
        "   <Option name='PRECISION' type='int' description="
        "'For improved efficiency, the library automatically, depending on "
        "the "
        "image depth, uses either 16 or 32 bit representation for wavelet "
        "coefficients. The precision property can be set to force the "
        "library "
        "to always use 32 bit representations. The use of 32 bit values "
        "may "
        "slightly improve image quality and the expense of speed and "
        "memory "
        "requirements. Default value: 0 (automatically select appropriate "
        "precision)' default='0'/>"
        "   <Option name='PROGRESSION' type='string-select' description="
        "'The organization of the coded data in the file can be set by "
        "this "
        "property tag. The following progression orders are defined: "
        "LRCP = Quality progressive, "
        "RLCP = Resolution then quality progressive, "
        "RPCL = Resolution then position progressive, "
        "PCRL = Position progressive, "
        "CPRL = Color/channel progressive. "
        "The setting LRCP (quality) is most useful when used with several "
        "layers. The PCRL (position) should be used with precincts.' "
        "default='LRCP'>"
        "       <Value>LRCP</Value>"
        "       <Value>RLCP</Value>"
        "       <Value>RPCL</Value>"
        "       <Value>PCRL</Value>"
        "       <Value>CPRL</Value>"
        "   </Option>"
        "   <Option name='REVERSIBLE' type='boolean' description="
        "'The reversible (Filter 5_3) and irreversible (Filter 9_7), may "
        "be "
        "selected using this property.' default='FALSE'/>"
        "   <Option name='LEVELS' type='int' description="
        "'The number of wavelet transformation levels can be set using "
        "this "
        "property. Valid values are in the range 0 (no wavelet analysis) "
        "to "
        "16 (very fine analysis). The memory requirements and compression "
        "time "
        "increases with the number of transformation levels. A reasonable "
        "number of transformation levels is in the 4-6 range.' "
        "min='0' max='16' default='5'/>"
        "   <Option name='QUANTIZATION_STYLE' type='string-select' "
        "description="
        "'This property may only be set when the irreversible filter (9_7) "
        "is "
        "used. The quantization steps can either be derived from a bases "
        "quantization step, DERIVED, or calculated for each image "
        "sub-band, "
        "EXPOUNDED.The EXPOUNDED style is recommended when using the "
        "irreversible filter.' default='EXPOUNDED'>"
        "       <Value>DERIVED</Value>"
        "       <Value>EXPOUNDED</Value>"
        "   </Option>"
        "   <Option name='TILEXSIZE' type='int' description="
        "'Tile Width. An image can  be split into smaller tiles, with each "
        "tile independently compressed. The basic tile size and the offset "
        "to "
        "the first tile on the virtual compression reference grid can be "
        "set "
        "using these properties. The first tile must contain the first "
        "image "
        "pixel. The tiling of an image is recommended only for very large "
        "images. Default value: (0) One Tile containing the complete image."
        "' default='0'/>"
        "   <Option name='TILEYSIZE' type='int' description="
        "'Tile Height. An image can be split into smaller tiles, with each "
        "tile independently compressed. The basic tile size and the offset "
        "to "
        "the first tile on the virtual compression reference grid can be "
        "set "
        "using these properties. The first tile must contain the first "
        "image "
        "pixel. The tiling of an image is recommended only for very large "
        "images. Default value: (0) One Tile containing the complete image."
        "' default='0'/>"
        "   <Option name='TLM' type='boolean' description="
        "'The efficiency of decoding regions in a tiled image may be "
        "improved by "
        "the usage of a tile length marker. Tile length markers contain "
        "the "
        "position of each tile in a JPEG2000 codestream, enabling faster "
        "access "
        "to tiled data.' default='FALSE'/>"
        "   <Option name='CODEBLOCK_WIDTH' type='int' description="
        "'The size of the blocks of data coded with the arithmetic entropy "
        "coder may be set using these parameters. A codeblock may contain "
        "no "
        "more than  4096 (result of CODEBLOCK_WIDTH x CODEBLOCK_HEIGHT) "
        "samples. Smaller codeblocks can aid the decoding of regions of an "
        "image and error resilience.' min='4' max='1024' default='64'/>"
        "   <Option name='CODEBLOCK_HEIGHT' type='int' description="
        "'The size of the blocks of data coded with the arithmetic entropy "
        "coder may be set using these parameters. A codeblock may contain "
        "no "
        "more than  4096 (result of CODEBLOCK_WIDTH x CODEBLOCK_HEIGHT) "
        "samples. Smaller codeblocks can aid the decoding of regions of an "
        "image and error resilience.' min='4' max='1024' default='64'/>"
        "   <Option name='ERROR_RESILIENCE' type='boolean' description="
        "'This option improves error resilient in JPEG2000 streams or for "
        "special codecs (e.g. hardware coder) for a faster compression/"
        "decompression. This option will increase the file size slightly "
        "when "
        "generating a code stream with the same image quality.' "
        "default='NO'/>"
        "   <Option name='WRITE_METADATA' type='boolean' description="
        "'Whether metadata should be written, in a dedicated JP2 XML box' "
        "default='NO'/>"
        "   <Option name='MAIN_MD_DOMAIN_ONLY' type='boolean' description="
        "'(Only if WRITE_METADATA=YES) Whether only metadata from the main "
        "domain should be written' default='NO'/>"
        "   <Option name='USE_SRC_CODESTREAM' type='boolean' description="
        "'When source dataset is JPEG2000, whether to reuse the codestream "
        "of "
        "the source dataset unmodified' default='NO'/>"
        "   <Option name='NBITS' type='int' description="
        "'Bits (precision) for sub-byte files (1-7), sub-uint16 (9-15), "
        "sub-uint32 (17-28)'/>"
        "</CreationOptionList>");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnIdentify = JP2LuraDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                   DeclareDeferredJP2LuraPlugin()                     */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredJP2LuraPlugin()
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
    JP2LuraDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif

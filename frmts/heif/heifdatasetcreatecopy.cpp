/******************************************************************************
 *
 * Project:  HEIF Driver
 * Author:   Brad Hards <bradh@frogmouth.net>
 *
 ******************************************************************************
 * Copyright (c) 2023, Brad Hards <bradh@frogmouth.net>
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

#include "heifdataset.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <libheif/heif.h>
#include <memory>
#include <vector>

#include "cpl_error.h"

#ifdef HAS_CUSTOM_FILE_WRITER

// Same default as libheif encoder example
const int DEFAULT_QUALITY = 50;

static CPLErr mapColourInterpretation(GDALColorInterp colourInterpretation,
                                      heif_channel *channel)
{
    switch (colourInterpretation)
    {
        case GCI_RedBand:
            *channel = heif_channel_R;
            return CE_None;
        case GCI_GreenBand:
            *channel = heif_channel_G;
            return CE_None;
        case GCI_BlueBand:
            *channel = heif_channel_B;
            return CE_None;
        case GCI_AlphaBand:
            *channel = heif_channel_Alpha;
            return CE_None;
        default:
            return CE_Failure;
    }
}

static heif_compression_format getCompressionType(char **papszOptions)
{
    const char *pszValue = CSLFetchNameValue(papszOptions, "CODEC");
    if (pszValue == nullptr)
    {
        return heif_compression_HEVC;
    }
    if (strcmp(pszValue, "HEVC") == 0)
    {
        return heif_compression_HEVC;
    }
#if LIBHEIF_HAVE_VERSION(1, 7, 0)
    if (strcmp(pszValue, "AV1") == 0)
    {
        return heif_compression_AV1;
    }
#endif
#if LIBHEIF_HAVE_VERSION(1, 17, 0)
    if (strcmp(pszValue, "JPEG") == 0)
    {
        return heif_compression_JPEG;
    }
#endif
#if LIBHEIF_HAVE_VERSION(1, 17, 0)
    if (strcmp(pszValue, "JPEG2000") == 0)
    {
        return heif_compression_JPEG2000;
    }
#endif
#if LIBHEIF_HAVE_VERSION(1, 16, 0)
    if (strcmp(pszValue, "UNCOMPRESSED") == 0)
    {
        return heif_compression_uncompressed;
    }
#endif
    CPLError(CE_Warning, CPLE_IllegalArg,
             "CODEC=%s value not recognised, ignoring.", pszValue);
    return heif_compression_HEVC;
}

static void setEncoderParameters(heif_encoder *encoder, char **papszOptions)
{
    const char *pszValue = CSLFetchNameValue(papszOptions, "QUALITY");
    int nQuality = DEFAULT_QUALITY;
    if (pszValue != nullptr)
    {
        nQuality = atoi(pszValue);
        if ((nQuality < 0) || (nQuality > 100))
        {
            CPLError(CE_Warning, CPLE_IllegalArg,
                     "QUALITY=%s value not recognised, ignoring.", pszValue);
            nQuality = DEFAULT_QUALITY;
        }
    }
    heif_encoder_set_lossy_quality(encoder, nQuality);
}

heif_error
GDALHEIFDataset::VFS_WriterCallback(CPL_UNUSED struct heif_context *ctx,
                                    const void *data, size_t size,
                                    void *userdata)
{
    VSILFILE *fp = static_cast<VSILFILE *>(userdata);
    size_t bytesWritten = VSIFWriteL(data, 1, size, fp);
    if (bytesWritten == size)
    {
        return heif_error{.code = heif_error_Ok,
                          .subcode = heif_suberror_Unspecified,
                          .message = "Success"};
    }
    else
    {
        return heif_error{.code = heif_error_Encoding_error,
                          .subcode = heif_suberror_Cannot_write_output_data,
                          .message = "Not all data written"};
    }
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/
GDALDataset *
GDALHEIFDataset::CreateCopy(const char *pszFilename, GDALDataset *poSrcDS, int,
                            CPL_UNUSED char **papszOptions,
                            CPL_UNUSED GDALProgressFunc pfnProgress,
                            CPL_UNUSED void *pProgressData)
{
    if (pfnProgress == nullptr)
        pfnProgress = GDALDummyProgress;

    int nBands = poSrcDS->GetRasterCount();
    if ((nBands != 3) && (nBands != 4))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Driver only supports source dataset with 3 or 4 bands.\n");
        return nullptr;
    }
    // TODO: more sanity checks

    heif_context *ctx = heif_context_alloc();
    heif_encoder *encoder;
    heif_compression_format codec = getCompressionType(papszOptions);
    heif_context_get_encoder_for_format(ctx, codec, &encoder);

    setEncoderParameters(encoder, papszOptions);

    heif_image *image;
    struct heif_error err;
    err =
        heif_image_create(poSrcDS->GetRasterXSize(), poSrcDS->GetRasterYSize(),
                          heif_colorspace_RGB, heif_chroma_444, &image);
    if (err.code)
    {
        heif_encoder_release(encoder);
        heif_context_free(ctx);
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Failed to create libheif input image.\n");
        return nullptr;
    }

    for (auto &&poBand : poSrcDS->GetBands())
    {
        if (poBand->GetRasterDataType() != GDT_Byte)
        {
            heif_image_release(image);
            heif_encoder_release(encoder);
            heif_context_free(ctx);
            CPLError(CE_Failure, CPLE_AppDefined, "Unsupported data type.\n");
            return nullptr;
        }
        heif_channel channel;
        auto mapError =
            mapColourInterpretation(poBand->GetColorInterpretation(), &channel);
        if (mapError != CE_None)
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Driver does not support bands other than RGBA yet, "
                     "ignoring band.\n");
            continue;
        }
        err = heif_image_add_plane(image, channel, poSrcDS->GetRasterXSize(),
                                   poSrcDS->GetRasterYSize(), 8);
        if (err.code)
        {
            heif_image_release(image);
            heif_encoder_release(encoder);
            heif_context_free(ctx);
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to add image plane to libheif input image.\n");
            return nullptr;
        }
        int stride;
        uint8_t *p = heif_image_get_plane(image, channel, &stride);
        auto eErr = poBand->RasterIO(
            GF_Read, 0, 0, poSrcDS->GetRasterXSize(), poSrcDS->GetRasterYSize(),
            p, poSrcDS->GetRasterXSize(), poSrcDS->GetRasterYSize(), GDT_Byte,
            0, stride, nullptr);

        if (eErr != CE_None)
        {
            heif_image_release(image);
            heif_encoder_release(encoder);
            heif_context_free(ctx);
            return nullptr;
        }
    }

    // TODO: set options based on creation options
    heif_encoding_options *encoding_options = nullptr;

    heif_image_handle *out_image_handle;

    heif_context_encode_image(ctx, image, encoder, encoding_options,
                              &out_image_handle);

    heif_image_release(image);

    // TODO: set properties on output image
    heif_image_handle_release(out_image_handle);
    heif_encoding_options_free(encoding_options);
    heif_encoder_release(encoder);

    VSILFILE *fp = VSIFOpenL(pszFilename, "wb");
    if (fp == nullptr)
    {
        ReportError(pszFilename, CE_Failure, CPLE_OpenFailed,
                    "Unable to create file.");
        return nullptr;
    }
    heif_writer writer;
    writer.writer_api_version = 1;
    writer.write = VFS_WriterCallback;
    heif_context_write(ctx, &writer, fp);
    VSIFCloseL(fp);

    heif_context_free(ctx);

    return GDALDataset::Open(pszFilename);
}
#endif
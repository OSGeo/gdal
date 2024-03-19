/*
 * Copyright (c) 2021, Airbus DS Intelligence
 * Author: <even.rouault at spatialys.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that (i) the above copyright notices and this permission notice appear in
 * all copies of the software and related documentation, and (ii) the names of
 * Sam Leffler and Silicon Graphics may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of Sam Leffler and Silicon Graphics.
 *
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
 * ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include "tiffiop.h"
#include "tif_jxl.h"

#include <jxl/decode.h>
#include <jxl/encode.h>

#include <stdint.h>

#include <assert.h>

#define LSTATE_INIT_DECODE 0x01
#define LSTATE_INIT_ENCODE 0x02

/*
 * State block for each open TIFF file using JXL compression/decompression.
 */
typedef struct
{
    int state; /* state flags */

    int lossless;         /* TRUE (default) or FALSE */
    int effort;           /* 3 to 9. default: 7 */
    float distance;       /* 0 to 15. default: 1.0 */
    float alpha_distance; /* 0 to 15. default: -1.0 (same as distance) */

    uint32_t segment_width;
    uint32_t segment_height;

    unsigned int uncompressed_size;
    unsigned int uncompressed_alloc;
    uint8_t *uncompressed_buffer;
    unsigned int uncompressed_offset;

    JxlDecoder *decoder;

    TIFFVGetMethod vgetparent; /* super-class method */
    TIFFVSetMethod vsetparent; /* super-class method */
} JXLState;

#define LState(tif) ((JXLState *)(tif)->tif_data)
#define DecoderState(tif) LState(tif)
#define EncoderState(tif) LState(tif)

static int JXLEncode(TIFF *tif, uint8_t *bp, tmsize_t cc, uint16_t s);
static int JXLDecode(TIFF *tif, uint8_t *op, tmsize_t occ, uint16_t s);

static int GetJXLDataType(TIFF *tif)
{
    TIFFDirectory *td = &tif->tif_dir;
    static const char module[] = "GetJXLDataType";

    if (td->td_sampleformat == SAMPLEFORMAT_UINT && td->td_bitspersample == 8)
    {
        return JXL_TYPE_UINT8;
    }

    if (td->td_sampleformat == SAMPLEFORMAT_UINT && td->td_bitspersample == 16)
    {
        return JXL_TYPE_UINT16;
    }

    /* 20210903: Not supported yet by libjxl*/
    /*
    if( td->td_sampleformat == SAMPLEFORMAT_INT &&
            td->td_bitspersample == 32 )
    {
        return JXL_TYPE_UINT32;
    }
    */

    if (td->td_sampleformat == SAMPLEFORMAT_IEEEFP &&
        td->td_bitspersample == 32)
    {
        return JXL_TYPE_FLOAT;
    }

    TIFFErrorExtR(tif, module,
                  "Unsupported combination of SampleFormat and BitsPerSample");
    return -1;
}

static int GetJXLDataTypeSize(JxlDataType dtype)
{
    switch (dtype)
    {
        case JXL_TYPE_UINT8:
            return 1;
        case JXL_TYPE_UINT16:
            return 2;
        case JXL_TYPE_FLOAT:
            return 4;
        default:
            return 0;
    }
}

static int JXLFixupTags(TIFF *tif)
{
    (void)tif;
    return 1;
}

static int JXLSetupDecode(TIFF *tif)
{
    JXLState *sp = DecoderState(tif);

    assert(sp != NULL);

    /* if we were last encoding, terminate this mode */
    if (sp->state & LSTATE_INIT_ENCODE)
    {
        sp->state = 0;
    }

    sp->state |= LSTATE_INIT_DECODE;
    return 1;
}

static int SetupUncompressedBuffer(TIFF *tif, JXLState *sp, const char *module)
{
    TIFFDirectory *td = &tif->tif_dir;
    uint64_t new_size_64;
    uint64_t new_alloc_64;
    unsigned int new_size;
    unsigned int new_alloc;

    sp->uncompressed_offset = 0;

    if (isTiled(tif))
    {
        sp->segment_width = td->td_tilewidth;
        sp->segment_height = td->td_tilelength;
    }
    else
    {
        sp->segment_width = td->td_imagewidth;
        sp->segment_height = td->td_imagelength - tif->tif_row;
        if (sp->segment_height > td->td_rowsperstrip)
            sp->segment_height = td->td_rowsperstrip;
    }

    JxlDataType dtype = GetJXLDataType(tif);
    if (dtype < 0)
    {
        _TIFFfreeExt(tif, sp->uncompressed_buffer);
        sp->uncompressed_buffer = 0;
        sp->uncompressed_alloc = 0;
        return 0;
    }
    int nBytesPerSample = GetJXLDataTypeSize(dtype);
    new_size_64 =
        (uint64_t)sp->segment_width * sp->segment_height * nBytesPerSample;
    if (td->td_planarconfig == PLANARCONFIG_CONTIG)
    {
        new_size_64 *= td->td_samplesperpixel;
    }

    new_size = (unsigned int)new_size_64;
    sp->uncompressed_size = new_size;

    /* add some margin */
    new_alloc_64 = 100 + new_size_64 + new_size_64 / 3;
    new_alloc = (unsigned int)new_alloc_64;
    if (new_alloc != new_alloc_64)
    {
        TIFFErrorExtR(tif, module, "Too large uncompressed strip/tile");
        _TIFFfreeExt(tif, sp->uncompressed_buffer);
        sp->uncompressed_buffer = 0;
        sp->uncompressed_alloc = 0;
        return 0;
    }

    if (sp->uncompressed_alloc < new_alloc)
    {
        _TIFFfreeExt(tif, sp->uncompressed_buffer);
        sp->uncompressed_buffer = _TIFFmallocExt(tif, new_alloc);
        if (!sp->uncompressed_buffer)
        {
            TIFFErrorExtR(tif, module, "Cannot allocate buffer");
            _TIFFfreeExt(tif, sp->uncompressed_buffer);
            sp->uncompressed_buffer = 0;
            sp->uncompressed_alloc = 0;
            return 0;
        }
        sp->uncompressed_alloc = new_alloc;
    }

    return 1;
}

/*
 * Setup state for decoding a strip.
 */
static int JXLPreDecode(TIFF *tif, uint16_t s)
{
    static const char module[] = "JXLPreDecode";
    JXLState *sp = DecoderState(tif);
    TIFFDirectory *td = &tif->tif_dir;

    (void)s;
    assert(sp != NULL);
    if (sp->state != LSTATE_INIT_DECODE)
        tif->tif_setupdecode(tif);

    const int jxlDataType = GetJXLDataType(tif);
    if (jxlDataType < 0)
        return 0;

    if (!SetupUncompressedBuffer(tif, sp, module))
        return 0;

    if (sp->decoder == NULL)
    {
        sp->decoder = JxlDecoderCreate(NULL);
        if (sp->decoder == NULL)
        {
            TIFFErrorExtR(tif, module, "JxlDecoderCreate() failed");
            return 0;
        }
    }
    else
    {
        JxlDecoderReset(sp->decoder);
    }

    JxlDecoderStatus status;
    status = JxlDecoderSubscribeEvents(sp->decoder,
                                       JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE);
    if (status != JXL_DEC_SUCCESS)
    {
        TIFFErrorExtR(tif, module, "JxlDecoderSubscribeEvents() failed");
        return 0;
    }

    status = JxlDecoderSetInput(sp->decoder, (const uint8_t *)tif->tif_rawcp,
                                (size_t)tif->tif_rawcc);
    if (status != JXL_DEC_SUCCESS)
    {
        TIFFErrorExtR(tif, module, "JxlDecoderSetInput() failed with %d",
                      status);
        return 0;
    }

    status = JxlDecoderProcessInput(sp->decoder);
    if (status != JXL_DEC_BASIC_INFO)
    {
        TIFFErrorExtR(tif, module, "JxlDecoderProcessInput() failed with %d",
                      status);
        JxlDecoderReleaseInput(sp->decoder);
        return 0;
    }

    JxlBasicInfo info;
    status = JxlDecoderGetBasicInfo(sp->decoder, &info);
    if (status != JXL_DEC_SUCCESS)
    {
        TIFFErrorExtR(tif, module, "JxlDecoderGetBasicInfo() failed with %d",
                      status);
        JxlDecoderReleaseInput(sp->decoder);
        return 0;
    }

    if (sp->segment_width != info.xsize)
    {
        TIFFErrorExtR(tif, module,
                      "JXL basic info xsize = %d, whereas %u was expected",
                      info.xsize, sp->segment_width);
        JxlDecoderReleaseInput(sp->decoder);
        return 0;
    }

    if (sp->segment_height != info.ysize)
    {
        TIFFErrorExtR(tif, module,
                      "JXL basic info ysize = %d, whereas %u was expected",
                      info.ysize, sp->segment_height);
        JxlDecoderReleaseInput(sp->decoder);
        return 0;
    }

    if (td->td_bitspersample != info.bits_per_sample)
    {
        TIFFErrorExtR(
            tif, module,
            "JXL basic info bits_per_sample = %d, whereas %d was expected",
            info.bits_per_sample, td->td_bitspersample);
        JxlDecoderReleaseInput(sp->decoder);
        return 0;
    }

    if (td->td_planarconfig == PLANARCONFIG_CONTIG)
    {
        if (info.num_color_channels + info.num_extra_channels !=
            td->td_samplesperpixel)
        {
            TIFFErrorExtR(tif, module,
                          "JXL basic info invalid number of channels");
            JxlDecoderReleaseInput(sp->decoder);
            return 0;
        }
    }
    else
    {
        if (info.num_color_channels != 1 || info.alpha_bits > 0 ||
            info.num_extra_channels > 0)
        {
            TIFFErrorExtR(tif, module,
                          "JXL basic info invalid number of channels");
            JxlDecoderReleaseInput(sp->decoder);
            return 0;
        }
    }

    JxlPixelFormat format = {0};
    format.data_type = jxlDataType;
    format.endianness = JXL_NATIVE_ENDIAN;
    format.align = 0;
    // alpha_bits is set even for a gray, gray, Alpha, gray, gray
    // or for R, G, B, undefined, Alpha
    // Probably a defect of libjxl: https://github.com/libjxl/libjxl/issues/1773
    // So for num_color_channels==3, num_color_channels > 1 and
    // alpha_bits != 0, get information of the first extra channel to
    // check if it is alpha, to detect R, G, B, Alpha, undefined.
    // Note: there's no difference in the codestream if writing RGBAU
    // as num_channels == 3 with 2 extra channels the first one being
    // explicitly set to alpha, or with num_channels == 4.
    int bAlphaEmbedded = 0;
    if (info.alpha_bits != 0)
    {
        if ((info.num_color_channels == 3 || info.num_color_channels == 1) &&
            (info.num_extra_channels == 1))
        {
            bAlphaEmbedded = 1;
        }
        else if (info.num_color_channels == 3 && info.num_extra_channels > 1)
        {
            JxlExtraChannelInfo extra_channel_info;
            memset(&extra_channel_info, 0, sizeof(extra_channel_info));
            if (JxlDecoderGetExtraChannelInfo(
                    sp->decoder, 0, &extra_channel_info) == JXL_DEC_SUCCESS &&
                extra_channel_info.type == JXL_CHANNEL_ALPHA)
            {
                bAlphaEmbedded = 1;
            }
        }
    }
    uint32_t nFirstExtraChannel = (bAlphaEmbedded) ? 1 : 0;
    unsigned int main_buffer_size = sp->uncompressed_size;
    unsigned int channel_size = main_buffer_size / td->td_samplesperpixel;
    uint8_t *extra_channel_buffer = NULL;

    int nBytesPerSample = GetJXLDataTypeSize(format.data_type);

    if (nFirstExtraChannel < info.num_extra_channels)
    {
        int nExtraChannelsToExtract =
            info.num_extra_channels - nFirstExtraChannel;
        format.num_channels = 1;
        main_buffer_size =
            channel_size * (info.num_color_channels + (bAlphaEmbedded ? 1 : 0));
        extra_channel_buffer =
            _TIFFmallocExt(tif, channel_size * nExtraChannelsToExtract);
        if (extra_channel_buffer == NULL)
            return 0;
        for (int i = 0; i < nExtraChannelsToExtract; ++i)
        {
            size_t buffer_size;
            const int iCorrectedIdx = i + nFirstExtraChannel;

            if (JxlDecoderExtraChannelBufferSize(sp->decoder, &format,
                                                 &buffer_size, iCorrectedIdx) !=
                JXL_DEC_SUCCESS)
            {
                TIFFErrorExtR(tif, module,
                              "JxlDecoderExtraChannelBufferSize failed()");
                _TIFFfreeExt(tif, extra_channel_buffer);
                return 0;
            }
            if (buffer_size != channel_size)
            {
                TIFFErrorExtR(tif, module,
                              "JxlDecoderExtraChannelBufferSize returned %ld, "
                              "expecting %u",
                              buffer_size, channel_size);
                _TIFFfreeExt(tif, extra_channel_buffer);
                return 0;
            }

#if 0
                // Check consistency of JXL codestream header regarding
                // extra alpha channels and TIFF ExtraSamples tag
                JxlExtraChannelInfo extra_channel_info;
                memset(&extra_channel_info, 0, sizeof(extra_channel_info));
                if( JxlDecoderGetExtraChannelInfo(sp->decoder, iCorrectedIdx, &extra_channel_info) == JXL_DEC_SUCCESS )
                {
                    if( extra_channel_info.type == JXL_CHANNEL_ALPHA &&
                        !extra_channel_info.alpha_premultiplied )
                    {
                        if( iCorrectedIdx < td->td_extrasamples &&
                            td->td_sampleinfo[iCorrectedIdx] == EXTRASAMPLE_UNASSALPHA )
                        {
                            // ok
                        }
                        else
                        {
                            TIFFWarningExtR(tif, module,
                                           "Unpremultiplied alpha channel expected from JXL codestream "
                                           "in extra channel %d, but other value found in ExtraSamples tag", iCorrectedIdx);
                        }
                    }
                    else if( extra_channel_info.type == JXL_CHANNEL_ALPHA &&
                             extra_channel_info.alpha_premultiplied )
                    {
                        if( iCorrectedIdx < td->td_extrasamples &&
                            td->td_sampleinfo[iCorrectedIdx] == EXTRASAMPLE_ASSOCALPHA )
                        {
                            // ok
                        }
                        else
                        {
                            TIFFWarningExtR(tif, module,
                                           "Premultiplied alpha channel expected from JXL codestream "
                                           "in extra channel %d, but other value found in ExtraSamples tag", iCorrectedIdx);
                        }
                    }
                    else if( iCorrectedIdx < td->td_extrasamples &&
                             td->td_sampleinfo[iCorrectedIdx] == EXTRASAMPLE_UNASSALPHA )
                    {
                        TIFFWarningExtR(tif, module,
                                       "Unpremultiplied alpha channel expected from ExtraSamples tag "
                                       "in extra channel %d, but other value found in JXL codestream", iCorrectedIdx);
                    }
                    else if( iCorrectedIdx < td->td_extrasamples &&
                             td->td_sampleinfo[iCorrectedIdx] == EXTRASAMPLE_ASSOCALPHA )
                    {
                        TIFFWarningExtR(tif, module,
                                       "Premultiplied alpha channel expected from ExtraSamples tag "
                                       "in extra channel %d, but other value found in JXL codestream", iCorrectedIdx);
                    }
                }
#endif
            if (JxlDecoderSetExtraChannelBuffer(
                    sp->decoder, &format,
                    extra_channel_buffer + i * channel_size, channel_size,
                    i + nFirstExtraChannel) != JXL_DEC_SUCCESS)
            {
                TIFFErrorExtR(tif, module,
                              "JxlDecoderSetExtraChannelBuffer failed()");
                _TIFFfreeExt(tif, extra_channel_buffer);
                return 0;
            }
        }
    }

    format.num_channels = info.num_color_channels;
    if (bAlphaEmbedded)
        format.num_channels++;

    status = JxlDecoderProcessInput(sp->decoder);
    if (status != JXL_DEC_NEED_IMAGE_OUT_BUFFER)
    {
        TIFFErrorExtR(tif, module,
                      "JxlDecoderProcessInput() (second call) failed with %d",
                      status);
        JxlDecoderReleaseInput(sp->decoder);
        _TIFFfreeExt(tif, extra_channel_buffer);
        return 0;
    }

    status = JxlDecoderSetImageOutBuffer(
        sp->decoder, &format, sp->uncompressed_buffer, main_buffer_size);
    if (status != JXL_DEC_SUCCESS)
    {
        TIFFErrorExtR(tif, module,
                      "JxlDecoderSetImageOutBuffer() failed with %d", status);
        JxlDecoderReleaseInput(sp->decoder);
        _TIFFfreeExt(tif, extra_channel_buffer);
        return 0;
    }

    status = JxlDecoderProcessInput(sp->decoder);
    if (status != JXL_DEC_FULL_IMAGE)
    {
        TIFFErrorExtR(tif, module,
                      "JxlDecoderProcessInput() (third call) failed with %d",
                      status);
        JxlDecoderReleaseInput(sp->decoder);
        _TIFFfreeExt(tif, extra_channel_buffer);
        return 0;
    }
    if (nFirstExtraChannel < info.num_extra_channels)
    {
        // first reorder the main buffer
        const int nMainChannels = bAlphaEmbedded ? info.num_color_channels + 1
                                                 : info.num_color_channels;
        const unsigned int mainPixSize = nMainChannels * nBytesPerSample;
        const unsigned int fullPixSize =
            td->td_samplesperpixel * nBytesPerSample;
        assert(fullPixSize > mainPixSize);

        /* Find min value of k such that k * fullPixSize >= (k + 1) * mainPixSize:
         * ==> k = ceil(mainPixSize / (fullPixSize - mainPixSize))
         * ==> k = (mainPixSize + (fullPixSize - mainPixSize) - 1) / (fullPixSize - mainPixSize)
         * ==> k = (fullPixSize - 1) / (fullPixSize - mainPixSize)
         */
        const unsigned int nNumPixels = info.xsize * info.ysize;
        unsigned int outOff = sp->uncompressed_size - fullPixSize;
        unsigned int inOff = main_buffer_size - mainPixSize;
        const unsigned int kThreshold =
            (fullPixSize - 1) / (fullPixSize - mainPixSize);
        if (mainPixSize == 1)
        {
            for (unsigned int k = kThreshold; k < nNumPixels; ++k)
            {
                memcpy(sp->uncompressed_buffer + outOff,
                       sp->uncompressed_buffer + inOff, /*mainPixSize=*/1);
                inOff -= /*mainPixSize=*/1;
                outOff -= fullPixSize;
            }
        }
        else if (mainPixSize == 2)
        {
            for (unsigned int k = kThreshold; k < nNumPixels; ++k)
            {
                memcpy(sp->uncompressed_buffer + outOff,
                       sp->uncompressed_buffer + inOff, /*mainPixSize=*/2);
                inOff -= /*mainPixSize=*/2;
                outOff -= fullPixSize;
            }
        }
        else if (mainPixSize == 3)
        {
            for (unsigned int k = kThreshold; k < nNumPixels; ++k)
            {
                memcpy(sp->uncompressed_buffer + outOff,
                       sp->uncompressed_buffer + inOff, /*mainPixSize=*/3);
                inOff -= /*mainPixSize=*/3;
                outOff -= fullPixSize;
            }
        }
        else if (mainPixSize == 4)
        {
            for (unsigned int k = kThreshold; k < nNumPixels; ++k)
            {
                memcpy(sp->uncompressed_buffer + outOff,
                       sp->uncompressed_buffer + inOff, /*mainPixSize=*/4);
                inOff -= /*mainPixSize=*/4;
                outOff -= fullPixSize;
            }
        }
        else if (mainPixSize == 3 * 2)
        {
            for (unsigned int k = kThreshold; k < nNumPixels; ++k)
            {
                memcpy(sp->uncompressed_buffer + outOff,
                       sp->uncompressed_buffer + inOff, /*mainPixSize=*/3 * 2);
                inOff -= /*mainPixSize=*/3 * 2;
                outOff -= fullPixSize;
            }
        }
        else if (mainPixSize == 4 * 2)
        {
            for (unsigned int k = kThreshold; k < nNumPixels; ++k)
            {
                memcpy(sp->uncompressed_buffer + outOff,
                       sp->uncompressed_buffer + inOff, /*mainPixSize=*/4 * 2);
                inOff -= /*mainPixSize=*/4 * 2;
                outOff -= fullPixSize;
            }
        }
        else
        {
            for (unsigned int k = kThreshold; k < nNumPixels; ++k)
            {
                memcpy(sp->uncompressed_buffer + outOff,
                       sp->uncompressed_buffer + inOff, mainPixSize);
                inOff -= mainPixSize;
                outOff -= fullPixSize;
            }
        }
        /* Last iterations need memmove() because of overlapping between */
        /* source and target regions. */
        for (unsigned int k = kThreshold; k > 1;)
        {
            --k;
            memmove(sp->uncompressed_buffer + outOff,
                    sp->uncompressed_buffer + inOff, mainPixSize);
            inOff -= mainPixSize;
            outOff -= fullPixSize;
        }
        // then copy over the data from the extra_channel_buffer
        const int nExtraChannelsToExtract =
            info.num_extra_channels - nFirstExtraChannel;
        for (int i = 0; i < nExtraChannelsToExtract; ++i)
        {
            outOff = (i + nMainChannels) * nBytesPerSample;
            uint8_t *channel_buffer = extra_channel_buffer + i * channel_size;
            if (nBytesPerSample == 1)
            {
                for (; outOff < sp->uncompressed_size;
                     outOff += fullPixSize,
                     channel_buffer += /*nBytesPerSample=*/1)
                {
                    memcpy(sp->uncompressed_buffer + outOff, channel_buffer,
                           /*nBytesPerSample=*/1);
                }
            }
            else if (nBytesPerSample == 2)
            {
                for (; outOff < sp->uncompressed_size;
                     outOff += fullPixSize,
                     channel_buffer += /*nBytesPerSample=*/2)
                {
                    memcpy(sp->uncompressed_buffer + outOff, channel_buffer,
                           /*nBytesPerSample=*/2);
                }
            }
            else
            {
                assert(nBytesPerSample == 4);
                for (; outOff < sp->uncompressed_size;
                     outOff += fullPixSize, channel_buffer += nBytesPerSample)
                {
                    memcpy(sp->uncompressed_buffer + outOff, channel_buffer,
                           nBytesPerSample);
                }
            }
        }
        _TIFFfreeExt(tif, extra_channel_buffer);
    }

    /*const size_t nRemaining = */ JxlDecoderReleaseInput(sp->decoder);
    /*if( nRemaining != 0 )
    {
        TIFFErrorExtR(tif, module,
                     "JxlDecoderReleaseInput(): %u input bytes remaining",
                     (unsigned)nRemaining);
    }*/

    return 1;
}

/*
 * Decode a strip, tile or scanline.
 */
static int JXLDecode(TIFF *tif, uint8_t *op, tmsize_t occ, uint16_t s)
{
    static const char module[] = "JXLDecode";
    JXLState *sp = DecoderState(tif);

    (void)s;
    assert(sp != NULL);
    assert(sp->state == LSTATE_INIT_DECODE);

    if (sp->uncompressed_buffer == 0)
    {
        TIFFErrorExtR(tif, module, "Uncompressed buffer not allocated");
        return 0;
    }

    if ((uint64_t)sp->uncompressed_offset + (uint64_t)occ >
        sp->uncompressed_size)
    {
        TIFFErrorExtR(tif, module, "Too many bytes read");
        return 0;
    }

    memcpy(op, sp->uncompressed_buffer + sp->uncompressed_offset, occ);
    sp->uncompressed_offset += (unsigned)occ;

    return 1;
}

static int JXLSetupEncode(TIFF *tif)
{
    JXLState *sp = EncoderState(tif);

    assert(sp != NULL);
    if (sp->state & LSTATE_INIT_DECODE)
    {
        sp->state = 0;
    }

    if (GetJXLDataType(tif) < 0)
        return 0;

    sp->state |= LSTATE_INIT_ENCODE;

    return 1;
}

/*
 * Reset encoding state at the start of a strip.
 */
static int JXLPreEncode(TIFF *tif, uint16_t s)
{
    static const char module[] = "JXLPreEncode";
    JXLState *sp = EncoderState(tif);

    (void)s;
    assert(sp != NULL);
    if (sp->state != LSTATE_INIT_ENCODE)
        tif->tif_setupencode(tif);

    if (!SetupUncompressedBuffer(tif, sp, module))
        return 0;

    return 1;
}

/*
 * Encode a chunk of pixels.
 */
static int JXLEncode(TIFF *tif, uint8_t *bp, tmsize_t cc, uint16_t s)
{
    static const char module[] = "JXLEncode";
    JXLState *sp = EncoderState(tif);

    (void)s;
    assert(sp != NULL);
    assert(sp->state == LSTATE_INIT_ENCODE);

    if ((uint64_t)sp->uncompressed_offset + (uint64_t)cc >
        sp->uncompressed_size)
    {
        TIFFErrorExtR(tif, module, "Too many bytes written");
        return 0;
    }

    memcpy(sp->uncompressed_buffer + sp->uncompressed_offset, bp, cc);
    sp->uncompressed_offset += (unsigned)cc;

    return 1;
}

/*
 * Finish off an encoded strip by flushing it.
 */
static int JXLPostEncode(TIFF *tif)
{
    static const char module[] = "JXLPostEncode";
    JXLState *sp = EncoderState(tif);
    TIFFDirectory *td = &tif->tif_dir;

    if (sp->uncompressed_offset != sp->uncompressed_size)
    {
        TIFFErrorExtR(tif, module, "Unexpected number of bytes in the buffer");
        return 0;
    }

    JxlEncoder *enc = JxlEncoderCreate(NULL);
    if (enc == NULL)
    {
        TIFFErrorExtR(tif, module, "JxlEncoderCreate() failed");
        return 0;
    }
    JxlEncoderUseContainer(enc, JXL_FALSE);

#ifdef HAVE_JxlEncoderFrameSettingsCreate
    JxlEncoderFrameSettings *opts = JxlEncoderFrameSettingsCreate(enc, NULL);
#else
    JxlEncoderOptions *opts = JxlEncoderOptionsCreate(enc, NULL);
#endif
    if (opts == NULL)
    {
        TIFFErrorExtR(tif, module, "JxlEncoderFrameSettingsCreate() failed");
        JxlEncoderDestroy(enc);
        return 0;
    }

    JxlPixelFormat format = {0};
    format.data_type = GetJXLDataType(tif);
    format.endianness = JXL_NATIVE_ENDIAN;
    format.align = 0;

#ifdef HAVE_JxlEncoderSetCodestreamLevel
    if (td->td_bitspersample > 12)
    {
        JxlEncoderSetCodestreamLevel(enc, 10);
    }
#endif
    JxlBasicInfo basic_info = {0};
    JxlEncoderInitBasicInfo(&basic_info);
    basic_info.xsize = sp->segment_width;
    basic_info.ysize = sp->segment_height;
    basic_info.bits_per_sample = td->td_bitspersample;
    basic_info.orientation = JXL_ORIENT_IDENTITY;
    if (td->td_sampleformat == SAMPLEFORMAT_IEEEFP)
    {
        basic_info.exponent_bits_per_sample = 8;
    }
    else
    {
        basic_info.exponent_bits_per_sample = 0;
    }

    int bAlphaEmbedded = 0;
    const int bAlphaDistanceSameAsMainChannel =
        (sp->alpha_distance < 0.0f) ||
        ((sp->lossless && sp->alpha_distance == 0.0f) ||
         (!sp->lossless && sp->alpha_distance == sp->distance));
#ifndef HAVE_JxlEncoderSetExtraChannelDistance
    if (!bAlphaDistanceSameAsMainChannel)
    {
        TIFFWarningExtR(tif, module,
                        "AlphaDistance ignored due to "
                        "JxlEncoderSetExtraChannelDistance() not being "
                        "available. Please upgrade libjxl to > 0.8.1");
    }
#endif

    if (td->td_planarconfig == PLANARCONFIG_SEPARATE)
    {
        format.num_channels = 1;
        basic_info.num_color_channels = 1;
        basic_info.num_extra_channels = 0;
        basic_info.alpha_bits = 0;
        basic_info.alpha_exponent_bits = 0;
    }
    else
    {
        if (td->td_photometric == PHOTOMETRIC_MINISBLACK &&
            td->td_extrasamples > 0 &&
            td->td_extrasamples == td->td_samplesperpixel - 1 &&
            td->td_sampleinfo[0] == EXTRASAMPLE_UNASSALPHA &&
            bAlphaDistanceSameAsMainChannel)
        {  // gray with alpha
            format.num_channels = 2;
            basic_info.num_color_channels = 1;
            basic_info.num_extra_channels = td->td_extrasamples;
            basic_info.alpha_bits = td->td_bitspersample;
            basic_info.alpha_exponent_bits =
                basic_info.exponent_bits_per_sample;
            bAlphaEmbedded = 1;
        }
        else if (td->td_photometric == PHOTOMETRIC_RGB &&
                 td->td_extrasamples > 0 &&
                 td->td_extrasamples == td->td_samplesperpixel - 3 &&
                 td->td_sampleinfo[0] == EXTRASAMPLE_UNASSALPHA &&
                 bAlphaDistanceSameAsMainChannel)
        {  // rgb with alpha, and same distance for alpha vs non-alpha channels
            format.num_channels = 4;
            basic_info.num_color_channels = 3;
            basic_info.num_extra_channels = td->td_samplesperpixel - 3;
            basic_info.alpha_bits = td->td_bitspersample;
            basic_info.alpha_exponent_bits =
                basic_info.exponent_bits_per_sample;
            bAlphaEmbedded = 1;
        }
        else if (td->td_photometric == PHOTOMETRIC_RGB &&
                 ((td->td_extrasamples == 0) ||
                  (td->td_extrasamples > 0 &&
                   td->td_extrasamples == td->td_samplesperpixel - 3 &&
                   (td->td_sampleinfo[0] != EXTRASAMPLE_UNASSALPHA ||
                    !bAlphaDistanceSameAsMainChannel))))
        {  // rgb without alpha, or differente distance for alpha vs non-alpha
            // channels
            format.num_channels = 3;
            basic_info.num_color_channels = 3;
            basic_info.num_extra_channels = td->td_samplesperpixel - 3;
            basic_info.alpha_bits = 0;
            basic_info.alpha_exponent_bits = 0;
        }
        else
        {  // fallback to gray without alpha and with eventual extra channels
            format.num_channels = 1;
            basic_info.num_color_channels = 1;
            basic_info.num_extra_channels = td->td_samplesperpixel - 1;
            basic_info.alpha_bits = 0;
            basic_info.alpha_exponent_bits = 0;
        }
#ifndef HAVE_JxlExtraChannels
        if (basic_info.num_extra_channels > 1 ||
            (basic_info.num_extra_channels == 1 && !bAlphaEmbedded))
        {
            TIFFErrorExtR(
                tif, module,
                "JXL: INTERLEAVE=PIXEL does not support this combination of "
                "bands. Please upgrade libjxl to 0.8+");
            return 0;
        }
#endif
    }

    if (sp->lossless)
    {
#ifdef HAVE_JxlEncoderSetFrameLossless
        JxlEncoderSetFrameLossless(opts, TRUE);
#else
        JxlEncoderOptionsSetLossless(opts, TRUE);
#endif
#ifdef HAVE_JxlEncoderSetFrameDistance
        JxlEncoderSetFrameDistance(opts, 0);
#else
        JxlEncoderOptionsSetDistance(opts, 0);
#endif
        basic_info.uses_original_profile = JXL_TRUE;
    }
    else
    {
#ifdef HAVE_JxlEncoderSetFrameDistance
        if (JxlEncoderSetFrameDistance(opts, sp->distance) != JXL_ENC_SUCCESS)
#else
        if (JxlEncoderOptionsSetDistance(opts, sp->distance) != JXL_ENC_SUCCESS)
#endif
        {
            TIFFErrorExtR(tif, module, "JxlEncoderSetFrameDistance() failed");
            JxlEncoderDestroy(enc);
            return 0;
        }
    }
#ifdef HAVE_JxlEncoderFrameSettingsSetOption
    if (JxlEncoderFrameSettingsSetOption(opts, JXL_ENC_FRAME_SETTING_EFFORT,
                                         sp->effort) != JXL_ENC_SUCCESS)
#else
    if (JxlEncoderOptionsSetEffort(opts, sp->effort) != JXL_ENC_SUCCESS)
#endif
    {
        TIFFErrorExtR(tif, module, "JxlEncoderFrameSettingsSetOption() failed");
        JxlEncoderDestroy(enc);
        return 0;
    }

    if (JXL_ENC_SUCCESS != JxlEncoderSetBasicInfo(enc, &basic_info))
    {
        TIFFErrorExtR(tif, module, "JxlEncoderSetBasicInfo() failed");
        JxlEncoderDestroy(enc);
        return 0;
    }

    JxlColorEncoding color_encoding = {0};
    JxlColorEncodingSetToSRGB(&color_encoding, /*is_gray*/
                              (td->td_planarconfig == PLANARCONFIG_SEPARATE ||
                               basic_info.num_color_channels == 1));
    if (JXL_ENC_SUCCESS != JxlEncoderSetColorEncoding(enc, &color_encoding))
    {
        TIFFErrorExtR(tif, module, "JxlEncoderSetColorEncoding() failed");
        JxlEncoderDestroy(enc);
        return 0;
    }

    uint8_t *main_buffer = sp->uncompressed_buffer;
    unsigned int main_size = sp->uncompressed_size;

#ifdef HAVE_JxlExtraChannels
    int nBytesPerSample = GetJXLDataTypeSize(format.data_type);
    if (td->td_planarconfig == PLANARCONFIG_CONTIG &&
        (basic_info.num_extra_channels > 1 ||
         (basic_info.num_extra_channels == 1 && !bAlphaEmbedded)))
    {
        main_size = (sp->uncompressed_size / td->td_samplesperpixel);
        int nMainChannels = basic_info.num_color_channels;
        if (bAlphaEmbedded)
            nMainChannels++;
        main_size *= nMainChannels;
        main_buffer = _TIFFmallocExt(tif, main_size);
        if (main_buffer == NULL)
            return 0;
        int outChunkSize = nBytesPerSample * nMainChannels;
        int inStep = nBytesPerSample * td->td_samplesperpixel;
        uint8_t *cur_outbuffer = main_buffer;
        uint8_t *cur_inbuffer = sp->uncompressed_buffer;
        for (; cur_outbuffer - main_buffer < main_size;
             cur_outbuffer += outChunkSize, cur_inbuffer += inStep)
        {
            memcpy(cur_outbuffer, cur_inbuffer, outChunkSize);
        }
        for (int iChannel = nMainChannels; iChannel < td->td_samplesperpixel;
             iChannel++)
        {
            JxlExtraChannelInfo extra_channel_info;
            int channelType = JXL_CHANNEL_OPTIONAL;
            const int iExtraChannel = iChannel - nMainChannels + bAlphaEmbedded;
            if (iExtraChannel < td->td_extrasamples &&
                (td->td_sampleinfo[iExtraChannel] == EXTRASAMPLE_UNASSALPHA ||
                 td->td_sampleinfo[iExtraChannel] == EXTRASAMPLE_ASSOCALPHA))
            {
                channelType = JXL_CHANNEL_ALPHA;
            }
            JxlEncoderInitExtraChannelInfo(channelType, &extra_channel_info);
            extra_channel_info.bits_per_sample = basic_info.bits_per_sample;
            extra_channel_info.exponent_bits_per_sample =
                basic_info.exponent_bits_per_sample;
            if (iExtraChannel < td->td_extrasamples &&
                td->td_sampleinfo[iExtraChannel] == EXTRASAMPLE_ASSOCALPHA)
            {
                extra_channel_info.alpha_premultiplied = JXL_TRUE;
            }

            if (JXL_ENC_SUCCESS != JxlEncoderSetExtraChannelInfo(
                                       enc, iExtraChannel, &extra_channel_info))
            {
                TIFFErrorExtR(tif, module,
                              "JxlEncoderSetExtraChannelInfo(%d) failed",
                              iChannel);
                JxlEncoderDestroy(enc);
                _TIFFfreeExt(tif, main_buffer);
                return 0;
            }
#if HAVE_JxlEncoderSetExtraChannelDistance
            if (channelType == JXL_CHANNEL_ALPHA && sp->alpha_distance >= 0.0f)
            {
                if (JXL_ENC_SUCCESS !=
                    JxlEncoderSetExtraChannelDistance(opts, iExtraChannel,
                                                      sp->alpha_distance))
                {
                    TIFFErrorExtR(
                        tif, module,
                        "JxlEncoderSetExtraChannelDistance(%d) failed",
                        iChannel);
                    JxlEncoderDestroy(enc);
                    _TIFFfreeExt(tif, main_buffer);
                    return 0;
                }
            }
#endif
        }
    }
#endif

    int retCode =
        JxlEncoderAddImageFrame(opts, &format, main_buffer, main_size);
    // cleanup now
    if (main_buffer != sp->uncompressed_buffer)
    {
        _TIFFfreeExt(tif, main_buffer);
    }
    if (retCode != JXL_ENC_SUCCESS)
    {
        TIFFErrorExtR(tif, module, "JxlEncoderAddImageFrame() failed");
        JxlEncoderDestroy(enc);
        return 0;
    }

#ifdef HAVE_JxlExtraChannels
    if (td->td_planarconfig == PLANARCONFIG_CONTIG &&
        (basic_info.num_extra_channels > 1 ||
         (basic_info.num_extra_channels == 1 && !bAlphaEmbedded)))
    {
        int nMainChannels = basic_info.num_color_channels;
        if (bAlphaEmbedded)
            nMainChannels++;
        int extra_channel_size =
            (sp->uncompressed_size / td->td_samplesperpixel);
        uint8_t *extra_channel_buffer = _TIFFmallocExt(tif, extra_channel_size);
        if (extra_channel_buffer == NULL)
            return 0;
        int inStep = nBytesPerSample * td->td_samplesperpixel;
        int outStep = nBytesPerSample;
        for (int iChannel = nMainChannels; iChannel < td->td_samplesperpixel;
             iChannel++)
        {
            uint8_t *cur_outbuffer = extra_channel_buffer;
            uint8_t *cur_inbuffer =
                sp->uncompressed_buffer + iChannel * outStep;
            for (; cur_outbuffer - extra_channel_buffer < extra_channel_size;
                 cur_outbuffer += outStep, cur_inbuffer += inStep)
            {
                memcpy(cur_outbuffer, cur_inbuffer, outStep);
            }
            if (JxlEncoderSetExtraChannelBuffer(
                    opts, &format, extra_channel_buffer, extra_channel_size,
                    (bAlphaEmbedded)
                        ? iChannel - nMainChannels + 1
                        : iChannel - nMainChannels) != JXL_ENC_SUCCESS)
            {
                TIFFErrorExtR(tif, module,
                              "JxlEncoderSetExtraChannelBuffer() failed");
                _TIFFfreeExt(tif, extra_channel_buffer);
                JxlEncoderDestroy(enc);
                return 0;
            }
        }
        _TIFFfreeExt(tif, extra_channel_buffer);
    }
#endif

    JxlEncoderCloseInput(enc);

    while (TRUE)
    {
        size_t len = (size_t)tif->tif_rawdatasize;
        uint8_t *buf = (uint8_t *)tif->tif_rawdata;
        JxlEncoderStatus process_result =
            JxlEncoderProcessOutput(enc, &buf, &len);
        if (process_result == JXL_ENC_ERROR)
        {
            TIFFErrorExtR(tif, module, "JxlEncoderProcessOutput() failed");
            JxlEncoderDestroy(enc);
            return 0;
        }
        tif->tif_rawcc = tif->tif_rawdatasize - len;
        if (!TIFFFlushData1(tif))
        {
            JxlEncoderDestroy(enc);
            return 0;
        }
        if (process_result != JXL_ENC_NEED_MORE_OUTPUT)
            break;
    }

    JxlEncoderDestroy(enc);
    return 1;
}

static void JXLCleanup(TIFF *tif)
{
    JXLState *sp = LState(tif);

    assert(sp != 0);

    tif->tif_tagmethods.vgetfield = sp->vgetparent;
    tif->tif_tagmethods.vsetfield = sp->vsetparent;

    _TIFFfreeExt(tif, sp->uncompressed_buffer);

    if (sp->decoder)
        JxlDecoderDestroy(sp->decoder);

    _TIFFfreeExt(tif, sp);
    tif->tif_data = NULL;

    _TIFFSetDefaultCompressionState(tif);
}

static const TIFFField JXLFields[] = {
    {TIFFTAG_JXL_LOSSYNESS, 0, 0, TIFF_ANY, 0, TIFF_SETGET_UINT32,
     TIFF_SETGET_UNDEFINED, FIELD_PSEUDO, FALSE, FALSE, "Lossyness", NULL},
    {TIFFTAG_JXL_EFFORT, 0, 0, TIFF_ANY, 0, TIFF_SETGET_UINT32,
     TIFF_SETGET_UNDEFINED, FIELD_PSEUDO, FALSE, FALSE, "Effort", NULL},
    {TIFFTAG_JXL_DISTANCE, 0, 0, TIFF_ANY, 0, TIFF_SETGET_FLOAT,
     TIFF_SETGET_UNDEFINED, FIELD_PSEUDO, FALSE, FALSE, "Distance", NULL},
    {TIFFTAG_JXL_ALPHA_DISTANCE, 0, 0, TIFF_ANY, 0, TIFF_SETGET_FLOAT,
     TIFF_SETGET_UNDEFINED, FIELD_PSEUDO, FALSE, FALSE, "AlphaDistance", NULL},
};

static int JXLVSetField(TIFF *tif, uint32_t tag, va_list ap)
{
    static const char module[] = "JXLVSetField";
    JXLState *sp = LState(tif);

    switch (tag)
    {
        case TIFFTAG_JXL_LOSSYNESS:
        {
            uint32_t lossyness = va_arg(ap, uint32_t);
            if (lossyness == JXL_LOSSLESS)
                sp->lossless = TRUE;
            else if (lossyness == JXL_LOSSY)
                sp->lossless = FALSE;
            else
            {
                TIFFErrorExtR(tif, module, "Invalid value for Lossyness: %u",
                              lossyness);
                return 0;
            }
            return 1;
        }

        case TIFFTAG_JXL_EFFORT:
        {
            uint32_t effort = va_arg(ap, uint32_t);
            if (effort < 1 || effort > 9)
            {
                TIFFErrorExtR(tif, module, "Invalid value for Effort: %u",
                              effort);
                return 0;
            }
            sp->effort = effort;
            return 1;
        }

        case TIFFTAG_JXL_DISTANCE:
        {
            float distance = (float)va_arg(ap, double);
            if (distance < 0 || distance > 15)
            {
                TIFFErrorExtR(tif, module, "Invalid value for Distance: %f",
                              distance);
                return 0;
            }
            sp->distance = distance;
            return 1;
        }

        case TIFFTAG_JXL_ALPHA_DISTANCE:
        {
            float alpha_distance = (float)va_arg(ap, double);
            if (alpha_distance != -1 &&
                (alpha_distance < 0 || alpha_distance > 15))
            {
                TIFFErrorExtR(tif, module,
                              "Invalid value for AlphaDistance: %f",
                              alpha_distance);
                return 0;
            }
            sp->alpha_distance = alpha_distance;
            return 1;
        }

        default:
        {
            return (*sp->vsetparent)(tif, tag, ap);
        }
    }
    /*NOTREACHED*/
}

static int JXLVGetField(TIFF *tif, uint32_t tag, va_list ap)
{
    JXLState *sp = LState(tif);

    switch (tag)
    {
        case TIFFTAG_JXL_LOSSYNESS:
            *va_arg(ap, uint32_t *) = sp->lossless ? JXL_LOSSLESS : JXL_LOSSY;
            break;
        case TIFFTAG_JXL_EFFORT:
            *va_arg(ap, uint32_t *) = sp->effort;
            break;
        case TIFFTAG_JXL_DISTANCE:
            *va_arg(ap, float *) = sp->distance;
            break;
        case TIFFTAG_JXL_ALPHA_DISTANCE:
            *va_arg(ap, float *) = sp->alpha_distance;
            break;
        default:
            return (*sp->vgetparent)(tif, tag, ap);
    }
    return 1;
}

int TIFFInitJXL(TIFF *tif, int scheme)
{
    static const char module[] = "TIFFInitJXL";
    JXLState *sp;

    (void)scheme;
    assert(scheme == COMPRESSION_JXL);

    /*
     * Merge codec-specific tag information.
     */
    if (!_TIFFMergeFields(tif, JXLFields, TIFFArrayCount(JXLFields)))
    {
        TIFFErrorExtR(tif, module, "Merging JXL codec-specific tags failed");
        return 0;
    }

    /*
     * Allocate state block so tag methods have storage to record values.
     */
    tif->tif_data = (uint8_t *)_TIFFcallocExt(tif, 1, sizeof(JXLState));
    if (tif->tif_data == NULL)
        goto bad;
    sp = LState(tif);

    /*
     * Override parent get/set field methods.
     */
    sp->vgetparent = tif->tif_tagmethods.vgetfield;
    tif->tif_tagmethods.vgetfield = JXLVGetField; /* hook for codec tags */
    sp->vsetparent = tif->tif_tagmethods.vsetfield;
    tif->tif_tagmethods.vsetfield = JXLVSetField; /* hook for codec tags */

    /*
     * Install codec methods.
     */
    tif->tif_fixuptags = JXLFixupTags;
    tif->tif_setupdecode = JXLSetupDecode;
    tif->tif_predecode = JXLPreDecode;
    tif->tif_decoderow = JXLDecode;
    tif->tif_decodestrip = JXLDecode;
    tif->tif_decodetile = JXLDecode;
    tif->tif_setupencode = JXLSetupEncode;
    tif->tif_preencode = JXLPreEncode;
    tif->tif_postencode = JXLPostEncode;
    tif->tif_encoderow = JXLEncode;
    tif->tif_encodestrip = JXLEncode;
    tif->tif_encodetile = JXLEncode;
    tif->tif_cleanup = JXLCleanup;

    /* Default values for codec-specific fields */
    sp->decoder = NULL;

    sp->state = 0;
    sp->lossless = TRUE;
    sp->effort = 5;
    sp->distance = 1.0;
    sp->alpha_distance = -1.0;

    return 1;
bad:
    TIFFErrorExtR(tif, module, "No space for JXL state block");
    return 0;
}

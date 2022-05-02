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
typedef struct {
        int             state;                  /* state flags */

        int             lossless;               /* TRUE (default) or FALSE */
        int             effort;                 /* 3 to 9. default: 7 */
        float           distance;               /* 0 to 15. default: 1.0 */

        uint32_t        segment_width;
        uint32_t        segment_height;

        unsigned int    uncompressed_size;
        unsigned int    uncompressed_alloc;
        uint8_t         *uncompressed_buffer;
        unsigned int    uncompressed_offset;

        JxlDecoder     *decoder;

        TIFFVGetMethod  vgetparent;            /* super-class method */
        TIFFVSetMethod  vsetparent;            /* super-class method */
} JXLState;

#define LState(tif)             ((JXLState*) (tif)->tif_data)
#define DecoderState(tif)       LState(tif)
#define EncoderState(tif)       LState(tif)

static int JXLEncode(TIFF* tif, uint8_t* bp, tmsize_t cc, uint16_t s);
static int JXLDecode(TIFF* tif, uint8_t* op, tmsize_t occ, uint16_t s);

static int GetJXLDataType(TIFF* tif)
{
    TIFFDirectory *td = &tif->tif_dir;
    static const char module[] = "GetJXLDataType";

    if( td->td_sampleformat == SAMPLEFORMAT_UINT &&
            td->td_bitspersample == 8 )
    {
        return JXL_TYPE_UINT8;
    }

    if( td->td_sampleformat == SAMPLEFORMAT_UINT &&
            td->td_bitspersample == 16 )
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

    if( td->td_sampleformat == SAMPLEFORMAT_IEEEFP &&
            td->td_bitspersample == 32 )
    {
        return JXL_TYPE_FLOAT;
    }

    TIFFErrorExt(tif->tif_clientdata, module,
        "Unsupported combination of SampleFormat and BitsPerSample");
    return -1;
}

static int
JXLFixupTags(TIFF* tif)
{
        (void) tif;
        return 1;
}

static int
JXLSetupDecode(TIFF* tif)
{
        JXLState* sp = DecoderState(tif);

        assert(sp != NULL);

        /* if we were last encoding, terminate this mode */
        if (sp->state & LSTATE_INIT_ENCODE) {
            sp->state = 0;
        }

        sp->state |= LSTATE_INIT_DECODE;
        return 1;
}

static int SetupUncompressedBuffer(TIFF* tif, JXLState* sp,
                                   const char* module)
{
    TIFFDirectory *td = &tif->tif_dir;
    uint64_t new_size_64;
    uint64_t new_alloc_64;
    unsigned int new_size;
    unsigned int new_alloc;

    sp->uncompressed_offset = 0;

    if (isTiled(tif)) {
            sp->segment_width = td->td_tilewidth;
            sp->segment_height = td->td_tilelength;
    } else {
            sp->segment_width = td->td_imagewidth;
            sp->segment_height = td->td_imagelength - tif->tif_row;
            if (sp->segment_height > td->td_rowsperstrip)
                sp->segment_height = td->td_rowsperstrip;
    }

    new_size_64 = (uint64_t)sp->segment_width * sp->segment_height *
                                        (td->td_bitspersample / 8);
    if( td->td_planarconfig == PLANARCONFIG_CONTIG )
    {
        new_size_64 *= td->td_samplesperpixel;
    }

    new_size = (unsigned int)new_size_64;
    sp->uncompressed_size = new_size;

    /* add some margin */
    new_alloc_64 = 100 + new_size_64 + new_size_64 / 3;
    new_alloc = (unsigned int)new_alloc_64;
    if( new_alloc != new_alloc_64 )
    {
        TIFFErrorExt(tif->tif_clientdata, module,
                        "Too large uncompressed strip/tile");
        _TIFFfree(sp->uncompressed_buffer);
        sp->uncompressed_buffer = 0;
        sp->uncompressed_alloc = 0;
        return 0;
    }

    if( sp->uncompressed_alloc < new_alloc )
    {
        _TIFFfree(sp->uncompressed_buffer);
        sp->uncompressed_buffer = _TIFFmalloc(new_alloc);
        if( !sp->uncompressed_buffer )
        {
            TIFFErrorExt(tif->tif_clientdata, module,
                            "Cannot allocate buffer");
            _TIFFfree(sp->uncompressed_buffer);
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
static int
JXLPreDecode(TIFF* tif, uint16_t s)
{
        static const char module[] = "JXLPreDecode";
        JXLState* sp = DecoderState(tif);
        TIFFDirectory *td = &tif->tif_dir;

        (void) s;
        assert(sp != NULL);
        if( sp->state != LSTATE_INIT_DECODE )
            tif->tif_setupdecode(tif);

        const int jxlDataType = GetJXLDataType(tif);
        if( jxlDataType < 0 )
            return 0;

        if( !SetupUncompressedBuffer(tif, sp, module) )
            return 0;

        if( sp->decoder == NULL )
        {
            sp->decoder = JxlDecoderCreate(NULL);
            if( sp->decoder == NULL )
            {
                TIFFErrorExt(tif->tif_clientdata, module,
                            "JxlDecoderCreate() failed");
                return 0;
            }
        }
        else
        {
            JxlDecoderReset(sp->decoder);
        }

        JxlDecoderStatus status;
        status = JxlDecoderSubscribeEvents(
                            sp->decoder, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE);
        if( status != JXL_DEC_SUCCESS )
        {
            TIFFErrorExt(tif->tif_clientdata, module,
                         "JxlDecoderSubscribeEvents() failed");
            return 0;
        }

        status = JxlDecoderSetInput(sp->decoder,
                                    (const uint8_t*)tif->tif_rawcp,
                                    (size_t)tif->tif_rawcc);
        if( status != JXL_DEC_SUCCESS )
        {
            TIFFErrorExt(tif->tif_clientdata, module,
                         "JxlDecoderSetInput() failed with %d", status);
            return 0;
        }

        status = JxlDecoderProcessInput(sp->decoder);
        if( status != JXL_DEC_BASIC_INFO )
        {
            TIFFErrorExt(tif->tif_clientdata, module,
                         "JxlDecoderProcessInput() failed with %d", status);
            JxlDecoderReleaseInput(sp->decoder);
            return 0;
        }

        JxlBasicInfo info;
        status = JxlDecoderGetBasicInfo(sp->decoder, &info);
        if( status != JXL_DEC_SUCCESS )
        {
            TIFFErrorExt(tif->tif_clientdata, module,
                         "JxlDecoderGetBasicInfo() failed with %d", status);
            JxlDecoderReleaseInput(sp->decoder);
            return 0;
        }

        if( sp->segment_width != info.xsize )
        {
            TIFFErrorExt(tif->tif_clientdata, module,
                         "JXL basic info xsize = %d, whereas %u was expected",
                         info.xsize, sp->segment_width);
            JxlDecoderReleaseInput(sp->decoder);
            return 0;
        }

        if( sp->segment_height != info.ysize )
        {
            TIFFErrorExt(tif->tif_clientdata, module,
                         "JXL basic info ysize = %d, whereas %u was expected",
                         info.ysize, sp->segment_height);
            JxlDecoderReleaseInput(sp->decoder);
            return 0;
        }

        if( td->td_bitspersample != info.bits_per_sample )
        {
            TIFFErrorExt(tif->tif_clientdata, module,
                         "JXL basic info bits_per_sample = %d, whereas %d was expected",
                         info.bits_per_sample, td->td_bitspersample);
            JxlDecoderReleaseInput(sp->decoder);
            return 0;
        }

        JxlPixelFormat format = {0};
        format.num_channels = td->td_planarconfig == PLANARCONFIG_CONTIG ?
                                                    td->td_samplesperpixel : 1;
        format.data_type = jxlDataType;
        format.endianness = JXL_NATIVE_ENDIAN;
        format.align = 0;

        status = JxlDecoderProcessInput(sp->decoder);
        if( status != JXL_DEC_NEED_IMAGE_OUT_BUFFER )
        {
            TIFFErrorExt(tif->tif_clientdata, module,
                         "JxlDecoderProcessInput() (second call) failed with %d", status);
            JxlDecoderReleaseInput(sp->decoder);
            return 0;
        }

        status = JxlDecoderSetImageOutBuffer(sp->decoder, &format,
                                             sp->uncompressed_buffer, sp->uncompressed_size);
        if( status != JXL_DEC_SUCCESS )
        {
            TIFFErrorExt(tif->tif_clientdata, module,
                         "JxlDecoderSetImageOutBuffer() failed with %d", status);
            JxlDecoderReleaseInput(sp->decoder);
            return 0;
        }

        status = JxlDecoderProcessInput(sp->decoder);
        if( status != JXL_DEC_FULL_IMAGE )
        {
            TIFFErrorExt(tif->tif_clientdata, module,
                         "JxlDecoderProcessInput() (third call) failed with %d", status);
            JxlDecoderReleaseInput(sp->decoder);
            return 0;
        }

        /*const size_t nRemaining = */ JxlDecoderReleaseInput(sp->decoder);
        /*if( nRemaining != 0 )
        {
            TIFFErrorExt(tif->tif_clientdata, module,
                         "JxlDecoderReleaseInput(): %u input bytes remaining",
                         (unsigned)nRemaining);
        }*/

        return 1;
}

/*
* Decode a strip, tile or scanline.
*/
static int
JXLDecode(TIFF* tif, uint8_t* op, tmsize_t occ, uint16_t s)
{
        static const char module[] = "JXLDecode";
        JXLState* sp = DecoderState(tif);

        (void) s;
        assert(sp != NULL);
        assert(sp->state == LSTATE_INIT_DECODE);

        if( sp->uncompressed_buffer == 0 )
        {
            TIFFErrorExt(tif->tif_clientdata, module,
                         "Uncompressed buffer not allocated");
            return 0;
        }

        if( (uint64_t)sp->uncompressed_offset +
                                        (uint64_t)occ > sp->uncompressed_size )
        {
            TIFFErrorExt(tif->tif_clientdata, module,
                         "Too many bytes read");
            return 0;
        }

        memcpy(op,
               sp->uncompressed_buffer + sp->uncompressed_offset,
               occ);
        sp->uncompressed_offset += (unsigned)occ;

        return 1;
}

static int
JXLSetupEncode(TIFF* tif)
{
        JXLState* sp = EncoderState(tif);

        assert(sp != NULL);
        if (sp->state & LSTATE_INIT_DECODE) {
            sp->state = 0;
        }

        sp->state |= LSTATE_INIT_ENCODE;

        return 1;
}

/*
* Reset encoding state at the start of a strip.
*/
static int
JXLPreEncode(TIFF* tif, uint16_t s)
{
        static const char module[] = "JXLPreEncode";
        JXLState *sp = EncoderState(tif);
        TIFFDirectory *td = &tif->tif_dir;

        (void) s;
        assert(sp != NULL);
        if( sp->state != LSTATE_INIT_ENCODE )
            tif->tif_setupencode(tif);

        if( td->td_planarconfig == PLANARCONFIG_CONTIG &&
            td->td_samplesperpixel > 4 )
        {
            TIFFErrorExt(tif->tif_clientdata, module,
                "JXL: INTERLEAVE=PIXEL supports a maximum of 4 bands (%d provided)", td->td_samplesperpixel);
            return 0;
        }

        if( GetJXLDataType(tif) < 0 )
            return 0;

        if( !SetupUncompressedBuffer(tif, sp, module) )
            return 0;

        return 1;
}

/*
* Encode a chunk of pixels.
*/
static int
JXLEncode(TIFF* tif, uint8_t* bp, tmsize_t cc, uint16_t s)
{
        static const char module[] = "JXLEncode";
        JXLState *sp = EncoderState(tif);

        (void)s;
        assert(sp != NULL);
        assert(sp->state == LSTATE_INIT_ENCODE);

        if( (uint64_t)sp->uncompressed_offset +
                                    (uint64_t)cc > sp->uncompressed_size )
        {
            TIFFErrorExt(tif->tif_clientdata, module,
                         "Too many bytes written");
            return 0;
        }

        memcpy(sp->uncompressed_buffer + sp->uncompressed_offset,
               bp, cc);
        sp->uncompressed_offset += (unsigned)cc;

        return 1;
}

/*
* Finish off an encoded strip by flushing it.
*/
static int
JXLPostEncode(TIFF* tif)
{
        static const char module[] = "JXLPostEncode";
        JXLState *sp = EncoderState(tif);
        TIFFDirectory *td = &tif->tif_dir;

        if( sp->uncompressed_offset != sp->uncompressed_size )
        {
            TIFFErrorExt(tif->tif_clientdata, module,
                         "Unexpected number of bytes in the buffer");
            return 0;
        }

        JxlEncoder *enc = JxlEncoderCreate(NULL);
        if( enc == NULL )
        {
            TIFFErrorExt(tif->tif_clientdata, module,
                         "JxlEncoderCreate() failed");
            return 0;
        }
        JxlEncoderUseContainer(enc, JXL_FALSE);

#ifdef HAVE_JxlEncoderFrameSettingsCreate
        JxlEncoderOptions *opts = JxlEncoderFrameSettingsCreate(enc, NULL);
#else
        JxlEncoderOptions *opts = JxlEncoderOptionsCreate(enc, NULL);
#endif
        if( opts == NULL )
        {
            TIFFErrorExt(tif->tif_clientdata, module,
                         "JxlEncoderFrameSettingsCreate() failed");
            JxlEncoderDestroy(enc);
            return 0;
        }

        JxlPixelFormat format = {0};
        format.data_type = GetJXLDataType(tif);
        format.endianness = JXL_NATIVE_ENDIAN;
        format.align = 0;

#ifdef HAVE_JxlEncoderSetCodestreamLevel
        if( sp->lossless && td->td_bitspersample > 12 )
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
        if ( td->td_sampleformat == SAMPLEFORMAT_IEEEFP ) {
            basic_info.exponent_bits_per_sample=8;
        } else {
            basic_info.exponent_bits_per_sample=0;
        }

        if(td->td_planarconfig == PLANARCONFIG_SEPARATE) {
            format.num_channels = 1;
            basic_info.num_color_channels = 1;
            basic_info.num_extra_channels = 0;
            basic_info.alpha_bits = 0;
            basic_info.alpha_exponent_bits=0;
        } else {
            format.num_channels = td->td_samplesperpixel;
            switch(td->td_samplesperpixel) {
                case 1:
                    format.num_channels = 1;
                    basic_info.num_color_channels = 1;
                    basic_info.num_extra_channels = 0;
                    basic_info.alpha_bits = 0;
                    basic_info.alpha_exponent_bits = 0;
                    break;
                case 2:
                    format.num_channels = 2;
                    basic_info.num_color_channels = 1;
                    basic_info.num_extra_channels = 1;
                    basic_info.alpha_bits = td->td_bitspersample;
                    basic_info.alpha_exponent_bits = basic_info.exponent_bits_per_sample;
                    break;
                case 3:
                    format.num_channels = 3;
                    basic_info.num_color_channels = 3;
                    basic_info.num_extra_channels = 0;
                    basic_info.alpha_bits = 0;
                    basic_info.alpha_exponent_bits = 0;
                    break;
                case 4:
                    format.num_channels = 4;
                    basic_info.num_color_channels = 3;
                    basic_info.num_extra_channels = 1;
                    basic_info.alpha_bits = td->td_bitspersample;
                    basic_info.alpha_exponent_bits = basic_info.exponent_bits_per_sample;
                    break;
            }
        }

        if( sp->lossless )
        {
            JxlEncoderOptionsSetLossless(opts, TRUE);
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
            if( JxlEncoderSetFrameDistance(opts, sp->distance) != JXL_ENC_SUCCESS )
#else
            if( JxlEncoderOptionsSetDistance(opts, sp->distance) != JXL_ENC_SUCCESS )
#endif
            {
                TIFFErrorExt(tif->tif_clientdata, module,
                            "JxlEncoderSetFrameDistance() failed");
                JxlEncoderDestroy(enc);
                return 0;
            }
        }
#ifdef HAVE_JxlEncoderFrameSettingsSetOption
        if( JxlEncoderFrameSettingsSetOption(opts, JXL_ENC_FRAME_SETTING_EFFORT, sp->effort) != JXL_ENC_SUCCESS )
#else
        if( JxlEncoderOptionsSetEffort(opts, sp->effort) != JXL_ENC_SUCCESS )
#endif
        {
            TIFFErrorExt(tif->tif_clientdata, module,
                         "JxlEncoderFrameSettingsSetOption() failed");
            JxlEncoderDestroy(enc);
            return 0;
        }


        if (JXL_ENC_SUCCESS != JxlEncoderSetBasicInfo(enc, &basic_info))
        {
            TIFFErrorExt(tif->tif_clientdata, module,
                         "JxlEncoderSetBasicInfo() failed");
            JxlEncoderDestroy(enc);
            return 0;
        }

        JxlColorEncoding color_encoding = {0};
        JxlColorEncodingSetToSRGB(&color_encoding, /*is_gray*/
            (td->td_planarconfig==PLANARCONFIG_SEPARATE ||
            td->td_samplesperpixel <= 2));
        if (JXL_ENC_SUCCESS != JxlEncoderSetColorEncoding(enc, &color_encoding))
        {
            TIFFErrorExt(tif->tif_clientdata, module,
                        "JxlEncoderSetColorEncoding() failed");
            JxlEncoderDestroy(enc);
            return 0;
        }

        if( JxlEncoderAddImageFrame(opts, &format, sp->uncompressed_buffer,
                                    sp->uncompressed_size) != JXL_ENC_SUCCESS )
        {
            TIFFErrorExt(tif->tif_clientdata, module,
                         "JxlEncoderAddImageFrame() failed");
            JxlEncoderDestroy(enc);
            return 0;
        }
        JxlEncoderCloseInput(enc);

        while( TRUE )
        {
            size_t len = (size_t)tif->tif_rawdatasize;
            uint8_t* buf = (uint8_t*)tif->tif_rawdata;
            JxlEncoderStatus process_result = JxlEncoderProcessOutput(enc, &buf, &len);
            if( process_result == JXL_ENC_ERROR )
            {
                TIFFErrorExt(tif->tif_clientdata, module,
                         "JxlEncoderProcessOutput() failed");
                JxlEncoderDestroy(enc);
                return 0;
            }
            tif->tif_rawcc = tif->tif_rawdatasize - len;
            if (!TIFFFlushData1(tif))
            {
                JxlEncoderDestroy(enc);
                return 0;
            }
            if( process_result != JXL_ENC_NEED_MORE_OUTPUT )
                break;
        }

        JxlEncoderDestroy(enc);
        return 1;
}

static void
JXLCleanup(TIFF* tif)
{
        JXLState* sp = LState(tif);

        assert(sp != 0);

        tif->tif_tagmethods.vgetfield = sp->vgetparent;
        tif->tif_tagmethods.vsetfield = sp->vsetparent;

        _TIFFfree(sp->uncompressed_buffer);

        if( sp->decoder )
            JxlDecoderDestroy(sp->decoder);

        _TIFFfree(sp);
        tif->tif_data = NULL;

        _TIFFSetDefaultCompressionState(tif);
}

static const TIFFField JXLFields[] = {
        { TIFFTAG_JXL_LOSSYNESS, 0, 0, TIFF_ANY, 0, TIFF_SETGET_UINT32,
          TIFF_SETGET_UNDEFINED,
          FIELD_PSEUDO, FALSE, FALSE, "Lossyness", NULL },
        { TIFFTAG_JXL_EFFORT, 0, 0, TIFF_ANY, 0, TIFF_SETGET_UINT32,
          TIFF_SETGET_UNDEFINED,
          FIELD_PSEUDO, FALSE, FALSE, "Effort", NULL },
        { TIFFTAG_JXL_DISTANCE, 0, 0, TIFF_ANY, 0, TIFF_SETGET_FLOAT,
          TIFF_SETGET_UNDEFINED,
          FIELD_PSEUDO, FALSE, FALSE, "Distance", NULL },
};

static int
JXLVSetField(TIFF* tif, uint32_t tag, va_list ap)
{
	static const char module[] = "JXLVSetField";
        JXLState* sp = LState(tif);

        switch (tag) {
            case TIFFTAG_JXL_LOSSYNESS:
            {
                uint32_t lossyness = va_arg(ap, uint32_t);
                if( lossyness == JXL_LOSSLESS )
                    sp->lossless = TRUE;
                else if( lossyness == JXL_LOSSY )
                    sp->lossless = FALSE;
                else
                {
                    TIFFErrorExt(tif->tif_clientdata, module,
                            "Invalid value for Lossyness: %u", lossyness);
                    return 0;
                }
                return 1;
            }

            case TIFFTAG_JXL_EFFORT:
            {
                uint32_t effort = va_arg(ap, uint32_t);
                if( effort < 1 || effort > 9)
                {
                    TIFFErrorExt(tif->tif_clientdata, module,
                            "Invalid value for Effort: %u", effort);
                    return 0;
                }
                sp->effort = effort;
                return 1;
            }

            case TIFFTAG_JXL_DISTANCE:
            {
                float distance = (float)va_arg(ap, double);
                if( distance < 0 || distance > 15)
                {
                    TIFFErrorExt(tif->tif_clientdata, module,
                            "Invalid value for Distance: %f", distance);
                    return 0;
                }
                sp->distance = distance;
                return 1;
            }

            default:
            {
                return (*sp->vsetparent)(tif, tag, ap);
            }
        }
        /*NOTREACHED*/
}

static int
JXLVGetField(TIFF* tif, uint32_t tag, va_list ap)
{
        JXLState* sp = LState(tif);

        switch (tag) {
            case TIFFTAG_JXL_LOSSYNESS:
                *va_arg(ap, uint32_t*) = sp->lossless ? JXL_LOSSLESS : JXL_LOSSY;
                break;
            case TIFFTAG_JXL_EFFORT:
                *va_arg(ap, uint32_t*) = sp->effort;
                break;
            case TIFFTAG_JXL_DISTANCE:
                *va_arg(ap, float*) = sp->distance;
                break;
            default:
                return (*sp->vgetparent)(tif, tag, ap);
        }
        return 1;
}

int TIFFInitJXL(TIFF* tif, int scheme)
{
        static const char module[] = "TIFFInitJXL";
        JXLState* sp;

        (void)scheme;
        assert( scheme == COMPRESSION_JXL );

        /*
        * Merge codec-specific tag information.
        */
        if (!_TIFFMergeFields(tif, JXLFields, TIFFArrayCount(JXLFields))) {
                TIFFErrorExt(tif->tif_clientdata, module,
                            "Merging JXL codec-specific tags failed");
                return 0;
        }

        /*
        * Allocate state block so tag methods have storage to record values.
        */
        tif->tif_data = (uint8_t*) _TIFFcalloc(1, sizeof(JXLState));
        if (tif->tif_data == NULL)
                goto bad;
        sp = LState(tif);

        /*
        * Override parent get/set field methods.
        */
        sp->vgetparent = tif->tif_tagmethods.vgetfield;
        tif->tif_tagmethods.vgetfield = JXLVGetField;	/* hook for codec tags */
        sp->vsetparent = tif->tif_tagmethods.vsetfield;
        tif->tif_tagmethods.vsetfield = JXLVSetField;	/* hook for codec tags */

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

        return 1;
bad:
        TIFFErrorExt(tif->tif_clientdata, module,
                    "No space for JXL state block");
        return 0;
}

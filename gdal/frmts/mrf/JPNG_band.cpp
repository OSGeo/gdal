/*
* Copyright 2016 Esri
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

/*
* JPNG band, uses JPEG or PNG encoding, depending on the input data
*/

#include "marfa.h"
#include <cassert>

CPL_CVSID("$Id$");

CPL_C_START
#include <jpeglib.h>

#ifdef INTERNAL_PNG
#include "../png/libpng/png.h"
#else
#include <png.h>
#endif
CPL_C_END

NAMESPACE_MRF_START

// Test that all alpha values are equal to N
template<int N> static bool AllAlpha(const buf_mgr &src, const ILImage &img) {
    int stride = img.pagesize.c;
    char *s = src.buffer + img.pagesize.c - 1;
    char *stop = src.buffer + img.pageSizeBytes;
    while (s < stop && N == static_cast<unsigned char>(*s))
        s += stride;
    return s >= stop;
}

// Fully opaque
#define opaque AllAlpha<255>
// Fully transparent
#define transparent AllAlpha<0>

// Strip the alpha from an RGBA buffer, safe to use in place
static void RGBA2RGB(const char *start, const char *stop, char *target) {
    while (start < stop) {
        *target++ = *start++;
        *target++ = *start++;
        *target++ = *start++;
        start++; // Skip the alpha
    }
}

// Add opaque alpha to an RGB buffer, safe to use in place
// works from stop to start, the last parameter is the end of the source region
static void RGB2RGBA(const char *start, char *stop, const char *source_end) {
    while (start < stop) {
        --stop;
        *(reinterpret_cast<unsigned char*>(stop)) = 0xff;
        *--stop = *--source_end;
        *--stop = *--source_end;
        *--stop = *--source_end;
    }
}

// Strip the alpha from an Luma Alpha buffer, safe to use in place
static void LA2L(const char *start, const char *stop, char *target) {
    while (start < stop) {
        *target++ = *start++;
        start++; // Skip the alpha
    }
}

// Add opaque alpha to a Luma buffer, safe to use in place
// works from stop to start, the last parameter is the end of the source region
static void L2LA(const char *start, char *stop, const char *source_end) {
    while (start < stop) {
        --stop;
        *(reinterpret_cast<unsigned char*>(stop)) = 0xff;
        *--stop = *--source_end;
    }
}

static CPLErr initBuffer(buf_mgr &b)
{
    b.buffer = (char *)(CPLMalloc(b.size));
    if (b.buffer != NULL)
        return CE_None;
    CPLError(CE_Failure, CPLE_OutOfMemory, "Allocating temporary JPNG buffer");
    return CE_Failure;
}

CPLErr JPNG_Band::Decompress(buf_mgr &dst, buf_mgr &src)
{
    CPLErr retval = CE_None;

    const static GUInt32 JPEG_SIG = 0xe0ffd8ff; // JPEG 4CC code
    const static GUInt32 PNG_SIG  = 0x474e5089;  // PNG 4CC code

    ILImage image(img);
    GUInt32 signature;
    memcpy(&signature, src.buffer, sizeof(GUInt32));

    // test against an LSB signature
    if (JPEG_SIG == CPL_LSBWORD32(signature)) {
        image.pagesize.c -= 1;
        JPEG_Codec codec(image);

        // JPEG decoder expects the destination size to be accurate
        buf_mgr temp = dst; // dst still owns the storage
        temp.size = (image.pagesize.c == 3) ? dst.size / 4 * 3 : dst.size / 2;

        retval = codec.DecompressJPEG(temp, src);
        if (CE_None == retval) { // add opaque alpha, in place
            if (image.pagesize.c == 3)
                RGB2RGBA(dst.buffer, dst.buffer + dst.size, temp.buffer + temp.size);
            else
                L2LA(dst.buffer, dst.buffer + dst.size, temp.buffer + temp.size);
        }
    }
    else { // Should be PNG
        assert(PNG_SIG == CPL_LSBWORD32(signature));
        PNG_Codec codec(image);
        // PNG codec expands to 4 bands
        return codec.DecompressPNG(dst, src);
    }

    return retval;
}

// The PNG internal palette is set on first band write
CPLErr JPNG_Band::Compress(buf_mgr &dst, buf_mgr &src)
{
    ILImage image(img);
    CPLErr retval = CE_None;

    buf_mgr temp = { NULL, static_cast<size_t>(img.pageSizeBytes) };
    retval = initBuffer(temp);
    if (retval != CE_None)
        return retval;

    try {
        if (opaque(src, image)) { // If all pixels are opaque, compress as JPEG
            if (image.pagesize.c == 4)
                RGBA2RGB(src.buffer, src.buffer + src.size, temp.buffer);
            else
                LA2L(src.buffer, src.buffer + src.size, temp.buffer);

            image.pagesize.c -= 1; // RGB or Grayscale only for JPEG
            JPEG_Codec codec(image);
            codec.rgb = rgb;
            codec.optimize = optimize;
            codec.sameres = sameres;
            retval = codec.CompressJPEG(dst, temp);
        }
        else if (transparent(src, image)) {
            dst.size = 0; // Don't store fully transparent pages
        }
        else
        {
            PNG_Codec codec(image);
            codec.deflate_flags = deflate_flags;
            retval = codec.CompressPNG(dst, src);
        }
    }
    catch (CPLErr err) {
        retval = err;
    }

    CPLFree(temp.buffer);
    return retval;
}

/**
* \brief For PPNG, builds the data structures needed to write the palette
* The presence of the PNGColors and PNGAlpha is used as a flag for PPNG only
*/

JPNG_Band::JPNG_Band( GDALMRFDataset *pDS, const ILImage &image,
                      int b, int level ) :
    GDALMRFRasterBand(pDS, image, b, level),
    rgb(FALSE),
    sameres(FALSE),
    optimize(false)
{   // Check error conditions
    if( image.dt != GDT_Byte )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Data type not supported by MRF JPNG");
        return;
    }
    if( image.order != IL_Interleaved ||
        (image.pagesize.c != 4 && image.pagesize.c != 2) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "MRF JPNG can only handle 2 or 4 interleaved bands");
        return;
    }

    if( img.pagesize.c == 4 )
    { // RGBA can have storage flavors
        CPLString const &pm = pDS->GetPhotometricInterpretation();
        if (pm == "RGB" || pm == "MULTISPECTRAL")
        { // Explicit RGB or MS
            rgb = TRUE;
            sameres = TRUE;
        }
        if (pm == "YCC")
            sameres = TRUE;
    }

    optimize = GetOptlist().FetchBoolean("OPTIMIZE", FALSE) != FALSE;

    // PNGs and JPGs can be larger than the source, especially for
    // small page size.
    poDS->SetPBufferSize(image.pageSizeBytes + 100);
}

JPNG_Band::~JPNG_Band() {}

NAMESPACE_MRF_END

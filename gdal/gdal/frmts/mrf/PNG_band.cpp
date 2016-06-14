/*
* Copyright (c) 2002-2012, California Institute of Technology.
* All rights reserved.  Based on Government Sponsored Research under contracts NAS7-1407 and/or NAS7-03001.
*
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
*   1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
*   2. Redistributions in binary form must reproduce the above copyright notice,
*      this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
*   3. Neither the name of the California Institute of Technology (Caltech), its operating division the Jet Propulsion Laboratory (JPL),
*      the National Aeronautics and Space Administration (NASA), nor the names of its contributors may be used to
*      endorse or promote products derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL THE CALIFORNIA INSTITUTE OF TECHNOLOGY BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* Copyright 2014-2015 Esri
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
 * $Id$
 * PNG band
 * PNG page compression and decompression functions
 * These functions are not methods, they reside in the global space
 *
 */

#include "marfa.h"
#include <cassert>

CPL_C_START
#ifdef INTERNAL_PNG
#include "../png/libpng/png.h"
#else
#include <png.h>
#endif
CPL_C_END

NAMESPACE_MRF_START

// Do Nothing
static void flush_png(png_structp) {}

// Warning Emit
static void pngWH(png_struct * /*png*/, png_const_charp message)
{
    CPLError(CE_Warning, CPLE_AppDefined, "MRF: PNG warning %s", message);
}

// Fatal Warning
static void pngEH(png_struct *png, png_const_charp message)
{
    CPLError(CE_Failure, CPLE_AppDefined, "MRF: PNG Failure %s", message);
    longjmp(png_jmpbuf(png), 1);
}

// Read memory handlers for PNG
// No check for attempting to read past the end of the buffer

static void read_png(png_structp pngp, png_bytep data, png_size_t length)
{
    buf_mgr *pmgr = (buf_mgr *)png_get_io_ptr(pngp);
    if( pmgr->size < length )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "MRF: PNG Failure: Not enough bytes in buffer");
        longjmp(png_jmpbuf(pngp), 1);
    }
    memcpy(data, pmgr->buffer, length);
    pmgr->buffer += length;
    pmgr->size -= length;
}

static void write_png(png_structp pngp, png_bytep data, png_size_t length) {
    buf_mgr *mgr = (buf_mgr *)png_get_io_ptr(pngp);
    // Buffer could be too small, trigger an error on debug mode
    assert(length <= mgr->size);
    memcpy(mgr->buffer, data, length);
    mgr->buffer += length;
    mgr->size -= length;
}

/**
*\brief In memory decompression of PNG file
*/

CPLErr PNG_Codec::DecompressPNG(buf_mgr &dst, buf_mgr &src)
{
    png_bytep* png_rowp = NULL;
    volatile png_bytep *p_volatile_png_rowp = (volatile png_bytep *)&png_rowp;

    // pngp=png_create_read_struct(PNG_LIBPNG_VER_STRING,0,pngEH,pngWH);
    png_structp pngp = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (NULL == pngp) {
        CPLError(CE_Failure, CPLE_AppDefined, "MRF: Error creating PNG decompress");
        return CE_Failure;
    }

    png_infop infop = png_create_info_struct(pngp);
    if (NULL == infop) {
        if (pngp) png_destroy_read_struct(&pngp, &infop, NULL);
        CPLError(CE_Failure, CPLE_AppDefined, "MRF: Error creating PNG info");
        return CE_Failure;
    }

    if (setjmp(png_jmpbuf(pngp))) {
        CPLError(CE_Failure, CPLE_AppDefined, "MRF: Error during PNG decompress");
        CPLFree((void*)(*p_volatile_png_rowp));
        png_destroy_read_struct(&pngp, &infop, NULL);
        return CE_Failure;
    }

    // The mgr data ptr is already set up
    png_set_read_fn(pngp, &src, read_png);
    // Ready to read
    png_read_info(pngp, infop);
    GInt32 height = static_cast<GInt32>(png_get_image_height(pngp, infop));
    GInt32 byte_count = png_get_bit_depth(pngp, infop) / 8;
    // Check the size
    if (dst.size < (png_get_rowbytes(pngp, infop)*height)) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "MRF: PNG Page data bigger than the buffer provided");
        png_destroy_read_struct(&pngp, &infop, NULL);
        return CE_Failure;
    }

    png_rowp = (png_bytep *)CPLMalloc(sizeof(png_bytep)*height);

    int rowbytes = static_cast<int>(png_get_rowbytes(pngp, infop));
    for (int i = 0; i < height; i++)
        png_rowp[i] = (png_bytep)dst.buffer + i*rowbytes;

    // Finally, the read
    // This is the lower level, the png_read_end allows some transforms
    // Like palette to RGBA
    png_read_image(pngp, png_rowp);

    if (byte_count != 1) { // Swap from net order if data is short
        for (int i = 0; i < height; i++) {
            unsigned short int*p = (unsigned short int *)png_rowp[i];
            for (int j = 0; j < rowbytes / 2; j++, p++)
                *p = net16(*p);
        }
    }

    //    ppmWrite("Test.ppm",(char *)data,ILSize(512,512,1,4,0));
    // Required
    png_read_end(pngp, infop);

    // png_set_rows(pngp,infop,png_rowp);
    // png_read_png(pngp,infop,PNG_TRANSFORM_IDENTITY,0);

    CPLFree(png_rowp);
    png_destroy_read_struct(&pngp, &infop, NULL);
    return CE_None;
}

/**
*\Brief Compress a page in PNG format
* Returns the compressed size in dst.size
*
*/

CPLErr PNG_Codec::CompressPNG(buf_mgr &dst, buf_mgr &src)

{
    png_structp pngp;
    png_infop infop;
    buf_mgr mgr = dst;

    pngp = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, pngEH, pngWH);
    if (!pngp) {
        CPLError(CE_Failure, CPLE_AppDefined, "MRF: Error creating png structure");
        return CE_Failure;
    }
    infop = png_create_info_struct(pngp);
    if (!infop) {
        png_destroy_write_struct(&pngp, NULL);
        CPLError(CE_Failure, CPLE_AppDefined, "MRF: Error creating png info structure");
        return CE_Failure;
    }

    if (setjmp(png_jmpbuf(pngp))) {
        png_destroy_write_struct(&pngp, &infop);
        CPLError(CE_Failure, CPLE_AppDefined, "MRF: Error during png init");
        return CE_Failure;
    }

    png_set_write_fn(pngp, &mgr, write_png, flush_png);

    int png_ctype;

    switch (img.pagesize.c) {
    case 1: if (PNGColors != NULL) png_ctype = PNG_COLOR_TYPE_PALETTE;
            else png_ctype = PNG_COLOR_TYPE_GRAY;
            break;
    case 2: png_ctype = PNG_COLOR_TYPE_GRAY_ALPHA; break;
    case 3: png_ctype = PNG_COLOR_TYPE_RGB; break;
    case 4: png_ctype = PNG_COLOR_TYPE_RGB_ALPHA; break;
    default: { // This never happens if we check at the open
        CPLError(CE_Failure, CPLE_AppDefined, "MRF:PNG Write with %d colors called",
            img.pagesize.c);
        return CE_Failure;
    }
    }

    png_set_IHDR(pngp, infop, img.pagesize.x, img.pagesize.y,
        GDALGetDataTypeSize(img.dt), png_ctype,
        PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    // Optional, force certain filters only.  Makes it somewhat faster but worse compression
    // png_set_filter(pngp, PNG_FILTER_TYPE_BASE, PNG_FILTER_SUB);

#if defined(PNG_LIBPNG_VER) && (PNG_LIBPNG_VER > 10200) && defined(PNG_SELECT_READ)
    png_uint_32 mask, flags;

    flags = png_get_asm_flags(pngp);
    mask = png_get_asm_flagmask(PNG_SELECT_READ | PNG_SELECT_WRITE);
    png_set_asm_flags(pngp, flags | mask); // use flags &~mask to disable all

    // Test that the MMX is compiled into PNG
    //	fprintf(stderr,"MMX support is %d\n", png_mmx_support());

#endif

    // Let the quality control the compression level
    // Supposedly low numbers work well too while being fast
    png_set_compression_level(pngp, img.quality / 10);

    // Custom strategy for zlib, set using the band option Z_STRATEGY
    if (deflate_flags & ZFLAG_SMASK)
        png_set_compression_strategy(pngp, (deflate_flags & ZFLAG_SMASK) >> 6);

    // Write the palette and the transparencies if they exist
    if (PNGColors != NULL)
    {
        png_set_PLTE(pngp, infop, (png_colorp)PNGColors, PalSize);
        if (TransSize != 0)
            png_set_tRNS(pngp, infop, (unsigned char*)PNGAlpha, TransSize, NULL);
    }

    png_write_info(pngp, infop);

    png_bytep *png_rowp = (png_bytep *)CPLMalloc(sizeof(png_bytep)*img.pagesize.y);

    if (setjmp(png_jmpbuf(pngp))) {
        CPLFree(png_rowp);
        png_destroy_write_struct(&pngp, &infop);
        CPLError(CE_Failure, CPLE_AppDefined, "MRF: Error during png compression");
        return CE_Failure;
    }

    int rowbytes = static_cast<int>(png_get_rowbytes(pngp, infop));
    for (int i = 0; i < img.pagesize.y; i++) {
        png_rowp[i] = (png_bytep)(src.buffer + i*rowbytes);
        if (img.dt != GDT_Byte) { // Swap to net order if data is short
            unsigned short int*p = (unsigned short int *)png_rowp[i];
            for (int j = 0; j < rowbytes / 2; j++, p++) *p = net16(*p);
        }
    }

    png_write_image(pngp, png_rowp);
    png_write_end(pngp, infop);

    // Done
    CPLFree(png_rowp);
    png_destroy_write_struct(&pngp, &infop);

    // Done
    // mgr.size holds the available bytes, so the size of the compressed png
    // is the original destination size minus the still available bytes
    dst.size -= mgr.size;

    return CE_None;
}

// Builds a PNG palette from a GDAL color table
static void ResetPalette(GDALColorTable *poCT, PNG_Codec &codec)
{   // Convert the GDAL LUT to PNG style
    codec.TransSize = codec.PalSize = poCT->GetColorEntryCount();

    png_color *pasPNGColors = (png_color *)CPLMalloc(sizeof(png_color) * codec.PalSize);
    unsigned char *pabyAlpha = (unsigned char *)CPLMalloc(codec.TransSize);
    codec.PNGColors = (void *)pasPNGColors;
    codec.PNGAlpha = (void *)pabyAlpha;
    bool NoTranspYet = true;

    // Set the palette from the end to reduce the size of the opacity mask
    for (int iColor = codec.PalSize - 1; iColor >= 0; iColor--)
    {
        GDALColorEntry  sEntry;
        poCT->GetColorEntryAsRGB(iColor, &sEntry);

        pasPNGColors[iColor].red = (png_byte)sEntry.c1;
        pasPNGColors[iColor].green = (png_byte)sEntry.c2;
        pasPNGColors[iColor].blue = (png_byte)sEntry.c3;
        if (NoTranspYet && sEntry.c4 == 255)
            codec.TransSize--;
        else {
            NoTranspYet = false;
            pabyAlpha[iColor] = (unsigned char)sEntry.c4;
        }
    }
}

CPLErr PNG_Band::Decompress(buf_mgr &dst, buf_mgr &src)
{
    return codec.DecompressPNG(dst, src);
}

CPLErr PNG_Band::Compress(buf_mgr &dst, buf_mgr &src)
{   
    if (!codec.PNGColors && img.comp == IL_PPNG) { // Late set PNG palette to conserve memory
        GDALColorTable *poCT = GetColorTable();
        if (!poCT) {
            CPLError(CE_Failure, CPLE_NotSupported, "MRF PPNG needs a color table");
            return CE_Failure;
        }
        ResetPalette(poCT, codec);
    }

    codec.deflate_flags = deflate_flags;
    return codec.CompressPNG(dst, src);
}

/**
 * \Brief For PPNG, builds the data structures needed to write the palette
 * The presence of the PNGColors and PNGAlpha is used as a flag for PPNG only
 */

PNG_Band::PNG_Band(GDALMRFDataset *pDS, const ILImage &image, int b, int level) :
GDALMRFRasterBand(pDS, image, b, level), codec(image)

{   // Check error conditions
    if (image.dt != GDT_Byte && image.dt != GDT_Int16 && image.dt != GDT_UInt16) {
        CPLError(CE_Failure, CPLE_NotSupported, "Data type not supported by MRF PNG");
        return;
    }
    if (image.pagesize.c > 4) {
        CPLError(CE_Failure, CPLE_NotSupported, "MRF PNG can only handle up to 4 bands per page");
        return;
    }
    // PNGs can be larger than the source, especially for small page size
    poDS->SetPBufferSize( image.pageSizeBytes + 100);
}

NAMESPACE_MRF_END

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
 * JPEG band
 * JPEG page compression and decompression functions, gets compiled twice
 * LIBJPEG_12_H is defined if both 8 and 12 bit JPEG will be supported
 * JPEG12_ON    is defined for the 12 bit versions
 */

#include "marfa.h"
#include <setjmp.h>

CPL_C_START
#include <jpeglib.h>
CPL_C_END

NAMESPACE_MRF_START

/**
*\Brief Helper class for jpeg error management
*/

struct ErrorMgr : public jpeg_error_mgr {
    inline ErrorMgr();
    int signaled() { return setjmp(setjmpBuffer); };
    jmp_buf setjmpBuffer;
};

/**
*\brief Called when jpeg wants to report a warning
* msgLevel can be:
* -1 Corrupt data
* 0 always display
* 1... Trace level
*/

static void emitMessage(j_common_ptr cinfo, int msgLevel)
{
    jpeg_error_mgr* err = cinfo->err;
    if (msgLevel > 0) return; // No trace msgs
    // There can be many warnings, just print the first one
    if (err->num_warnings++ >1) return;
    char buffer[JMSG_LENGTH_MAX];
    err->format_message(cinfo, buffer);
    CPLError(CE_Failure, CPLE_AppDefined, "%s", buffer);
}

static void errorExit(j_common_ptr cinfo)
{
    ErrorMgr* err = (ErrorMgr*)cinfo->err;
    // format the warning message
    char buffer[JMSG_LENGTH_MAX];

    err->format_message(cinfo, buffer);
    CPLError(CE_Failure, CPLE_AppDefined, "%s", buffer);
    // return control to the setjmp point
    longjmp(err->setjmpBuffer, 1);
}

/**
*\bried set up the normal JPEG error routines, then override error_exit
*/
ErrorMgr::ErrorMgr()
{
    jpeg_std_error(this);
    error_exit = errorExit;
    emit_message = emitMessage;
}

/**
*\Brief Do nothing stub function for JPEG library, called
*/
static void stub_source_dec(j_decompress_ptr /*cinfo*/) {};

/**
*\Brief: Do nothing stub function for JPEG library, called?
*/
static boolean fill_input_buffer_dec(j_decompress_ptr /*cinfo*/) { return TRUE; };

/**
*\Brief: Do nothing stub function for JPEG library, not called
*/
static void skip_input_data_dec(j_decompress_ptr /*cinfo*/, long /*l*/) {};

// Destination should be already set up
static void init_or_terminate_destination(j_compress_ptr /*cinfo*/) {}

// Called if the buffer provided is too small
static boolean empty_output_buffer(j_compress_ptr /*cinfo*/) {
    std::cerr << "JPEG Output buffer empty called\n";
    return FALSE;
}

/*
 *\Brief Compress a JPEG page
 * 
 * For now it only handles byte data, grayscale, RGB or CMYK
 *
 * Returns the compressed size in dest.size
 */
#if defined(JPEG12_ON)
CPLErr JPEG_Band::CompressJPEG12(buf_mgr &dst, buf_mgr &src)
#else
CPLErr JPEG_Band::CompressJPEG(buf_mgr &dst, buf_mgr &src)
#endif

{
    // The cinfo should stay open and reside in the DS, since it can be left initialized
    // It saves some time because it has the tables initialized
    struct jpeg_compress_struct cinfo;
    ErrorMgr jerr;

    jpeg_destination_mgr jmgr;
    jmgr.next_output_byte = (JOCTET *)dst.buffer;
    jmgr.free_in_buffer = dst.size;
    jmgr.init_destination = init_or_terminate_destination;
    jmgr.empty_output_buffer = empty_output_buffer;
    jmgr.term_destination = init_or_terminate_destination;

    // Look at the source of this, some interesting tidbits
    cinfo.err = &jerr;
    jpeg_create_compress(&cinfo);
    cinfo.dest = &jmgr;

    // The page specific info, size and color spaces
    cinfo.image_width = img.pagesize.x;
    cinfo.image_height = img.pagesize.y;
    cinfo.input_components = img.pagesize.c;
    switch (cinfo.input_components) {
    case 1:cinfo.in_color_space = JCS_GRAYSCALE; break;
    case 3:cinfo.in_color_space = JCS_RGB; break;  // Stored as YCbCr 4:2:0 by default
    default:
	cinfo.in_color_space = JCS_UNKNOWN; // 2, 4-10 bands
    }

    // Set all required fields and overwrite the ones we want to change
    jpeg_set_defaults(&cinfo);

    // Override certain settings
    jpeg_set_quality(&cinfo, img.quality, TRUE);
    cinfo.dct_method = JDCT_FLOAT; // Pretty fast and precise
    cinfo.optimize_coding = optimize; // Set "OPTIMIZE=TRUE" in OPTIONS, default for 12bit

    // Do we explicitly turn off the YCC color and downsampling?

    if (cinfo.in_color_space == JCS_RGB) {
	if (rgb) {  // Stored as RGB
	    jpeg_set_colorspace(&cinfo, JCS_RGB);  // Huge files
	} else if (sameres) { // YCC, somewhat larger files with improved color spatial detail
	    cinfo.comp_info[0].h_samp_factor = 1;
	    cinfo.comp_info[0].v_samp_factor = 1;

	    // Enabling these lines will make the color components use the same tables as Y, even larger file with slightly better color depth detail
	    // cinfo.comp_info[1].quant_tbl_no = 0;
	    // cinfo.comp_info[2].quant_tbl_no = 0;

	    // cinfo.comp_info[1].dc_tbl_no = 0;
	    // cinfo.comp_info[2].dc_tbl_no = 0;

	    // cinfo.comp_info[1].ac_tbl_no = 0;
	    // cinfo.comp_info[2].ac_tbl_no = 0;
	}
    }

    int linesize = cinfo.image_width*cinfo.num_components*((cinfo.data_precision == 8) ? 1 : 2);
    JSAMPROW *rowp = (JSAMPROW *)CPLMalloc(sizeof(JSAMPROW)*img.pagesize.y);
    for (int i = 0; i < img.pagesize.y; i++)
	rowp[i] = (JSAMPROW)(src.buffer + i*linesize);

    if (jerr.signaled()) {
	CPLError(CE_Failure, CPLE_AppDefined, "MRF: JPEG compression error");
	jpeg_destroy_compress(&cinfo);
	CPLFree(rowp);
	return CE_Failure;
    }

    jpeg_start_compress(&cinfo, TRUE);
    jpeg_write_scanlines(&cinfo, rowp, img.pagesize.y);
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    CPLFree(rowp);

    // Figure out the size
    dst.size -= jmgr.free_in_buffer;

    return CE_None;
}

/**
 *\brief In memory decompression of JPEG file
 *
 * @param data pointer to output buffer 
 * @param png pointer to PNG in memory
 * @param sz if non-zero, test that uncompressed data fits in the buffer.
 */
#if defined(JPEG12_ON)
CPLErr JPEG_Band::DecompressJPEG12(buf_mgr &dst, buf_mgr &isrc)
#else
CPLErr JPEG_Band::DecompressJPEG(buf_mgr &dst, buf_mgr &isrc) 
#endif

{
    int nbands = img.pagesize.c;
    // Locals, clean up after themselves
    jpeg_decompress_struct cinfo={0};
    ErrorMgr jerr;

    struct jpeg_source_mgr src={(JOCTET *)isrc.buffer, isrc.size};

    cinfo.err=&jerr;
    src.term_source = src.init_source = stub_source_dec;
    src.skip_input_data = skip_input_data_dec;
    src.fill_input_buffer = fill_input_buffer_dec;
    src.resync_to_restart = jpeg_resync_to_restart;

    if (jerr.signaled()) {
        CPLError(CE_Failure, CPLE_AppDefined, "MRF: Error reading JPEG page");
        return CE_Failure;
    }
    jpeg_create_decompress(&cinfo);
    cinfo.src = &src;
    jpeg_read_header(&cinfo, TRUE);
    // Use float, it is actually faster than the ISLOW method by a tiny bit
    cinfo.dct_method = JDCT_FLOAT;

    //
    // Tolerate different input if we can do the conversion
    // Gray and RGB for example
    // This also means that a RGB MRF can be read as grayscale and vice versa
    // If libJPEG can't convert it will throw an error
    // 
    if (nbands == 3 && cinfo.num_components != nbands)
	cinfo.out_color_space = JCS_RGB;
    if (nbands == 1 && cinfo.num_components != nbands)
	cinfo.out_color_space = JCS_GRAYSCALE;

    int linesize = cinfo.image_width * nbands * ((cinfo.data_precision == 8) ? 1 : 2);

    jpeg_start_decompress(&cinfo);

    // We have a missmatch between the real and the declared data format
    // warn and fail if output buffer is too small
    if (linesize*cinfo.image_height!=dst.size) {
        CPLError(CE_Warning,CPLE_AppDefined,"MRF: read JPEG size is wrong");
        if (linesize*cinfo.image_height>dst.size) {
            CPLError(CE_Failure,CPLE_AppDefined,"MRF: JPEG decompress buffer overflow");
            jpeg_destroy_decompress(&cinfo);
            return CE_Failure;
        }
    }
    // Decompress, two lines at a time
    while (cinfo.output_scanline < cinfo.image_height ) {
        char *rp[2];
        rp[0]=(char *)dst.buffer+linesize*cinfo.output_scanline;
        rp[1]=rp[0]+linesize;
        // if this fails, it calls the error handler
        // which will report an error
        jpeg_read_scanlines(&cinfo,JSAMPARRAY(rp),2);
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return CE_None;
}

// This part get done only once
#if !defined(JPEG12_ON)

// Type dependent dispachers
CPLErr JPEG_Band::Decompress(buf_mgr &dst, buf_mgr &src)
{
#if defined(LIBJPEG_12_H)
    if (GDT_Byte != img.dt)
	return DecompressJPEG12(dst, src);
#endif
    return DecompressJPEG(dst, src);
}

CPLErr JPEG_Band::Compress(buf_mgr &dst, buf_mgr &src)
{
#if defined(LIBJPEG_12_H)
    if (GDT_Byte != img.dt)
	return CompressJPEG12(dst, src);
#endif
    return CompressJPEG(dst, src);
}

// PHOTOMETRIC == MULTISPECTRAL turns off YCbCr conversion and downsampling
JPEG_Band::JPEG_Band(GDALMRFDataset *pDS, const ILImage &image, int b, int level) :
GDALMRFRasterBand(pDS, image, b, int(level)), sameres(FALSE), rgb(FALSE)
{
    int nbands = image.pagesize.c;
    //  TODO: Add 12bit JPEG support
    // Check behavior on signed 16bit.  Does the libjpeg sign extend?
#if defined(LIBJPEG_12_H)
    if (GDT_Byte != image.dt && GDT_UInt16 != image.dt) {
#else
    if (GDT_Byte != image.dt) {
#endif
	CPLError(CE_Failure, CPLE_NotSupported, "Data type not supported by MRF JPEG");
	return;
    }

    if (nbands == 3) { // Only the 3 band JPEG has storage flavors
	CPLString const &pm = pDS->GetPhotometricInterpretation();
	if (pm == "RGB" || pm == "MULTISPECTRAL") { // Explicit RGB or MS
	    rgb = TRUE;
	    sameres = TRUE;
	}
	if (pm == "YCC")
	    sameres = TRUE;
    }

    if (GDT_Byte == image.dt)
	optimize = GetOptlist().FetchBoolean("OPTIMIZE", FALSE);
    else
	optimize = TRUE; // Required for 12bit
}
#endif

NAMESPACE_MRF_END

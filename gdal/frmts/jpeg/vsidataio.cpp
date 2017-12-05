/******************************************************************************
 *
 * Project:  JPEG JFIF Driver
 * Purpose:  Implement JPEG read/write io indirection through VSI.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *           Code partially derived from libjpeg jdatasrc.c and jdatadst.c.
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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

#include "cpl_port.h"
#include "vsidataio.h"

#include <cstddef>

CPL_CVSID("$Id$")

CPL_C_START
#include "jerror.h"
CPL_C_END

// Expanded data source object for stdio input.

typedef struct
{
    struct jpeg_source_mgr pub;  // public fields.

    VSILFILE *infile;       // Source stream.
    JOCTET *buffer;         // Start of buffer.
    boolean start_of_file;  // Have we gotten any data yet?
} my_source_mgr;

typedef my_source_mgr *my_src_ptr;

// Choose an efficiently fread'able size.
static const size_t INPUT_BUF_SIZE = 4096;

// Initialize source --- called by jpeg_read_header
// before any data is actually read.

METHODDEF(void)
init_source(j_decompress_ptr cinfo)
{
    my_src_ptr src = (my_src_ptr)cinfo->src;

    // We reset the empty-input-file flag for each image,
    // but we don't clear the input buffer.
    // This is correct behavior for reading a series of images from one source.
    src->start_of_file = TRUE;
}

// Fill the input buffer --- called whenever buffer is emptied.
//
// In typical applications, this should read fresh data into the buffer
// (ignoring the current state of next_input_byte & bytes_in_buffer),
// reset the pointer & count to the start of the buffer, and return TRUE
// indicating that the buffer has been reloaded.  It is not necessary to
// fill the buffer entirely, only to obtain at least one more byte.
//
// There is no such thing as an EOF return.  If the end of the file has been
// reached, the routine has a choice of ERREXIT() or inserting fake data into
// the buffer.  In most cases, generating a warning message and inserting a
// fake EOI marker is the best course of action --- this will allow the
// decompressor to output however much of the image is there.  However,
// the resulting error message is misleading if the real problem is an empty
// input file, so we handle that case specially.
//
// In applications that need to be able to suspend compression due to input
// not being available yet, a FALSE return indicates that no more data can be
// obtained right now, but more may be forthcoming later.  In this situation,
// the decompressor will return to its caller (with an indication of the
// number of scanlines it has read, if any).  The application should resume
// decompression after it has loaded more data into the input buffer.  Note
// that there are substantial restrictions on the use of suspension --- see
// the documentation.
//
// When suspending, the decompressor will back up to a convenient restart point
// (typically the start of the current MCU). next_input_byte & bytes_in_buffer
// indicate where the restart point will be if the current call returns FALSE.
// Data beyond this point must be rescanned after resumption, so move it to
// the front of the buffer rather than discarding it.

METHODDEF(boolean)
fill_input_buffer(j_decompress_ptr cinfo)
{
    my_src_ptr src = (my_src_ptr)cinfo->src;
    size_t nbytes = VSIFReadL(src->buffer, 1, INPUT_BUF_SIZE, src->infile);

    if(nbytes == 0)
    {
        if(src->start_of_file)
        {
            // Treat empty input file as fatal error.
            cinfo->err->msg_code = JERR_INPUT_EMPTY;
            cinfo->err->error_exit((j_common_ptr)(cinfo));
        }
        WARNMS(cinfo, JWRN_JPEG_EOF);
        // Insert a fake EOI marker.
        src->buffer[0] = (JOCTET)0xFF;
        src->buffer[1] = (JOCTET)JPEG_EOI;
        nbytes = 2;
    }

    src->pub.next_input_byte = src->buffer;
    src->pub.bytes_in_buffer = nbytes;
    src->start_of_file = FALSE;

    return TRUE;
}

// The Intel IPP performance libraries do not necessarily read the
// entire contents of the buffer with each pass, so each re-fill
// copies the remaining buffer bytes to the front of the buffer,
// then fills up the rest with new data.
#ifdef IPPJ_HUFF
METHODDEF(boolean)
fill_input_buffer_ipp(j_decompress_ptr cinfo)
{
    my_src_ptr src = (my_src_ptr)cinfo->src;
    size_t bytes_left = src->pub.bytes_in_buffer;
    size_t bytes_to_read = INPUT_BUF_SIZE - bytes_left;

    if(src->start_of_file || cinfo->progressive_mode)
    {
        return fill_input_buffer(cinfo);
    }

    memmove(src->buffer, src->pub.next_input_byte, bytes_left);

    size_t nbytes =
        VSIFReadL(src->buffer + bytes_left, 1, bytes_to_read, src->infile);

    if(nbytes <= 0)
    {
        if(src->start_of_file)
        {
            // Treat empty input file as fatal error.
            ERREXIT(cinfo, JERR_INPUT_EMPTY);
        }

        if(src->pub.bytes_in_buffer == 0 && cinfo->unread_marker == 0)
        {
            WARNMS(cinfo, JWRN_JPEG_EOF);

            // Insert a fake EOI marker.
            src->buffer[0] = (JOCTET)0xFF;
            src->buffer[1] = (JOCTET)JPEG_EOI;
            nbytes = 2;
        }

        src->pub.next_input_byte = src->buffer;
        src->pub.bytes_in_buffer = bytes_left + nbytes;
        src->start_of_file = FALSE;

        return TRUE;
    }

    src->pub.next_input_byte = src->buffer;
    src->pub.bytes_in_buffer = bytes_left + nbytes;
    src->start_of_file = FALSE;

    return TRUE;
}
#endif  // IPPJ_HUFF

// Skip data --- used to skip over a potentially large amount of
// uninteresting data (such as an APPn marker).
//
// Writers of suspendable-input applications must note that skip_input_data
// is not granted the right to give a suspension return.  If the skip extends
// beyond the data currently in the buffer, the buffer can be marked empty so
// that the next read will cause a fill_input_buffer call that can suspend.
// Arranging for additional bytes to be discarded before reloading the input
// buffer is the application writer's problem.

METHODDEF(void)
skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
    my_src_ptr src = (my_src_ptr)cinfo->src;

    // Just a dumb implementation for now.  Could use fseek() except
    // it doesn't work on pipes.  Not clear that being smart is worth
    // any trouble anyway --- large skips are infrequent.
    if(num_bytes > 0)
    {
        while(num_bytes > (long)src->pub.bytes_in_buffer)
        {
            num_bytes -= (long)src->pub.bytes_in_buffer;
            (void)fill_input_buffer(cinfo);
            // note we assume that fill_input_buffer will never return FALSE,
            // so suspension need not be handled.
        }
        src->pub.next_input_byte += (size_t)num_bytes;
        src->pub.bytes_in_buffer -= (size_t)num_bytes;
    }
}

// An additional method that can be provided by data source modules is the
// resync_to_restart method for error recovery in the presence of RST markers.
// For the moment, this source module just uses the default resync method
// provided by the JPEG library.  That method assumes that no backtracking
// is possible.

// Terminate source --- called by jpeg_finish_decompress
// after all data has been read.  Often a no-op.
//
// NB://not* called by jpeg_abort or jpeg_destroy; surrounding
// application must deal with any cleanup that should happen even
// for error exit.

METHODDEF(void)
term_source(CPL_UNUSED j_decompress_ptr cinfo)
{
    // No work necessary here.
}

// Prepare for input from a stdio stream.
// The caller must have already opened the stream, and is responsible
// for closing it after finishing decompression.

void jpeg_vsiio_src(j_decompress_ptr cinfo, VSILFILE *infile)
{
    my_src_ptr src;

    // The source object and input buffer are made permanent so that a series
    // of JPEG images can be read from the same file by calling jpeg_stdio_src
    // only before the first one.  (If we discarded the buffer at the end of
    // one image, we'd likely lose the start of the next one.)
    // This makes it unsafe to use this manager and a different source
    // manager serially with the same JPEG object.  Caveat programmer.
    if(cinfo->src == NULL)
    {
        // First time for this JPEG object?
        cinfo->src = (struct jpeg_source_mgr *)(*cinfo->mem->alloc_small)(
            (j_common_ptr)cinfo, JPOOL_PERMANENT, sizeof(my_source_mgr));
        src = (my_src_ptr)cinfo->src;
        src->buffer = (JOCTET *)(*cinfo->mem->alloc_small)(
            (j_common_ptr)cinfo, JPOOL_PERMANENT,
            INPUT_BUF_SIZE * sizeof(JOCTET));
    }

    src = (my_src_ptr)cinfo->src;
    src->pub.init_source = init_source;
#ifdef IPPJ_HUFF
    src->pub.fill_input_buffer = fill_input_buffer_ipp;
#else
    src->pub.fill_input_buffer = fill_input_buffer;
#endif
    src->pub.skip_input_data = skip_input_data;
    src->pub.resync_to_restart = jpeg_resync_to_restart;  // Use default method.
    src->pub.term_source = term_source;
    src->infile = infile;
    src->pub.bytes_in_buffer = 0;     // Forces fill_input_buffer on first read.
    src->pub.next_input_byte = NULL;  // Until buffer loaded.
}

/* ==================================================================== */
/*      The rest was derived from jdatadst.c                            */
/* ==================================================================== */

// Expanded data destination object for stdio output.

typedef struct
{
    struct jpeg_destination_mgr pub;  // Public fields.

    VSILFILE *outfile;  // Target stream.
    JOCTET *buffer;     // Start of buffer.
} my_destination_mgr;

typedef my_destination_mgr *my_dest_ptr;

// choose an efficiently fwrite'able size.
static const size_t OUTPUT_BUF_SIZE = 4096;

// Initialize destination --- called by jpeg_start_compress
// before any data is actually written.

METHODDEF(void)
init_destination(j_compress_ptr cinfo)
{
    my_dest_ptr dest = (my_dest_ptr)cinfo->dest;

    // Allocate the output buffer --- it will be released when done with image.
    dest->buffer = (JOCTET *)(*cinfo->mem->alloc_small)(
        (j_common_ptr)cinfo, JPOOL_IMAGE, OUTPUT_BUF_SIZE * sizeof(JOCTET));

    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}

// Empty the output buffer --- called whenever buffer fills up.
//
// In typical applications, this should write the entire output buffer
// (ignoring the current state of next_output_byte & free_in_buffer),
// reset the pointer & count to the start of the buffer, and return TRUE
// indicating that the buffer has been dumped.
//
// In applications that need to be able to suspend compression due to output
// overrun, a FALSE return indicates that the buffer cannot be emptied now.
// In this situation, the compressor will return to its caller (possibly with
// an indication that it has not accepted all the supplied scanlines).  The
// application should resume compression after it has made more room in the
// output buffer.  Note that there are substantial restrictions on the use of
// suspension --- see the documentation.
//
// When suspending, the compressor will back up to a convenient restart point
// (typically the start of the current MCU). next_output_byte & free_in_buffer
// indicate where the restart point will be if the current call returns FALSE.
// Data beyond this point will be regenerated after resumption, so do not
// write it out when emptying the buffer externally.

METHODDEF(boolean)
empty_output_buffer(j_compress_ptr cinfo)
{
    my_dest_ptr dest = (my_dest_ptr)cinfo->dest;
    size_t bytes_to_write = OUTPUT_BUF_SIZE;

#ifdef IPPJ_HUFF
    // The Intel IPP performance libraries do not necessarily fill up
    // the whole output buffer with each compression pass, so we only
    // want to write out the parts of the buffer that are full.
    if(!cinfo->progressive_mode)
    {
        bytes_to_write -= dest->pub.free_in_buffer;
    }
#endif

    if(VSIFWriteL(dest->buffer, 1, bytes_to_write, dest->outfile) !=
       bytes_to_write)
    {
        cinfo->err->msg_code = JERR_FILE_WRITE;
        cinfo->err->error_exit((j_common_ptr)(cinfo));
    }

    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;

    return TRUE;
}

// Terminate destination --- called by jpeg_finish_compress
// after all data has been written.  Usually needs to flush buffer.
//
// NB://not* called by jpeg_abort or jpeg_destroy; surrounding
// application must deal with any cleanup that should happen even
// for error exit.
METHODDEF(void)
term_destination(j_compress_ptr cinfo)
{
    my_dest_ptr dest = (my_dest_ptr)cinfo->dest;
    size_t datacount = OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;

    // Write any data remaining in the buffer.
    if(datacount > 0)
    {
        if(VSIFWriteL(dest->buffer, 1, datacount, dest->outfile) != datacount)
        {
            cinfo->err->msg_code = JERR_FILE_WRITE;
            cinfo->err->error_exit((j_common_ptr)(cinfo));
        }
    }
    if( VSIFFlushL(dest->outfile) != 0 )
    {
        cinfo->err->msg_code = JERR_FILE_WRITE;
        cinfo->err->error_exit((j_common_ptr)(cinfo));
    }
}

// Prepare for output to a stdio stream.
// The caller must have already opened the stream, and is responsible
// for closing it after finishing compression.

void jpeg_vsiio_dest(j_compress_ptr cinfo, VSILFILE *outfile)
{
    my_dest_ptr dest;

    // The destination object is made permanent so that multiple JPEG images
    // can be written to the same file without re-executing jpeg_stdio_dest.
    // This makes it dangerous to use this manager and a different destination
    // manager serially with the same JPEG object, because their private object
    // sizes may be different.  Caveat programmer.
    if(cinfo->dest == NULL)
    {
        // First time for this JPEG object?
        cinfo->dest = (struct jpeg_destination_mgr *)(*cinfo->mem->alloc_small)(
            (j_common_ptr)cinfo, JPOOL_PERMANENT, sizeof(my_destination_mgr));
    }

    dest = (my_dest_ptr)cinfo->dest;
    dest->pub.init_destination = init_destination;
    dest->pub.empty_output_buffer = empty_output_buffer;
    dest->pub.term_destination = term_destination;
    dest->outfile = outfile;
}

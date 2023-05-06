/**********************************************************************
 * Project:  CPL - Common Portability Library
 * Purpose:  Registry of compression/decompression functions
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 **********************************************************************
 * Copyright (c) 2021, Even Rouault <even.rouault at spatialys.com>
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

#ifndef CPL_COMPRESSOR_H_INCLUDED
#define CPL_COMPRESSOR_H_INCLUDED

#include "cpl_port.h"

#include <stdbool.h>

/**
 * \file cpl_compressor.h
 *
 * API for compressors and decompressors of binary buffers.
 */

CPL_C_START

/** Callback of a compressor/decompressor.
 *
 * For a compressor, input is uncompressed data, and output compressed data.
 * For a decompressor, input is compressed data, and output uncompressed data.
 *
 * Valid situations for output_data and output_size are:
 * <ul>
 * <li>output_data != NULL and *output_data != NULL and output_size != NULL and
 * *output_size != 0. The caller provides the output
 * buffer in *output_data and its size in *output_size. In case of successful
 * operation, *output_size will be updated to the actual size.
 * This mode is the one that is always guaranteed to be implemented efficiently.
 * In case of failure due to insufficient space, it will be updated to the size
 * needed (if known), or 0 (if unknown)</li>
 * <li>output_data == NULL and output_size != NULL. *output_size will be updated
 * with the minimum size the output buffer should be (if known), or 0 (if
 * unknown).</li> <li>output_data != NULL and *output_data == NULL and
 * output_size != NULL. *output_data will be allocated using VSIMalloc(), and
 * should be freed by the caller with VSIFree(). *output_size will be updated to
 * the size of the output buffer.</li>
 * </ul>
 *
 * @param input_data Input data. Should not be NULL.
 * @param input_size Size of input data, in bytes.
 * @param output_data Pointer to output data.
 * @param output_size Pointer to output size.
 * @param options NULL terminated list of options. Or NULL.
 * @param compressor_user_data User data provided at registration time.
 * @return true in case of success.
 */
typedef bool (*CPLCompressionFunc)(const void *input_data, size_t input_size,
                                   void **output_data, size_t *output_size,
                                   CSLConstList options,
                                   void *compressor_user_data);

/** Type of compressor */
typedef enum
{
    /** Compressor */
    CCT_COMPRESSOR,
    /** Filter */
    CCT_FILTER
} CPLCompressorType;

/** Compressor/decompressor description */
typedef struct
{
    /** Structure version. Should be set to 1 */
    int nStructVersion;
    /** Id of the compressor/decompressor. Should NOT be NULL. */
    const char *pszId;
    /** Compressor type */
    CPLCompressorType eType;
    /** Metadata, as a NULL terminated list of strings. Or NULL.
     * The OPTIONS metadata key is reserved for compressors/decompressors to
     * provide the available options as a XML string of the form
     * &lt;Options&gt;
     *   &lt;Option name='' type='' description='' default=''/&gt;
     * &lt;/Options&gt;
     */
    CSLConstList papszMetadata;
    /** Compressor/decompressor callback. Should NOT be NULL. */
    CPLCompressionFunc pfnFunc;
    /** User data to provide to the callback. May be NULL. */
    void *user_data;
} CPLCompressor;

bool CPL_DLL CPLRegisterCompressor(const CPLCompressor *compressor);

bool CPL_DLL CPLRegisterDecompressor(const CPLCompressor *decompressor);

char CPL_DLL **CPLGetCompressors(void);

char CPL_DLL **CPLGetDecompressors(void);

const CPLCompressor CPL_DLL *CPLGetCompressor(const char *pszId);

const CPLCompressor CPL_DLL *CPLGetDecompressor(const char *pszId);

/*! @cond Doxygen_Suppress */
void CPL_DLL CPLDestroyCompressorRegistry(void);
/*! @endcond */

CPL_C_END

#endif  // CPL_COMPRESSOR_H_INCLUDED

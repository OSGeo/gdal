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

#include "cpl_compressor.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_conv.h" // CPLZLibInflate()

#ifdef HAVE_BLOSC
#include <blosc.h>
#endif

#ifdef HAVE_LIBDEFLATE
#include "libdeflate.h"
#endif

#ifdef HAVE_LZMA
#include <lzma.h>
#endif

#ifdef HAVE_ZSTD
#include <zstd.h>
#endif

#ifdef HAVE_LZ4
#include <lz4.h>
#endif

#include <mutex>
#include <vector>

static std::mutex gMutex;
static std::vector<CPLCompressor*>* gpCompressors = nullptr;
static std::vector<CPLCompressor*>* gpDecompressors = nullptr;

#ifdef HAVE_BLOSC
static bool CPLBloscCompressor(const void* input_data,
                               size_t input_size,
                               void** output_data,
                               size_t* output_size,
                               CSLConstList options,
                               void* /* compressor_user_data */)
{
    if( output_data != nullptr && *output_data != nullptr &&
        output_size != nullptr && *output_size != 0 )
    {
        const int clevel = atoi(CSLFetchNameValueDef(options, "CLEVEL", "5"));
        const char* pszShuffle = CSLFetchNameValueDef(options, "SHUFFLE", "BYTE");
        const int shuffle = (EQUAL(pszShuffle, "BYTE") ||
                            EQUAL(pszShuffle, "1")) ? BLOSC_SHUFFLE :
                            (EQUAL(pszShuffle, "BIT") ||
                            EQUAL(pszShuffle, "2")) ?  BLOSC_BITSHUFFLE :
                                                        BLOSC_NOSHUFFLE;
        const int typesize = atoi(CSLFetchNameValueDef(options, "TYPESIZE", "1"));
        const char* compressor = CSLFetchNameValueDef(
                                options, "CNAME", BLOSC_LZ4_COMPNAME);
        const int blocksize = atoi(CSLFetchNameValueDef(options, "BLOCKSIZE", "0"));
        if( blocksize < 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid BLOCKSIZE");
            return false;
        }
        const char* pszNumThreads = CSLFetchNameValueDef(options, "NUM_THREADS", "1");
        const int numthreads = EQUAL(pszNumThreads, "ALL_CPUS") ?
                                        CPLGetNumCPUs(): atoi(pszNumThreads);
        int ret = blosc_compress_ctx(clevel, shuffle, typesize, input_size,
                                     input_data, *output_data, *output_size,
                                     compressor, blocksize, numthreads);
        if( ret < 0 )
        {
            *output_size = 0;
            return false;
        }
        if( ret == 0 )
        {
            *output_size = input_size + BLOSC_MAX_OVERHEAD;
            return false;
        }
        *output_size = ret;
        return true;
    }

    if( output_data == nullptr && output_size != nullptr )
    {
        *output_size = input_size + BLOSC_MAX_OVERHEAD;
        return true;
    }

    if( output_data != nullptr && *output_data == nullptr &&
        output_size != nullptr )
    {
        size_t nSafeSize = input_size + BLOSC_MAX_OVERHEAD;
        *output_data = VSI_MALLOC_VERBOSE(nSafeSize);
        *output_size = nSafeSize;
        if( *output_data == nullptr )
            return false;
        bool ret = CPLBloscCompressor(input_data, input_size,
                                      output_data, output_size,
                                      options, nullptr);
        if( !ret )
        {
            VSIFree(*output_data);
            *output_data = nullptr;
        }
        return ret;
    }

    CPLError(CE_Failure, CPLE_AppDefined, "Invalid use of API");
    return false;
}

static bool CPLBloscDecompressor(const void* input_data,
                                 size_t input_size,
                                 void** output_data,
                                 size_t* output_size,
                                 CSLConstList options,
                                 void* /* compressor_user_data */)
{
    size_t nSafeSize = 0;
    if( blosc_cbuffer_validate(input_data, input_size, &nSafeSize) < 0 )
    {
        *output_size = 0;
        return false;
    }

    if( output_data != nullptr && *output_data != nullptr &&
        output_size != nullptr && *output_size != 0 )
    {
        if( nSafeSize < *output_size )
        {
            *output_size = nSafeSize;
            return false;
        }

        const char* pszNumThreads = CSLFetchNameValueDef(options, "NUM_THREADS", "1");
        const int numthreads = EQUAL(pszNumThreads, "ALL_CPUS") ?
                                        CPLGetNumCPUs(): atoi(pszNumThreads);
        if( blosc_decompress_ctx(input_data, *output_data, *output_size,
                                 numthreads) <= 0 )
        {
            *output_size = 0;
            return false;
        }

        *output_size = nSafeSize;
        return true;
    }

    if( output_data == nullptr && output_size != nullptr )
    {
        *output_size = nSafeSize;
        return true;
    }

    if( output_data != nullptr && *output_data == nullptr &&
        output_size != nullptr )
    {
        *output_data = VSI_MALLOC_VERBOSE(nSafeSize);
        *output_size = nSafeSize;
        if( *output_data == nullptr )
            return false;
        bool ret = CPLBloscDecompressor(input_data, input_size,
                                        output_data, output_size,
                                        options, nullptr);
        if( !ret )
        {
            VSIFree(*output_data);
            *output_data = nullptr;
        }
        return ret;
    }

    CPLError(CE_Failure, CPLE_AppDefined, "Invalid use of API");
    return false;
}

#endif

#ifdef HAVE_LZMA
static bool CPLLZMACompressor (const void* input_data,
                               size_t input_size,
                               void** output_data,
                               size_t* output_size,
                               CSLConstList options,
                               void* /* compressor_user_data */)
{
    if( output_data != nullptr && *output_data != nullptr &&
        output_size != nullptr && *output_size != 0 )
    {
        const int preset = atoi(CSLFetchNameValueDef(options, "PRESET", "6"));
        const int delta = atoi(CSLFetchNameValueDef(options, "DELTA", "1"));

        lzma_filter filters[3];
        lzma_options_delta opt_delta;
        lzma_options_lzma opt_lzma;

        opt_delta.type = LZMA_DELTA_TYPE_BYTE;
        opt_delta.dist = delta;
        filters[0].id = LZMA_FILTER_DELTA;
        filters[0].options = &opt_delta;

        lzma_lzma_preset(&opt_lzma, preset);
        filters[1].id = LZMA_FILTER_LZMA2;
        filters[1].options = &opt_lzma;

        filters[2].id = LZMA_VLI_UNKNOWN;
        filters[2].options = nullptr;

        size_t out_pos = 0;
        lzma_ret ret = lzma_stream_buffer_encode(filters,
                                                 LZMA_CHECK_NONE,
                                                 nullptr, // allocator,
                                                 static_cast<const uint8_t*>(input_data),
                                                 input_size,
                                                 static_cast<uint8_t*>(*output_data),
                                                 &out_pos,
                                                 *output_size);
        if( ret != LZMA_OK )
        {
            *output_size = 0;
            return false;
        }
        *output_size = out_pos;
        return true;
    }

    if( output_data == nullptr && output_size != nullptr )
    {
        *output_size = lzma_stream_buffer_bound(input_size);
        return true;
    }

    if( output_data != nullptr && *output_data == nullptr &&
        output_size != nullptr )
    {
        size_t nSafeSize = lzma_stream_buffer_bound(input_size);
        *output_data = VSI_MALLOC_VERBOSE(nSafeSize);
        *output_size = nSafeSize;
        if( *output_data == nullptr )
            return false;
        bool ret = CPLLZMACompressor(input_data, input_size,
                                     output_data, output_size,
                                     options, nullptr);
        if( !ret )
        {
            VSIFree(*output_data);
            *output_data = nullptr;
        }
        return ret;
    }

    CPLError(CE_Failure, CPLE_AppDefined, "Invalid use of API");
    return false;
}

static bool CPLLZMADecompressor (const void* input_data,
                                 size_t input_size,
                                 void** output_data,
                                 size_t* output_size,
                                 CSLConstList options,
                                 void* /* compressor_user_data */)
{
    if( output_data != nullptr && *output_data != nullptr &&
        output_size != nullptr && *output_size != 0 )
    {
        size_t in_pos = 0;
        size_t out_pos = 0;
        uint64_t memlimit = 100 * 1024 * 1024;
        lzma_ret ret = lzma_stream_buffer_decode(&memlimit,
                                                 0, // flags
                                                 nullptr, // allocator,
                                                 static_cast<const uint8_t*>(input_data),
                                                 &in_pos,
                                                 input_size,
                                                 static_cast<uint8_t*>(*output_data),
                                                 &out_pos,
                                                 *output_size);
        if( ret != LZMA_OK )
        {
            *output_size = 0;
            return false;
        }
        *output_size = out_pos;
        return true;
    }

    if( output_data == nullptr && output_size != nullptr )
    {
        // inefficient !
        void* tmpBuffer = nullptr;
        bool ret = CPLLZMADecompressor(input_data, input_size, &tmpBuffer, output_size,
                                       options, nullptr);
        VSIFree(tmpBuffer);
        return ret;
    }

    if( output_data != nullptr && *output_data == nullptr &&
        output_size != nullptr )
    {
        size_t nOutSize = input_size < std::numeric_limits<size_t>::max() / 2 ? input_size * 2 : input_size;
        *output_data = VSI_MALLOC_VERBOSE(nOutSize);
        if( *output_data == nullptr )
        {
            *output_size = 0;
            return false;
        }

        while( true )
        {
            size_t in_pos = 0;
            size_t out_pos = 0;
            uint64_t memlimit = 100 * 1024 * 1024;
            lzma_ret ret = lzma_stream_buffer_decode(&memlimit,
                                                     0, // flags
                                                     nullptr, // allocator,
                                                     static_cast<const uint8_t*>(input_data),
                                                     &in_pos,
                                                     input_size,
                                                     static_cast<uint8_t*>(*output_data),
                                                     &out_pos,
                                                     nOutSize);
            if( ret == LZMA_OK )
            {
                *output_size = out_pos;
                return true;
            }
            else if( ret == LZMA_BUF_ERROR &&
                     nOutSize < std::numeric_limits<size_t>::max() / 2 )
            {
                nOutSize *= 2;
                void* tmpBuffer = VSI_REALLOC_VERBOSE(*output_data, nOutSize);
                if( tmpBuffer == nullptr )
                {
                    VSIFree(*output_data);
                    *output_data = nullptr;
                    *output_size = 0;
                    return false;
                }
                *output_data = tmpBuffer;
            }
            else
            {
                VSIFree(*output_data);
                *output_data = nullptr;
                *output_size = 0;
                return false;
            }
        }
    }

    CPLError(CE_Failure, CPLE_AppDefined, "Invalid use of API");
    return false;
}

#endif // HAVE_LZMA

#ifdef HAVE_ZSTD
static bool CPLZSTDCompressor (const void* input_data,
                               size_t input_size,
                               void** output_data,
                               size_t* output_size,
                               CSLConstList options,
                               void* /* compressor_user_data */)
{
    if( output_data != nullptr && *output_data != nullptr &&
        output_size != nullptr && *output_size != 0 )
    {
        const int level = atoi(CSLFetchNameValueDef(options, "LEVEL", "13"));
        ZSTD_CCtx* ctx = ZSTD_createCCtx();
        if( ctx == nullptr )
        {
            *output_size = 0;
            return false;
        }

        size_t ret = ZSTD_compressCCtx(ctx, *output_data, *output_size,
                                       input_data, input_size,
                                       level);
        if( ZSTD_isError(ret) )
        {
            *output_size = 0;
            return false;
        }

        *output_size = ret;
        return true;
    }

    if( output_data == nullptr && output_size != nullptr )
    {
        *output_size = ZSTD_compressBound(input_size);
        return true;
    }

    if( output_data != nullptr && *output_data == nullptr &&
        output_size != nullptr )
    {
        size_t nSafeSize = ZSTD_compressBound(input_size);
        *output_data = VSI_MALLOC_VERBOSE(nSafeSize);
        *output_size = nSafeSize;
        if( *output_data == nullptr )
            return false;
        bool ret = CPLZSTDCompressor(input_data, input_size,
                                     output_data, output_size,
                                     options, nullptr);
        if( !ret )
        {
            VSIFree(*output_data);
            *output_data = nullptr;
        }
        return ret;
    }

    CPLError(CE_Failure, CPLE_AppDefined, "Invalid use of API");
    return false;
}

static bool CPLZSTDDecompressor (const void* input_data,
                                 size_t input_size,
                                 void** output_data,
                                 size_t* output_size,
                                 CSLConstList /* options */,
                                 void* /* compressor_user_data */)
{
    if( output_data != nullptr && *output_data != nullptr &&
        output_size != nullptr && *output_size != 0 )
    {
        size_t ret = ZSTD_decompress(*output_data, *output_size,
                                     input_data, input_size);
        if( ZSTD_isError(ret) )
        {
            *output_size = static_cast<size_t>(ZSTD_getDecompressedSize(input_data, input_size));
            return false;
        }

        *output_size = ret;
        return true;
    }

    if( output_data == nullptr && output_size != nullptr )
    {
        *output_size = static_cast<size_t>(ZSTD_getDecompressedSize(input_data, input_size));
        return *output_size != 0;
    }

    if( output_data != nullptr && *output_data == nullptr &&
        output_size != nullptr )
    {
        size_t nOutSize = static_cast<size_t>(ZSTD_getDecompressedSize(input_data, input_size));
        *output_data = VSI_MALLOC_VERBOSE(nOutSize);
        if( *output_data == nullptr )
        {
            *output_size = 0;
            return false;
        }

        size_t ret = ZSTD_decompress(*output_data, nOutSize,
                                     input_data, input_size);
        if( ZSTD_isError(ret) )
        {
            *output_size = 0;
            VSIFree(*output_data);
            *output_data = nullptr;
            return false;
        }

        *output_size = ret;
        return true;
    }

    CPLError(CE_Failure, CPLE_AppDefined, "Invalid use of API");
    return false;
}

#endif // HAVE_ZSTD

#ifdef HAVE_LZ4
static bool CPLLZ4Compressor  (const void* input_data,
                               size_t input_size,
                               void** output_data,
                               size_t* output_size,
                               CSLConstList options,
                               void* /* compressor_user_data */)
{
    if( input_size > static_cast<size_t>(std::numeric_limits<int>::max()) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Too large input buffer. "
                 "Max supported is INT_MAX");
        *output_size = 0;
        return false;
    }

    const bool bHeader = CPLTestBool(CSLFetchNameValueDef(options, "HEADER", "YES"));
    const int header_size = bHeader ? static_cast<int>(sizeof(int32_t)) : 0;

    if( output_data != nullptr && *output_data != nullptr &&
        output_size != nullptr && *output_size != 0 )
    {
        const int acceleration = atoi(CSLFetchNameValueDef(options, "ACCELERATION", "1"));
        if( *output_size > static_cast<size_t>(std::numeric_limits<int>::max() - 4) )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Too large output buffer. "
                     "Max supported is INT_MAX");
            *output_size = 0;
            return false;
        }

        if( bHeader && static_cast<int>(*output_size) < header_size )
        {
            *output_size = 0;
            return false;
        }

        int ret = LZ4_compress_fast(static_cast<const char*>(input_data),
                                    static_cast<char*>(*output_data) + header_size,
                                    static_cast<int>(input_size),
                                    static_cast<int>(*output_size) - header_size,
                                    acceleration);
        if( ret <= 0 || ret > std::numeric_limits<int>::max() - header_size )
        {
            *output_size = 0;
            return false;
        }

        int32_t sizeLSB = CPL_LSBWORD32(static_cast<int>(input_size));
        memcpy(*output_data, &sizeLSB, sizeof(sizeLSB));

        *output_size = static_cast<size_t>(header_size + ret);
        return true;
    }

    if( output_data == nullptr && output_size != nullptr )
    {
        *output_size = static_cast<size_t>(header_size) +
                        LZ4_compressBound(static_cast<int>(input_size));
        return true;
    }

    if( output_data != nullptr && *output_data == nullptr &&
        output_size != nullptr )
    {
        size_t nSafeSize = static_cast<size_t>(header_size) +
                        LZ4_compressBound(static_cast<int>(input_size));
        *output_data = VSI_MALLOC_VERBOSE(nSafeSize);
        *output_size = nSafeSize;
        if( *output_data == nullptr )
            return false;
        bool ret = CPLLZ4Compressor(input_data, input_size,
                                    output_data, output_size,
                                    options, nullptr);
        if( !ret )
        {
            VSIFree(*output_data);
            *output_data = nullptr;
        }
        return ret;
    }

    CPLError(CE_Failure, CPLE_AppDefined, "Invalid use of API");
    return false;
}

static bool CPLLZ4Decompressor  (const void* input_data,
                                 size_t input_size,
                                 void** output_data,
                                 size_t* output_size,
                                 CSLConstList options,
                                 void* /* compressor_user_data */)
{
    if( input_size > static_cast<size_t>(std::numeric_limits<int>::max()) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Too large input buffer. "
                 "Max supported is INT_MAX");
        *output_size = 0;
        return false;
    }

    const bool bHeader = CPLTestBool(CSLFetchNameValueDef(options, "HEADER", "YES"));
    const int header_size = bHeader ? static_cast<int>(sizeof(int32_t)) : 0;
    if( bHeader && static_cast<int>(input_size) < header_size )
    {
        *output_size = 0;
        return false;
    }

    if( output_data != nullptr && *output_data != nullptr &&
        output_size != nullptr && *output_size != 0 )
    {
        if( *output_size > static_cast<size_t>(std::numeric_limits<int>::max()) )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Too large output buffer. "
                     "Max supported is INT_MAX");
            *output_size = 0;
            return false;
        }

        int ret = LZ4_decompress_safe(static_cast<const char*>(input_data) + header_size,
                                      static_cast<char*>(*output_data),
                                      static_cast<int>(input_size) - header_size,
                                      static_cast<int>(*output_size));
        if( ret <= 0 )
        {
            *output_size = 0;
            return false;
        }

        *output_size = ret;
        return true;
    }

    if( output_data == nullptr && output_size != nullptr )
    {
        if( bHeader )
        {
            int nSize = CPL_LSBSINT32PTR(input_data);
            if( nSize < 0 )
            {
                *output_size = 0;
                return false;
            }
            *output_size = nSize;
            return true;
        }

        // inefficient !
        void* tmpBuffer = nullptr;
        bool ret = CPLLZ4Decompressor(input_data, input_size,
                                      &tmpBuffer, output_size,
                                      options, nullptr);
        VSIFree(tmpBuffer);
        return ret;
    }

    if( output_data != nullptr && *output_data == nullptr &&
        output_size != nullptr )
    {
        if( bHeader )
        {
            int nSize = CPL_LSBSINT32PTR(input_data);
            if( nSize < 0 )
            {
                *output_size = 0;
                return false;
            }
            *output_data = VSI_MALLOC_VERBOSE(nSize);
            *output_size = nSize;
            if( *output_data == nullptr )
            {
                return false;
            }
            if( !CPLLZ4Decompressor(input_data, input_size,
                                    output_data, output_size,
                                    options, nullptr) )
            {
                VSIFree(*output_data);
                *output_data = nullptr;
                *output_size = 0;
                return false;
            }
            return true;
        }

        size_t nOutSize = static_cast<int>(input_size) < std::numeric_limits<int>::max() / 2 ?
            input_size * 2 :
            static_cast<size_t>(std::numeric_limits<int>::max());
        *output_data = VSI_MALLOC_VERBOSE(nOutSize);
        if( *output_data == nullptr )
        {
            *output_size = 0;
            return false;
        }

        while( true )
        {
            int ret = LZ4_decompress_safe_partial (
                static_cast<const char*>(input_data),
                static_cast<char*>(*output_data),
                static_cast<int>(input_size),
                static_cast<int>(nOutSize),
                static_cast<int>(nOutSize));
            if( ret <= 0 )
            {
                VSIFree(*output_data);
                *output_data = nullptr;
                *output_size = 0;
                return false;
            }
            else if( ret < static_cast<int>(nOutSize) )
            {
                *output_size = ret;
                return true;
            }
            else if( static_cast<int>(nOutSize) < std::numeric_limits<int>::max() / 2)
            {
                nOutSize *= 2;
                void* tmpBuffer = VSI_REALLOC_VERBOSE(*output_data, nOutSize);
                if( tmpBuffer == nullptr )
                {
                    VSIFree(*output_data);
                    *output_data = nullptr;
                    *output_size = 0;
                    return false;
                }
                *output_data = tmpBuffer;
            }
            else
            {
                VSIFree(*output_data);
                *output_data = nullptr;
                *output_size = 0;
                return false;
            }
        }
    }

    CPLError(CE_Failure, CPLE_AppDefined, "Invalid use of API");
    return false;
}

#endif // HAVE_LZ4

static bool CPLZlibCompressor(const void* input_data,
                                 size_t input_size,
                                 void** output_data,
                                 size_t* output_size,
                                 CSLConstList options,
                                 void* /* compressor_user_data */)
{
    const int clevel = atoi(CSLFetchNameValueDef(options, "LEVEL",
#if HAVE_LIBDEFLATE
                                                 "7"
#else
                                                 "6"
#endif
                                                ));

    if( output_data != nullptr && *output_data != nullptr &&
        output_size != nullptr && *output_size != 0 )
    {
        size_t nOutBytes = 0;
        if( nullptr == CPLZLibDeflate( input_data, input_size,
                                       clevel,
                                       *output_data, *output_size,
                                       &nOutBytes ) )
        {
            *output_size = 0;
            return false;
        }

        *output_size = nOutBytes;
        return true;
    }

    if( output_data == nullptr && output_size != nullptr )
    {
#if HAVE_LIBDEFLATE
        struct libdeflate_compressor* enc = libdeflate_alloc_compressor(clevel);
        if( enc == nullptr )
        {
            *output_size = 0;
            return false;
        }
        *output_size = libdeflate_zlib_compress_bound(enc, input_size);
        libdeflate_free_compressor(enc);
#else
        // Really inefficient !
        size_t nOutSize = 0;
        void* outbuffer = CPLZLibDeflate( input_data, input_size,
                                         clevel,
                                         nullptr, 0,
                                         &nOutSize );
        if( outbuffer == nullptr )
        {
            *output_size = 0;
            return false;
        }
        VSIFree(outbuffer);
        *output_size = nOutSize;
#endif
        return true;
    }

    if( output_data != nullptr && *output_data == nullptr &&
        output_size != nullptr )
    {
        size_t nOutSize = 0;
        *output_data = CPLZLibDeflate( input_data, input_size,
                                       clevel,
                                       nullptr, 0,
                                       &nOutSize );
        if( *output_data == nullptr )
        {
            *output_size = 0;
            return false;
        }
        *output_size = nOutSize;
        return true;
    }

    CPLError(CE_Failure, CPLE_AppDefined, "Invalid use of API");
    return false;
}

static void CPLAddCompressor(const CPLCompressor* compressor)
{
    CPLCompressor* copy = new CPLCompressor();
    copy->nStructVersion = 1;
    copy->pszId = CPLStrdup(compressor->pszId);
    copy->papszMetadata = CSLDuplicate(compressor->papszMetadata);
    copy->pfnFunc = compressor->pfnFunc;
    copy->user_data = compressor->user_data;
    gpCompressors->emplace_back(copy);
}

static void CPLAddBuiltinCompressors()
{
#ifdef HAVE_BLOSC
    do {
        CPLCompressor sComp;
        sComp.nStructVersion = 1;
        sComp.pszId = "blosc";

        const CPLStringList aosCompressors(CSLTokenizeString2(blosc_list_compressors(), ",", 0));
        if( aosCompressors.size() == 0 )
            break;
        std::string options("OPTIONS=<Options>"
            "  <Option name='CNAME' type='string-select' description='Compressor name' default='");
        std::string values;
        std::string defaultCompressor;
        bool bFoundLZ4 = false;
        bool bFoundSnappy = false;
        bool bFoundZlib = false;
        for( int i = 0; i < aosCompressors.size(); i++ )
        {
            values += "<Value>";
            values += aosCompressors[i];
            values += "</Value>";
            if( strcmp(aosCompressors[i], "lz4") == 0 )
                bFoundLZ4 = true;
            else if( strcmp(aosCompressors[i], "snappy") == 0 )
                bFoundSnappy = true;
            else if( strcmp(aosCompressors[i], "zlib") == 0 )
                bFoundZlib = true;
        }
        options += bFoundLZ4 ? "lz4" : bFoundSnappy ? "snappy" : bFoundZlib ? "zlib" : aosCompressors[0];
        options += "'>";
        options += values;
        options +=
            "  </Option>"
            "  <Option name='CLEVEL' type='int' description='Compression level' min='1' max='9' default='5' />"
            "  <Option name='SHUFFLE' type='string-select' description='Type of shuffle algorithm' default='BYTE'>"
            "    <Value alias='0'>NONE</Value>"
            "    <Value alias='1'>BYTE</Value>"
            "    <Value alias='2'>BIT</Value>"
            "  </Option>"
            "  <Option name='BLOCKSIZE' type='int' description='Block size' default='0' />"
            "  <Option name='TYPESIZE' type='int' description='Number of bytes for the atomic type' default='1' />"
            "  <Option name='NUM_THREADS' type='string' "
            "description='Number of worker threads for compression. Can be set to ALL_CPUS' default='1' />"
            "</Options>";

        const char* const apszMetadata[] = {
            "BLOSC_VERSION=" BLOSC_VERSION_STRING,
            options.c_str(),
            nullptr
        };
        sComp.papszMetadata = apszMetadata;
        sComp.pfnFunc = CPLBloscCompressor;
        sComp.user_data = nullptr;
        CPLAddCompressor(&sComp);
    } while(0);
#endif
    {
        CPLCompressor sComp;
        sComp.nStructVersion = 1;
        sComp.pszId = "zlib";
        const char* const apszMetadata[] = {
            "OPTIONS=<Options>"
            "  <Option name='LEVEL' type='int' description='Compression level' min='1' max='9' default='6' />"
            "</Options>",
            nullptr
        };
        sComp.papszMetadata = apszMetadata;
        sComp.pfnFunc = CPLZlibCompressor;
        sComp.user_data = nullptr;
        CPLAddCompressor(&sComp);
    }
#ifdef HAVE_LZMA
    {
        CPLCompressor sComp;
        sComp.nStructVersion = 1;
        sComp.pszId = "lzma";
        const char* const apszMetadata[] = {
            "OPTIONS=<Options>"
            "  <Option name='PRESET' type='int' description='Compression level' min='0' max='9' default='6' />"
            "  <Option name='DELTA' type='int' description='Delta distance in byte' default='1' />"
            "</Options>",
            nullptr
        };
        sComp.papszMetadata = apszMetadata;
        sComp.pfnFunc = CPLLZMACompressor;
        sComp.user_data = nullptr;
        CPLAddCompressor(&sComp);
    }
#endif
#ifdef HAVE_ZSTD
    {
        CPLCompressor sComp;
        sComp.nStructVersion = 1;
        sComp.pszId = "zstd";
        const char* const apszMetadata[] = {
            "OPTIONS=<Options>"
            "  <Option name='LEVEL' type='int' description='Compression level' min='1' max='22' default='13' />"
            "</Options>",
            nullptr
        };
        sComp.papszMetadata = apszMetadata;
        sComp.pfnFunc = CPLZSTDCompressor;
        sComp.user_data = nullptr;
        CPLAddCompressor(&sComp);
    }
#endif
#ifdef HAVE_LZ4
    {
        CPLCompressor sComp;
        sComp.nStructVersion = 1;
        sComp.pszId = "lz4";
        const char* const apszMetadata[] = {
            "OPTIONS=<Options>"
            "  <Option name='ACCELERATION' type='int' description='Acceleration factor. The higher, the less compressed' min='1' default='1' />"
            "  <Option name='HEADER' type='boolean' description='Whether a header with the uncompressed size should be included (as used by Zarr)' default='YES' />"
            "</Options>",
            nullptr
        };
        sComp.papszMetadata = apszMetadata;
        sComp.pfnFunc = CPLLZ4Compressor;
        sComp.user_data = nullptr;
        CPLAddCompressor(&sComp);
    }
#endif
}


static bool CPLZlibDecompressor(const void* input_data,
                                 size_t input_size,
                                 void** output_data,
                                 size_t* output_size,
                                 CSLConstList /* options */,
                                 void* /* compressor_user_data */)
{
    if( output_data != nullptr && *output_data != nullptr &&
        output_size != nullptr && *output_size != 0 )
    {
        size_t nOutBytes = 0;
        if( nullptr == CPLZLibInflate( input_data, input_size,
                                       *output_data, *output_size,
                                       &nOutBytes) )
        {
            *output_size = 0;
            return false;
        }

        *output_size = nOutBytes;
        return true;
    }

    if( output_data == nullptr && output_size != nullptr )
    {
        size_t nOutSize = input_size < std::numeric_limits<size_t>::max() / 4 ? input_size * 4 : input_size;
        void* tmpOutBuffer = VSIMalloc(nOutSize);
        if( tmpOutBuffer == nullptr )
        {
            *output_size = 0;
            return false;
        }
        if( nullptr == CPLZLibInflate( input_data, input_size,
                                       tmpOutBuffer, nOutSize,
                                       &nOutSize) )
        {
            VSIFree(tmpOutBuffer);
            *output_size = 0;
            return false;
        }
        VSIFree(tmpOutBuffer);
        *output_size = nOutSize;
        return true;
    }

    if( output_data != nullptr && *output_data == nullptr &&
        output_size != nullptr )
    {
        size_t nOutSize = input_size < std::numeric_limits<size_t>::max() / 4 ? input_size * 4 : input_size;
        void* tmpOutBuffer = VSIMalloc(nOutSize);
        if( tmpOutBuffer == nullptr )
        {
            *output_size = 0;
            return false;
        }
        size_t nOutSizeOut = 0;
        if( nullptr == CPLZLibInflate( input_data, input_size,
                                       tmpOutBuffer, nOutSize,
                                       &nOutSizeOut) )
        {
            VSIFree(tmpOutBuffer);
            *output_size = 0;
            return false;
        }
        *output_data = VSIRealloc(tmpOutBuffer, nOutSizeOut); // cannot fail
        *output_size = nOutSizeOut;
        return true;
    }

    CPLError(CE_Failure, CPLE_AppDefined, "Invalid use of API");
    return false;
}


static void CPLAddDecompressor(const CPLCompressor* decompressor)
{
    CPLCompressor* copy = new CPLCompressor();
    copy->nStructVersion = 1;
    copy->pszId = CPLStrdup(decompressor->pszId);
    copy->papszMetadata = CSLDuplicate(decompressor->papszMetadata);
    copy->pfnFunc = decompressor->pfnFunc;
    copy->user_data = decompressor->user_data;
    gpDecompressors->emplace_back(copy);
}

static void CPLAddBuiltinDecompressors()
{
#ifdef HAVE_BLOSC
    {
        CPLCompressor sComp;
        sComp.nStructVersion = 1;
        sComp.pszId = "blosc";
        const char* const apszMetadata[] = {
            "BLOSC_VERSION=" BLOSC_VERSION_STRING,
            "OPTIONS=<Options>"
            "  <Option name='NUM_THREADS' type='string' "
            "description='Number of worker threads for decompression. Can be set to ALL_CPUS' default='1' />"
            "</Options>",
            nullptr
        };
        sComp.papszMetadata = apszMetadata;
        sComp.pfnFunc = CPLBloscDecompressor;
        sComp.user_data = nullptr;
        CPLAddDecompressor(&sComp);
    }
#endif
    {
        CPLCompressor sComp;
        sComp.nStructVersion = 1;
        sComp.pszId = "zlib";
        sComp.papszMetadata = nullptr;
        sComp.pfnFunc = CPLZlibDecompressor;
        sComp.user_data = nullptr;
        CPLAddDecompressor(&sComp);
    }
#ifdef HAVE_LZMA
    {
        CPLCompressor sComp;
        sComp.nStructVersion = 1;
        sComp.pszId = "lzma";
        sComp.papszMetadata = nullptr;
        sComp.pfnFunc = CPLLZMADecompressor;
        sComp.user_data = nullptr;
        CPLAddDecompressor(&sComp);
    }
#endif
#ifdef HAVE_ZSTD
    {
        CPLCompressor sComp;
        sComp.nStructVersion = 1;
        sComp.pszId = "zstd";
        sComp.papszMetadata = nullptr;
        sComp.pfnFunc = CPLZSTDDecompressor;
        sComp.user_data = nullptr;
        CPLAddDecompressor(&sComp);
    }
#endif
#ifdef HAVE_LZ4
    {
        CPLCompressor sComp;
        sComp.nStructVersion = 1;
        sComp.pszId = "lz4";
        const char* const apszMetadata[] = {
            "OPTIONS=<Options>"
            "  <Option name='HEADER' type='boolean' description='Whether a header with the uncompressed size should be included (as used by Zarr)' default='YES' />"
            "</Options>",
            nullptr
        };
        sComp.papszMetadata = apszMetadata;
        sComp.pfnFunc = CPLLZ4Decompressor;
        sComp.user_data = nullptr;
        CPLAddDecompressor(&sComp);
    }
#endif
}


/** Register a new compressor.
 *
 * The provided structure is copied. Its pfnFunc and user_data members should
 * remain valid beyond this call however.
 *
 * @param compressor Compressor structure. Should not be null.
 * @return true if successful
 * @since GDAL 3.4
 */
bool CPLRegisterCompressor(const CPLCompressor* compressor)
{
    if( compressor->nStructVersion < 1 )
        return false;
    std::lock_guard<std::mutex> lock(gMutex);
    if( gpCompressors == nullptr )
    {
        gpCompressors = new std::vector<CPLCompressor*>();
        CPLAddBuiltinCompressors();
    }
    for( size_t i = 0; i < gpCompressors->size(); ++i )
    {
        if( strcmp(compressor->pszId, (*gpCompressors)[i]->pszId) == 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Compressor %s already registered", compressor->pszId);
            return false;
        }
    }
    CPLAddCompressor(compressor);
    return true;
}

/** Register a new decompressor.
 *
 * The provided structure is copied. Its pfnFunc and user_data members should
 * remain valid beyond this call however.
 *
 * @param decompressor Compressor structure. Should not be null.
 * @return true if successful
 * @since GDAL 3.4
 */
bool CPLRegisterDecompressor(const CPLCompressor* decompressor)
{
    if( decompressor->nStructVersion < 1 )
        return false;
    std::lock_guard<std::mutex> lock(gMutex);
    if( gpDecompressors == nullptr )
    {
        gpDecompressors = new std::vector<CPLCompressor*>();
        CPLAddBuiltinDecompressors();
    }
    for( size_t i = 0; i < gpDecompressors->size(); ++i )
    {
        if( strcmp(decompressor->pszId, (*gpDecompressors)[i]->pszId) == 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Decompressor %s already registered", decompressor->pszId);
            return false;
        }
    }
    CPLAddDecompressor(decompressor);
    return true;
}


/** Return the list of registered compressors.
 *
 * @return list of strings. Should be freed with CSLDestroy()
 * @since GDAL 3.4
 */
char ** CPLGetCompressors(void)
{
    std::lock_guard<std::mutex> lock(gMutex);
    if( gpCompressors == nullptr )
    {
        gpCompressors = new std::vector<CPLCompressor*>();
        CPLAddBuiltinCompressors();
    }
    char** papszRet = nullptr;
    for( size_t i = 0; i < gpCompressors->size(); ++i )
    {
        papszRet = CSLAddString(papszRet, (*gpCompressors)[i]->pszId);
    }
    return papszRet;
}


/** Return the list of registered decompressors.
 *
 * @return list of strings. Should be freed with CSLDestroy()
 * @since GDAL 3.4
 */
char ** CPLGetDecompressors(void)
{
    std::lock_guard<std::mutex> lock(gMutex);
    if( gpDecompressors == nullptr )
    {
        gpDecompressors = new std::vector<CPLCompressor*>();
        CPLAddBuiltinDecompressors();
    }
    char** papszRet = nullptr;
    for( size_t i = 0; gpDecompressors != nullptr && i < gpDecompressors->size(); ++i )
    {
        papszRet = CSLAddString(papszRet, (*gpDecompressors)[i]->pszId);
    }
    return papszRet;
}


/** Return a compressor.
 *
 * @param pszId Compressor id. Should NOT be NULL.
 * @return compressor structure, or NULL.
 * @since GDAL 3.4
 */
const CPLCompressor *CPLGetCompressor(const char* pszId)
{
    std::lock_guard<std::mutex> lock(gMutex);
    if( gpCompressors == nullptr )
    {
        gpCompressors = new std::vector<CPLCompressor*>();
        CPLAddBuiltinCompressors();
    }
    for( size_t i = 0; i < gpCompressors->size(); ++i )
    {
        if( EQUAL(pszId, (*gpCompressors)[i]->pszId) )
        {
            return (*gpCompressors)[i];
        }
    }
    return nullptr;
}



/** Return a decompressor.
 *
 * @param pszId Decompressor id. Should NOT be NULL.
 * @return compressor structure, or NULL.
 * @since GDAL 3.4
 */
const CPLCompressor *CPLGetDecompressor(const char* pszId)
{
    std::lock_guard<std::mutex> lock(gMutex);
    if( gpDecompressors == nullptr )
    {
        gpDecompressors = new std::vector<CPLCompressor*>();
        CPLAddBuiltinDecompressors();
    }
    for( size_t i = 0; i < gpDecompressors->size(); ++i )
    {
        if( EQUAL(pszId, (*gpDecompressors)[i]->pszId) )
        {
            return (*gpDecompressors)[i];
        }
    }
    return nullptr;
}


static void CPLDestroyCompressorRegistryInternal(std::vector<CPLCompressor*>*& v)
{
    for( size_t i = 0; v != nullptr && i < v->size(); ++i )
    {
        CPLFree(const_cast<char*>((*v)[i]->pszId));
        CSLDestroy(const_cast<char**>((*v)[i]->papszMetadata));
        delete (*v)[i];
    }
    delete v;
    v = nullptr;
}

/*! @cond Doxygen_Suppress */
void CPLDestroyCompressorRegistry(void)
{
    std::lock_guard<std::mutex> lock(gMutex);

    CPLDestroyCompressorRegistryInternal(gpCompressors);
    CPLDestroyCompressorRegistryInternal(gpDecompressors);
}
/*! @endcond */

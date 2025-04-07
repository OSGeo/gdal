/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "zarr.h"

#include "cpl_conv.h"

#include <limits>

/************************************************************************/
/*                       ZarrShuffleCompressor()                        */
/************************************************************************/

static bool ZarrShuffleCompressor(const void *input_data, size_t input_size,
                                  void **output_data, size_t *output_size,
                                  CSLConstList options,
                                  void * /* compressor_user_data */)
{
    // 4 is the default of the shuffle numcodecs:
    // https://numcodecs.readthedocs.io/en/v0.10.0/shuffle.html
    const int eltSize = atoi(CSLFetchNameValueDef(options, "ELEMENTSIZE", "4"));
    if (eltSize != 1 && eltSize != 2 && eltSize != 4 && eltSize != 8)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Only ELEMENTSIZE=1,2,4,8 is supported");
        if (output_size)
            *output_size = 0;
        return false;
    }
    if ((input_size % eltSize) != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "input_size should be a multiple of ELEMENTSIZE");
        if (output_size)
            *output_size = 0;
        return false;
    }
    if (output_data != nullptr && *output_data != nullptr &&
        output_size != nullptr && *output_size != 0)
    {
        if (*output_size < input_size)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Too small output size");
            *output_size = input_size;
            return false;
        }

        const size_t nElts = input_size / eltSize;
        // Put at the front of the output buffer all the least significant
        // bytes of each word, then then 2nd least significant byte, etc.
        for (size_t i = 0; i < nElts; ++i)
        {
            for (int j = 0; j < eltSize; j++)
            {
                (static_cast<uint8_t *>(*output_data))[j * nElts + i] =
                    (static_cast<const uint8_t *>(input_data))[i * eltSize + j];
            }
        }

        *output_size = input_size;
        return true;
    }

#ifdef not_needed
    if (output_data == nullptr && output_size != nullptr)
    {
        *output_size = input_size;
        return true;
    }

    if (output_data != nullptr && *output_data == nullptr &&
        output_size != nullptr)
    {
        *output_data = VSI_MALLOC_VERBOSE(input_size);
        *output_size = input_size;
        if (*output_data == nullptr)
            return false;
        bool ret = ZarrShuffleCompressor(input_data, input_size, output_data,
                                         output_size, options, nullptr);
        if (!ret)
        {
            VSIFree(*output_data);
            *output_data = nullptr;
        }
        return ret;
    }
#endif

    CPLError(CE_Failure, CPLE_AppDefined, "Invalid use of API");
    return false;
}

/************************************************************************/
/*                       ZarrShuffleDecompressor()                      */
/************************************************************************/

static bool ZarrShuffleDecompressor(const void *input_data, size_t input_size,
                                    void **output_data, size_t *output_size,
                                    CSLConstList options,
                                    void * /* compressor_user_data */)
{
    // 4 is the default of the shuffle numcodecs:
    // https://numcodecs.readthedocs.io/en/v0.10.0/shuffle.html
    const int eltSize = atoi(CSLFetchNameValueDef(options, "ELEMENTSIZE", "4"));
    if (eltSize != 1 && eltSize != 2 && eltSize != 4 && eltSize != 8)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Only ELEMENTSIZE=1,2,4,8 is supported");
        if (output_size)
            *output_size = 0;
        return false;
    }
    if ((input_size % eltSize) != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "input_size should be a multiple of ELEMENTSIZE");
        if (output_size)
            *output_size = 0;
        return false;
    }
    if (output_data != nullptr && *output_data != nullptr &&
        output_size != nullptr && *output_size != 0)
    {
        if (*output_size < input_size)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Too small output size");
            *output_size = input_size;
            return false;
        }

        // Reverse of what is done in the compressor function.
        const size_t nElts = input_size / eltSize;
        for (size_t i = 0; i < nElts; ++i)
        {
            for (int j = 0; j < eltSize; j++)
            {
                (static_cast<uint8_t *>(*output_data))[i * eltSize + j] =
                    (static_cast<const uint8_t *>(input_data))[j * nElts + i];
            }
        }

        *output_size = input_size;
        return true;
    }

#ifdef not_needed
    if (output_data == nullptr && output_size != nullptr)
    {
        *output_size = input_size;
        return true;
    }

    if (output_data != nullptr && *output_data == nullptr &&
        output_size != nullptr)
    {
        *output_data = VSI_MALLOC_VERBOSE(input_size);
        *output_size = input_size;
        if (*output_data == nullptr)
            return false;
        bool ret = ZarrShuffleDecompressor(input_data, input_size, output_data,
                                           output_size, options, nullptr);
        if (!ret)
        {
            VSIFree(*output_data);
            *output_data = nullptr;
        }
        return ret;
    }
#endif

    CPLError(CE_Failure, CPLE_AppDefined, "Invalid use of API");
    return false;
}

/************************************************************************/
/*                       ZarrGetShuffleCompressor()                     */
/************************************************************************/

const CPLCompressor *ZarrGetShuffleCompressor()
{
    static const CPLCompressor gShuffleCompressor = {
        /* nStructVersion = */ 1,
        /* pszId = */ "shuffle",
        CCT_FILTER,
        /* papszMetadata = */ nullptr,
        ZarrShuffleCompressor,
        /* user_data = */ nullptr};

    return &gShuffleCompressor;
}

/************************************************************************/
/*                     ZarrGetShuffleDecompressor()                     */
/************************************************************************/

const CPLCompressor *ZarrGetShuffleDecompressor()
{
    static const CPLCompressor gShuffleDecompressor = {
        /* nStructVersion = */ 1,
        /* pszId = */ "shuffle",
        CCT_FILTER,
        /* papszMetadata = */ nullptr,
        ZarrShuffleDecompressor,
        /* user_data = */ nullptr};

    return &gShuffleDecompressor;
}

/************************************************************************/
/*                       ZarrQuantizeDecompressor()                     */
/************************************************************************/

static bool ZarrQuantizeDecompressor(const void *input_data, size_t input_size,
                                     void **output_data, size_t *output_size,
                                     CSLConstList options,
                                     void * /* compressor_user_data */)
{
    const char *dtype = CSLFetchNameValue(options, "DTYPE");
    if (!dtype)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "quantize: DTYPE missing");
        if (output_size)
            *output_size = 0;
        return false;
    }
    if (!EQUAL(dtype, "<f4") && !EQUAL(dtype, "<f8"))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "quantize: Only DTYPE=<f4 or <f8 is supported. Not %s.",
                 dtype);
        if (output_size)
            *output_size = 0;
        return false;
    }

    const int outputEltSize = EQUAL(dtype, "<f4") ? 4 : 8;
    const GDALDataType eOutDT = EQUAL(dtype, "<f4") ? GDT_Float32 : GDT_Float64;

    const char *astype = CSLFetchNameValue(options, "ASTYPE");
    if (!astype)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "quantize: ASTYPE missing");
        if (output_size)
            *output_size = 0;
        return false;
    }
    if (!EQUAL(astype, "<f4") && !EQUAL(astype, "<f8"))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "quantize: Only ASTYPE=<f4 or <f8 is supported. Not %s.",
                 astype);
        if (output_size)
            *output_size = 0;
        return false;
    }

    const int inputEltSize = EQUAL(astype, "<f4") ? 4 : 8;
    const GDALDataType eInDT = EQUAL(astype, "<f4") ? GDT_Float32 : GDT_Float64;

    if ((input_size % inputEltSize) != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "input_size should be a multiple of sizeof(ASTYPE)");
        if (output_size)
            *output_size = 0;
        return false;
    }

    const size_t nElts = input_size / inputEltSize;
    const uint64_t required_output_size64 =
        static_cast<uint64_t>(nElts) * outputEltSize;
    if constexpr (SIZEOF_VOIDP < 8)
    {
        if (required_output_size64 >= std::numeric_limits<size_t>::max())
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Too large input");
            if (output_size)
                *output_size = 0;
            return false;
        }
    }
    const size_t required_output_size =
        static_cast<size_t>(required_output_size64);

    if (output_data != nullptr && *output_data != nullptr &&
        output_size != nullptr && *output_size != 0)
    {
        if (*output_size < required_output_size)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Too small output size");
            *output_size = required_output_size;
            return false;
        }

#ifdef CPL_MSB
        std::vector<GByte> abyTmp;
        try
        {
            abyTmp.resize(input_size);
        }
        catch (const std::exception &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "ZarrQuantizeDecompressor: Out of memory");
            return false;
        }
        memcpy(abyTmp.data(), input_data, input_size);
        GDALSwapWordsEx(abyTmp.data(), inputEltSize, nElts, inputEltSize);
        input_data = abyTmp.data();
#endif

        GDALCopyWords64(input_data, eInDT, inputEltSize, *output_data, eOutDT,
                        outputEltSize, nElts);

#ifdef CPL_MSB
        GDALSwapWordsEx(*output_data, outputEltSize, nElts, outputEltSize);
#endif

        *output_size = required_output_size;
        return true;
    }

#ifdef not_needed
    if (output_data == nullptr && output_size != nullptr)
    {
        *output_size = required_output_size;
        return true;
    }

    if (output_data != nullptr && *output_data == nullptr &&
        output_size != nullptr)
    {
        *output_data = VSI_MALLOC_VERBOSE(required_output_size);
        *output_size = required_output_size;
        if (*output_data == nullptr)
            return false;
        bool ret = ZarrQuantizeDecompressor(input_data, input_size, output_data,
                                            output_size, options, nullptr);
        if (!ret)
        {
            VSIFree(*output_data);
            *output_data = nullptr;
        }
        return ret;
    }
#endif

    CPLError(CE_Failure, CPLE_AppDefined, "Invalid use of API");
    return false;
}

/************************************************************************/
/*                     ZarrGetQuantizeDecompressor()                    */
/************************************************************************/

const CPLCompressor *ZarrGetQuantizeDecompressor()
{
    static const CPLCompressor gQuantizeDecompressor = {
        /* nStructVersion = */ 1,
        /* pszId = */ "quantize",
        CCT_FILTER,
        /* papszMetadata = */ nullptr,
        ZarrQuantizeDecompressor,
        /* user_data = */ nullptr};

    return &gQuantizeDecompressor;
}

/************************************************************************/
/*                 ZarrFixedScaleOffsetDecompressor()                   */
/************************************************************************/

static bool ZarrFixedScaleOffsetDecompressor(const void *input_data,
                                             size_t input_size,
                                             void **output_data,
                                             size_t *output_size,
                                             CSLConstList options,
                                             void * /* compressor_user_data */)
{
    const char *offset = CSLFetchNameValue(options, "OFFSET");
    if (!offset)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "fixedscaleoffset: OFFSET missing");
        if (output_size)
            *output_size = 0;
        return false;
    }
    const double dfOffset = CPLAtof(offset);

    const char *scale = CSLFetchNameValue(options, "SCALE");
    if (!scale)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "fixedscaleoffset: SCALE missing");
        if (output_size)
            *output_size = 0;
        return false;
    }
    const double dfScale = CPLAtof(scale);
    if (dfScale == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "fixedscaleoffset: SCALE = 0 is invalid");
        if (output_size)
            *output_size = 0;
        return false;
    }

    const char *dtype = CSLFetchNameValue(options, "DTYPE");
    if (!dtype)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "fixedscaleoffset: DTYPE missing");
        if (output_size)
            *output_size = 0;
        return false;
    }
    if (!EQUAL(dtype, "<f4") && !EQUAL(dtype, "<f8"))
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "fixedscaleoffset: Only DTYPE=<f4 or <f8 is supported. Not %s.",
            dtype);
        if (output_size)
            *output_size = 0;
        return false;
    }

    const GDALDataType eOutDT = EQUAL(dtype, "<f4") ? GDT_Float32 : GDT_Float64;
    const int outputEltSize = GDALGetDataTypeSizeBytes(eOutDT);

    const char *astype = CSLFetchNameValue(options, "ASTYPE");
    if (!astype)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "fixedscaleoffset: ASTYPE missing");
        if (output_size)
            *output_size = 0;
        return false;
    }
    if (!EQUAL(astype, "|u1") && !EQUAL(astype, "<u2") && !EQUAL(astype, "<u4"))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "fixedscaleoffset: Only ASTYPE=|u1, <u2 or <f4 is supported. "
                 "Not %s.",
                 astype);
        if (output_size)
            *output_size = 0;
        return false;
    }

    const int inputEltSize = astype[2] - '0';
    const GDALDataType eInDT = inputEltSize == 1   ? GDT_Byte
                               : inputEltSize == 2 ? GDT_UInt16
                                                   : GDT_UInt32;

    if ((input_size % inputEltSize) != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "input_size should be a multiple of sizeof(ASTYPE)");
        if (output_size)
            *output_size = 0;
        return false;
    }

    const size_t nElts = input_size / inputEltSize;
    const uint64_t required_output_size64 =
        static_cast<uint64_t>(nElts) * outputEltSize;
    if constexpr (SIZEOF_VOIDP < 8)
    {
        if (required_output_size64 >= std::numeric_limits<size_t>::max())
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Too large input");
            if (output_size)
                *output_size = 0;
            return false;
        }
    }
    const size_t required_output_size =
        static_cast<size_t>(required_output_size64);

    if (output_data != nullptr && *output_data != nullptr &&
        output_size != nullptr && *output_size != 0)
    {
        if (*output_size < required_output_size)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Too small output size");
            *output_size = required_output_size;
            return false;
        }

#ifdef CPL_MSB
        std::vector<GByte> abyTmp;
        try
        {
            abyTmp.resize(input_size);
        }
        catch (const std::exception &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "ZarrFixedScaleOffsetDecompressor: Out of memory");
            return false;
        }
        memcpy(abyTmp.data(), input_data, input_size);
        GDALSwapWordsEx(abyTmp.data(), inputEltSize, nElts, inputEltSize);
        input_data = abyTmp.data();
#endif

        GDALCopyWords64(input_data, eInDT, inputEltSize, *output_data, eOutDT,
                        outputEltSize, nElts);

        // Cf https://numcodecs.readthedocs.io/en/v0.4.1/fixedscaleoffset.html
        if (eOutDT == GDT_Float32)
        {
            float *pafData = static_cast<float *>(*output_data);
            for (size_t i = 0; i < nElts; ++i)
            {
                pafData[i] =
                    static_cast<float>(pafData[i] / dfScale + dfOffset);
            }
        }
        else
        {
            CPLAssert(eOutDT == GDT_Float64);
            double *padfData = static_cast<double *>(*output_data);
            for (size_t i = 0; i < nElts; ++i)
            {
                padfData[i] = padfData[i] / dfScale + dfOffset;
            }
        }

#ifdef CPL_MSB
        GDALSwapWordsEx(*output_data, outputEltSize, nElts, outputEltSize);
#endif

        *output_size = required_output_size;
        return true;
    }

#ifdef not_needed
    if (output_data == nullptr && output_size != nullptr)
    {
        *output_size = required_output_size;
        return true;
    }

    if (output_data != nullptr && *output_data == nullptr &&
        output_size != nullptr)
    {
        *output_data = VSI_MALLOC_VERBOSE(required_output_size);
        *output_size = required_output_size;
        if (*output_data == nullptr)
            return false;
        bool ret = ZarrFixedScaleOffsetDecompressor(
            input_data, input_size, output_data, output_size, options, nullptr);
        if (!ret)
        {
            VSIFree(*output_data);
            *output_data = nullptr;
        }
        return ret;
    }
#endif

    CPLError(CE_Failure, CPLE_AppDefined, "Invalid use of API");
    return false;
}

/************************************************************************/
/*                  ZarrGetFixedScaleOffsetDecompressor()               */
/************************************************************************/

const CPLCompressor *ZarrGetFixedScaleOffsetDecompressor()
{
    static const CPLCompressor gFixedScaleOffsetDecompressor = {
        /* nStructVersion = */ 1,
        /* pszId = */ "fixedscaleoffset",
        CCT_FILTER,
        /* papszMetadata = */ nullptr,
        ZarrFixedScaleOffsetDecompressor,
        /* user_data = */ nullptr};

    return &gFixedScaleOffsetDecompressor;
}

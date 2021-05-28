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
#include "cpl_string.h"

#include <mutex>
#include <vector>

static std::mutex gMutex;
static std::vector<CPLCompressor*>* gpCompressors = nullptr;
static std::vector<CPLCompressor*>* gpDecompressors = nullptr;

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
        gpCompressors = new std::vector<CPLCompressor*>();
    for( size_t i = 0; i < gpCompressors->size(); ++i )
    {
        if( strcmp(compressor->pszId, (*gpCompressors)[i]->pszId) == 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Compressor %s already registered", compressor->pszId);
            return false;
        }
    }
    CPLCompressor* copy = new CPLCompressor();
    copy->nStructVersion = 1;
    copy->pszId = CPLStrdup(compressor->pszId);
    copy->papszMetadata = CSLDuplicate(compressor->papszMetadata);
    copy->pfnFunc = compressor->pfnFunc;
    copy->user_data = compressor->user_data;
    gpCompressors->emplace_back(copy);
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
        gpDecompressors = new std::vector<CPLCompressor*>();
    for( size_t i = 0; i < gpDecompressors->size(); ++i )
    {
        if( strcmp(decompressor->pszId, (*gpDecompressors)[i]->pszId) == 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Decompressor %s already registered", decompressor->pszId);
            return false;
        }
    }
    CPLCompressor* copy = new CPLCompressor();
    copy->nStructVersion = 1;
    copy->pszId = CPLStrdup(decompressor->pszId);
    copy->papszMetadata = CSLDuplicate(decompressor->papszMetadata);
    copy->pfnFunc = decompressor->pfnFunc;
    copy->user_data = decompressor->user_data;
    gpDecompressors->emplace_back(copy);
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
    char** papszRet = nullptr;
    for( size_t i = 0; gpCompressors != nullptr && i < gpCompressors->size(); ++i )
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
    for( size_t i = 0; gpCompressors != nullptr && i < gpCompressors->size(); ++i )
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
    for( size_t i = 0; gpDecompressors != nullptr && i < gpDecompressors->size(); ++i )
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

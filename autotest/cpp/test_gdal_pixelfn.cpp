///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test constant and builtin arguments for C++ pixel functions
// Author:   Momtchil Momtchev <momtchil@momtchev.com>
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2022, Momtchil Momtchev <momtchil@momtchev.com>
/*
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

#include "gdal_unit_test.h"

#include "cpl_string.h"
#include "gdal_alg.h"
#include "gdal_priv.h"
#include "gdal.h"

#include <sstream>
#include <string>
#include <vector>

CPLErr CustomPixelFunc( void **papoSources, int nSources, void *pData,
                            int nXSize, int nYSize,
                            GDALDataType eSrcType, GDALDataType eBufType,
                            int nPixelSpace, int nLineSpace, CSLConstList papszArgs );

template<typename T>
inline double
GetSrcVal(const void* pSource, GDALDataType eSrcType, T ii)
{
    switch (eSrcType)
    {
        case GDT_Unknown:
            return 0;
        case GDT_Byte:
            return static_cast<const GByte*>(pSource)[ii];
        case GDT_UInt16:
            return static_cast<const GUInt16*>(pSource)[ii];
        case GDT_Int16:
            return static_cast<const GInt16*>(pSource)[ii];
        case GDT_UInt32:
            return static_cast<const GUInt32*>(pSource)[ii];
        case GDT_Int32:
            return static_cast<const GInt32*>(pSource)[ii];
        case GDT_Float32:
            return static_cast<const float*>(pSource)[ii];
        case GDT_Float64:
            return static_cast<const double*>(pSource)[ii];
        case GDT_CInt16:
            return static_cast<const GInt16*>(pSource)[2 * ii];
        case GDT_CInt32:
            return static_cast<const GInt32*>(pSource)[2 * ii];
        case GDT_CFloat32:
            return static_cast<const float*>(pSource)[2 * ii];
        case GDT_CFloat64:
            return static_cast<const double*>(pSource)[2 * ii];
        case GDT_TypeCount:
            break;
    }
    return 0;
}

CPLErr CustomPixelFunc( void **papoSources, int nSources, void *pData,
                            int nXSize, int nYSize,
                            GDALDataType eSrcType, GDALDataType eBufType,
                            int nPixelSpace, int nLineSpace, CSLConstList papszArgs ) {

    /* ---- Init ---- */
    if( nSources != 1 ) return CE_Failure;
    const char* pszConstant = CSLFetchNameValue(papszArgs, "customConstant");
    if (pszConstant == nullptr) return CE_Failure;
    if (strncmp(pszConstant, "something", strlen("something"))) return CE_Failure;
    const char* pszScale = CSLFetchNameValue(papszArgs, "scale");
    if (pszScale == nullptr) return CE_Failure;

    /* ---- Set pixels ---- */
    size_t ii = 0;
    for( int iLine = 0; iLine < nYSize; ++iLine )
    {
        for( int iCol = 0; iCol < nXSize; ++iCol, ++ii )
        {
            const double dfPixVal = GetSrcVal(papoSources[0], eSrcType, ii);
            GDALCopyWords(
                    &dfPixVal, GDT_Float64, 0,
                    static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1);
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}

namespace tut
{
    const char pszFuncMetadata[] =
        "<PixelFunctionArgumentsList>"
        "   <Argument name='customConstant' type='constant' value='something'>"
        "   </Argument>"
        "   <Argument type='builtin' value='scale'>"
        "   </Argument>"
        "</PixelFunctionArgumentsList>";
    const char src[] = "data/pixelfn.vrt";
    struct test_pixelfn_data {};

    // Register test group
    typedef test_group<test_pixelfn_data> group;
    typedef group::object object;
    group test_pixelfn_group("GDAL::PixelFunc");

    // Test constant parameters in a custom pixel function
    template<>
    template<>
    void object::test<1>()
    {
        GDALAddDerivedBandPixelFuncWithArgs("custom", CustomPixelFunc, pszFuncMetadata);
        GDALDatasetH ds = GDALOpen(src, GA_ReadOnly);
        ensure("Can't open dataset", nullptr != ds);

        GDALRasterBandH band = GDALGetRasterBand(ds, 1);
        ensure("Can't get raster band", nullptr != band);

        float buf[20 * 20];
        CPL_IGNORE_RET_VAL(GDALRasterIO(band, GF_Read, 0, 0, 20, 20, buf, 20, 20, GDT_Float32, 0, 0));

        ensure_equals("Read wrong data", buf[0], 107);

        GDALClose(ds);
    }

 } // namespace tut

/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Fuzzer
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2017, Even Rouault <even.rouault at spatialys.com>
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

#include <stddef.h>
#include <stdint.h>

#include "ogr_api.h"
#include "cpl_error.h"
#include "cpl_vsi.h"
#include "ogrsf_frmts.h"

#ifndef REGISTER_FUNC
#define REGISTER_FUNC OGRRegisterAll
#endif

#ifndef MEM_FILENAME
#define MEM_FILENAME "/vsimem/test"
#endif

#ifndef GDAL_FILENAME
#define GDAL_FILENAME MEM_FILENAME
#endif

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len);

int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len)
{
    VSILFILE* fp = VSIFileFromMemBuffer( MEM_FILENAME,
            reinterpret_cast<GByte*>(const_cast<uint8_t*>(buf)), len, FALSE );
    VSIFCloseL(fp);
    REGISTER_FUNC();
    CPLPushErrorHandler(CPLQuietErrorHandler);
    OGRDataSourceH hDS = OGROpen( GDAL_FILENAME, FALSE, NULL );
    if( hDS )
    {
        const int nLayers = OGR_DS_GetLayerCount(hDS);
        for( int i = 0; i < nLayers; i++ )
        {
            OGRLayerH hLayer = OGR_DS_GetLayer(hDS, i);
            OGRFeatureH hFeature;
            while( (hFeature = OGR_L_GetNextFeature(hLayer)) != NULL )
            {
                OGR_F_Destroy(hFeature);
            }
        }
        OGR_DS_Destroy(hDS);
    }
    CPLPopErrorHandler();
    VSIUnlink( MEM_FILENAME );
    return 0;
}

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
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_vsi.h"
#include "ogrsf_frmts.h"

#ifndef REGISTER_FUNC
#define REGISTER_FUNC OGRRegisterAll
#ifndef OGR_SKIP
#define OGR_SKIP "CAD,DXF"
#endif
#endif

#ifndef EXTENSION
#define EXTENSION "bin"
#endif

#ifndef MEM_FILENAME
#define MEM_FILENAME "/vsimem/test"
#endif

#ifndef GDAL_FILENAME
#define GDAL_FILENAME MEM_FILENAME
#endif

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv);
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len);

int LLVMFuzzerInitialize(int* /*argc*/, char*** argv)
{
    const char* exe_path = (*argv)[0];
    if( CPLGetConfigOption("GDAL_DATA", nullptr) == nullptr )
    {
        CPLSetConfigOption("GDAL_DATA", CPLGetPath(exe_path));
    }
    CPLSetConfigOption("CPL_TMPDIR", "/tmp");
    CPLSetConfigOption("DISABLE_OPEN_REAL_NETCDF_FILES", "YES");
    CPLSetConfigOption("GDAL_HTTP_TIMEOUT", "1");
    CPLSetConfigOption("GDAL_HTTP_CONNECTTIMEOUT", "1");
    // To avoid timeouts. See https://github.com/OSGeo/gdal/issues/502
    CPLSetConfigOption("DXF_MAX_BSPLINE_CONTROL_POINTS", "100");
    CPLSetConfigOption("NAS_INDICATOR","NAS-Operationen;AAA-Fachschema;aaa.xsd;aaa-suite");
    CPLSetConfigOption("USERNAME", "unknown"); // see GMLASConfiguration::GetBaseCacheDirectory()

#ifdef OGR_SKIP
    CPLSetConfigOption("OGR_SKIP", OGR_SKIP);
#endif
    REGISTER_FUNC();

    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len)
{
#ifdef USE_FILESYSTEM
    char szTempFilename[64];
    snprintf(szTempFilename, sizeof(szTempFilename),
             "/tmp/gdal_fuzzer_%d.%s",
             (int)getpid(), EXTENSION);
    VSILFILE* fp = VSIFOpenL(szTempFilename, "wb");
    if( !fp )
    {
        fprintf(stderr, "Cannot create %s\n", szTempFilename);
        return 1;
    }
    VSIFWriteL( buf, 1, len, fp );
#else
    VSILFILE* fp = VSIFileFromMemBuffer( MEM_FILENAME,
            reinterpret_cast<GByte*>(const_cast<uint8_t*>(buf)), len, FALSE );
#endif
    VSIFCloseL(fp);

    CPLPushErrorHandler(CPLQuietErrorHandler);
#ifdef USE_FILESYSTEM
    OGRDataSourceH hDS = OGROpen( szTempFilename, FALSE, nullptr );
#else
    OGRDataSourceH hDS = OGROpen( GDAL_FILENAME, FALSE, nullptr );
#endif
    if( hDS )
    {
        const int nLayers = OGR_DS_GetLayerCount(hDS);
        time_t nStartTime = time(nullptr);
        bool bStop = false;
        for( int i = 0; !bStop && i < 10 && i < nLayers; i++ )
        {
            OGRLayerH hLayer = OGR_DS_GetLayer(hDS, i);
            OGR_L_GetSpatialRef(hLayer);
            OGR_L_GetGeomType(hLayer);
            OGR_L_GetFIDColumn(hLayer);
            OGR_L_GetGeometryColumn(hLayer);
            OGRFeatureH hFeature;
            OGRFeatureH hFeaturePrev = nullptr;
            for( int j = 0; j < 1000 && !bStop &&
                    (hFeature = OGR_L_GetNextFeature(hLayer)) != nullptr; j++ )
            {
                // Limit runtime to 20 seconds if features returned are
                // different. Otherwise this may be a sign of a bug in the
                // reader and we want the infinite loop to be revealed.
                if( time(nullptr) - nStartTime > 20 )
                {
                    bool bIsSameAsPrevious =
                        (hFeaturePrev != nullptr &&
                         OGR_F_Equal(hFeature, hFeaturePrev));
                    if( !bIsSameAsPrevious )
                    {
                        bStop = true;
                    }
                }
                if( hFeaturePrev )
                    OGR_F_Destroy(hFeaturePrev);
                hFeaturePrev = hFeature;
            }
            if( hFeaturePrev )
                OGR_F_Destroy(hFeaturePrev);
        }
        OGR_DS_Destroy(hDS);
    }
    CPLPopErrorHandler();
#ifdef USE_FILESYSTEM
    VSIUnlink( szTempFilename );
#else
    VSIUnlink( MEM_FILENAME );
#endif
    return 0;
}

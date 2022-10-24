/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  bench_ogr_bach
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
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

#include "gdal_priv.h"
#include "ogr_api.h"
#include "ogrsf_frmts.h"
#include "ogr_recordbatch.h"

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()
{
    printf("Usage: bench_ogr_bach filename\n");
    exit(1);
}

/************************************************************************/
/*                               main()                                 */
/************************************************************************/

int main(int argc, char* argv[])
{
/* -------------------------------------------------------------------- */
/*      Process arguments.                                              */
/* -------------------------------------------------------------------- */
    argc = GDALGeneralCmdLineProcessor(argc, &argv, 0);
    if( argc < 1 )
        exit(-argc);

    if( argc != 2 )
        Usage();

    GDALAllRegister();

    auto poDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(argv[1]));
    if( poDS == nullptr)
    {
        fprintf(stderr, "Cannot open %s\n", argv[1]);
        CSLDestroy(argv);
        exit(1);
    }

    OGRLayer* poLayer = poDS->GetLayer(0);
    if( poLayer == nullptr )
    {
        fprintf(stderr, "Cannot find layer\n");
        CSLDestroy(argv);
        exit(1);
    }

    OGRLayerH hLayer = OGRLayer::ToHandle(poLayer);
    struct ArrowArrayStream stream;
    if( !OGR_L_GetArrowStream(hLayer, &stream, nullptr))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "OGR_L_GetArrowStream() failed\n");
        CSLDestroy(argv);
        exit(1);
    }
#if 0
    struct ArrowSchema schema;
    if( stream.get_schema(&stream, &schema) == 0 )
    {
        // Do something useful
        schema.release(&schema);
    }
#endif
    while( true )
    {
        struct ArrowArray array;
        if( stream.get_next(&stream, &array) != 0 ||
            array.release == nullptr )
        {
            break;
        }
        array.release(&array);
    }
    stream.release(&stream);

    poDS.reset();

    CSLDestroy(argv);

    GDALDestroyDriverManager();

    return 0;
}

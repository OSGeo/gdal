/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  bench_ogr_batch
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
    printf(
        "Usage: bench_ogr_batch [-where filter] [-spat xmin ymin xmax ymax]\n");
    printf("                      [--stream-opt NAME=VALUE] [-v]*\n");
    printf("                      filename [layer_name]\n");
    exit(1);
}

/************************************************************************/
/*                               main()                                 */
/************************************************************************/

int main(int argc, char *argv[])
{
    /* -------------------------------------------------------------------- */
    /*      Process arguments.                                              */
    /* -------------------------------------------------------------------- */
    argc = GDALGeneralCmdLineProcessor(argc, &argv, 0);
    if (argc < 1)
        exit(-argc);

    const char *pszWhere = nullptr;
    const char *pszDataset = nullptr;
    std::unique_ptr<OGRPolygon> poSpatialFilter;
    const char *pszLayerName = nullptr;
    CPLStringList aosSteamOptions;
    bool bVerbose = false;
    for (int iArg = 1; iArg < argc; ++iArg)
    {
        if (iArg + 1 < argc && strcmp(argv[iArg], "-where") == 0)
        {
            pszWhere = argv[iArg + 1];
            ++iArg;
        }
        else if (iArg + 4 < argc && strcmp(argv[iArg], "-spat") == 0)
        {
            OGRLinearRing oRing;
            oRing.addPoint(CPLAtof(argv[iArg + 1]), CPLAtof(argv[iArg + 2]));
            oRing.addPoint(CPLAtof(argv[iArg + 1]), CPLAtof(argv[iArg + 4]));
            oRing.addPoint(CPLAtof(argv[iArg + 3]), CPLAtof(argv[iArg + 4]));
            oRing.addPoint(CPLAtof(argv[iArg + 3]), CPLAtof(argv[iArg + 2]));
            oRing.addPoint(CPLAtof(argv[iArg + 1]), CPLAtof(argv[iArg + 2]));

            poSpatialFilter = cpl::make_unique<OGRPolygon>();
            poSpatialFilter->addRing(&oRing);

            iArg += 4;
        }
        else if (iArg + 1 < argc && strcmp(argv[iArg], "--stream-opt") == 0)
        {
            aosSteamOptions.AddString(argv[iArg + 1]);
            ++iArg;
        }
        else if (strcmp(argv[iArg], "-v") == 0)
        {
            bVerbose = true;
        }
        else if (argv[iArg][0] == '-')
        {
            Usage();
        }
        else if (pszDataset == nullptr)
        {
            pszDataset = argv[iArg];
        }
        else if (pszLayerName == nullptr)
        {
            pszLayerName = argv[iArg];
        }
        else
        {
            Usage();
        }
    }
    if (pszDataset == nullptr)
    {
        Usage();
    }

    GDALAllRegister();

    auto poDS = std::unique_ptr<GDALDataset>(
        GDALDataset::Open(pszDataset, GDAL_OF_VECTOR | GDAL_OF_VERBOSE_ERROR));
    if (poDS == nullptr)
    {
        CSLDestroy(argv);
        exit(1);
    }

    if (pszLayerName == nullptr && poDS->GetLayerCount() > 1)
    {
        fprintf(stderr, "A layer name must be specified because the dataset "
                        "has several layers.\n");
        CSLDestroy(argv);
        exit(1);
    }
    OGRLayer *poLayer =
        pszLayerName ? poDS->GetLayerByName(pszLayerName) : poDS->GetLayer(0);
    if (poLayer == nullptr)
    {
        fprintf(stderr, "Cannot find layer\n");
        CSLDestroy(argv);
        exit(1);
    }
    if (pszWhere)
        poLayer->SetAttributeFilter(pszWhere);
    if (poSpatialFilter)
        poLayer->SetSpatialFilter(poSpatialFilter.get());

    OGRLayerH hLayer = OGRLayer::ToHandle(poLayer);
    struct ArrowArrayStream stream;
    if (!OGR_L_GetArrowStream(hLayer, &stream, aosSteamOptions.List()))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "OGR_L_GetArrowStream() failed\n");
        CSLDestroy(argv);
        exit(1);
    }

    struct ArrowSchema schema;
    if (stream.get_schema(&stream, &schema) == 0)
    {
        // Do something useful
        schema.release(&schema);
    }
    else
    {
        schema.release(&schema);
        stream.release(&stream);
        CPLError(CE_Failure, CPLE_AppDefined, "get_schema() failed\n");
        CSLDestroy(argv);
        exit(1);
    }

#if 0
    int64_t lastId = 0;
#endif
    GUIntBig nFeatureCount = 0;
    while (true)
    {
        struct ArrowArray array;
        if (stream.get_next(&stream, &array) != 0 || array.release == nullptr)
        {
            break;
        }
        nFeatureCount += array.length;
#if 0
        const int64_t* fid_col = static_cast<const int64_t*>(array.children[0]->buffers[1]);
        for(int64_t i = 0; i < array.length; ++i )
        {
            int64_t id = fid_col[i];
            if( id != lastId + 1 )
                printf(CPL_FRMT_GIB "\n", static_cast<GIntBig>(id));
            lastId = id;
        }
#endif
        array.release(&array);
    }
    stream.release(&stream);

    if (bVerbose)
    {
        printf(CPL_FRMT_GUIB " features/rows selected\n", nFeatureCount);
    }

    poDS.reset();

    CSLDestroy(argv);

    GDALDestroyDriverManager();

    return 0;
}

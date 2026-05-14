/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  bench_ogr_c_api
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_priv.h"
#include "ogr_api.h"
#include "ogrsf_frmts.h"

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()
{
    printf(
        "Usage: bench_ogr_c_api [-where filter] [-spat xmin ymin xmax ymax]\n");
    printf("                       [-oo NAME=VALUE]* filename [layer_name]\n");
    exit(1);
}

/************************************************************************/
/*                                main()                                */
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
    CPLStringList aosOpenOptions;
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

            poSpatialFilter = std::make_unique<OGRPolygon>();
            poSpatialFilter->addRing(&oRing);

            iArg += 4;
        }
        else if (iArg + 1 < argc && strcmp(argv[iArg], "-oo") == 0)
        {
            ++iArg;
            aosOpenOptions.AddString(argv[iArg]);
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
        GDALDataset::Open(pszDataset, GDAL_OF_VECTOR | GDAL_OF_VERBOSE_ERROR,
                          nullptr, aosOpenOptions.List()));
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
    OGRFeatureDefnH hFDefn = OGR_L_GetLayerDefn(hLayer);
    int nFields = OGR_FD_GetFieldCount(hFDefn);
    std::vector<OGRFieldType> aeTypes;
    for (int i = 0; i < nFields; i++)
        aeTypes.push_back(OGR_Fld_GetType(OGR_FD_GetFieldDefn(hFDefn, i)));
    int nYear, nMonth, nDay, nHour, nMin, nSecond, nTZ;
    std::vector<GByte> abyWKB;
    while (true)
    {
        OGRFeatureH hFeat = OGR_L_GetNextFeature(hLayer);
        if (hFeat == nullptr)
            break;
        OGR_F_GetFID(hFeat);
        for (int i = 0; i < nFields; i++)
        {
            if (aeTypes[i] == OFTInteger)
                OGR_F_GetFieldAsInteger(hFeat, i);
            else if (aeTypes[i] == OFTInteger64)
                OGR_F_GetFieldAsInteger64(hFeat, i);
            else if (aeTypes[i] == OFTReal)
                OGR_F_GetFieldAsDouble(hFeat, i);
            else if (aeTypes[i] == OFTString)
                OGR_F_GetFieldAsString(hFeat, i);
            else if (aeTypes[i] == OFTDate || aeTypes[i] == OFTDateTime)
                OGR_F_GetFieldAsDateTime(hFeat, i, &nYear, &nMonth, &nDay,
                                         &nHour, &nMin, &nSecond, &nTZ);
        }
        OGRGeometryH hGeom = OGR_F_GetGeometryRef(hFeat);
        if (hGeom)
        {
            int size = OGR_G_WkbSize(hGeom);
            abyWKB.resize(size);
            OGR_G_ExportToIsoWkb(hGeom, wkbNDR, abyWKB.data());
        }
        OGR_F_Destroy(hFeat);
    }

    poDS.reset();

    CSLDestroy(argv);

    GDALDestroyDriverManager();

    return 0;
}

/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Test multi-threaded reprojection
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include <assert.h>

#include "cpl_string.h"
#include "cpl_atomic_ops.h"
#include "cpl_multiproc.h"
#include "ogr_spatialref.h"

double *padfRefX = nullptr;
double *padfRefY = nullptr;
double *padfRefResultX = nullptr;
double *padfRefResultY = nullptr;
OGRCoordinateTransformation *poCT = nullptr;
volatile int nIter = 0;
bool bCreateCTInThread = false;
OGRSpatialReference oSrcSRS;
OGRSpatialReference oDstSRS;
int nCountIter = 10000;

static void ReprojFunc(void * /* unused */)
{
    double *padfResultX =
        static_cast<double *>(CPLMalloc(1024 * sizeof(double)));
    double *padfResultY =
        static_cast<double *>(CPLMalloc(1024 * sizeof(double)));
    OGRCoordinateTransformation *poCTInThread;
    if (!bCreateCTInThread)
        poCTInThread = poCT;
    while (true)
    {
        if (bCreateCTInThread)
            poCTInThread =
                OGRCreateCoordinateTransformation(&oSrcSRS, &oDstSRS);

        CPLAtomicInc(&nIter);
        memcpy(padfResultX, padfRefX, 1024 * sizeof(double));
        memcpy(padfResultY, padfRefY, 1024 * sizeof(double));
        poCT->Transform(1024, padfResultX, padfResultY, nullptr, nullptr);

        /* Check that the results are consistent with the reference results */
        assert(memcmp(padfResultX, padfRefResultX, 1024 * sizeof(double)) == 0);
        assert(memcmp(padfResultY, padfRefResultY, 1024 * sizeof(double)) == 0);

        if (bCreateCTInThread)
            OGRCoordinateTransformation::DestroyCT(poCTInThread);
    }
}

int main(int argc, char *argv[])
{
    int nThreads = 2;

    for (int i = 0; i < argc; i++)
    {
        if (EQUAL(argv[i], "-threads") && i + 1 < argc)
            nThreads = atoi(argv[++i]);
        else if (EQUAL(argv[i], "-iter") && i + 1 < argc)
            nCountIter = atoi(argv[++i]);
        else if (EQUAL(argv[i], "-createctinthread"))
            bCreateCTInThread = true;
    }

    oSrcSRS.importFromEPSG(4326);
    oSrcSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    oDstSRS.importFromEPSG(32631);
    oDstSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    poCT = OGRCreateCoordinateTransformation(&oSrcSRS, &oDstSRS);
    if (poCT == nullptr)
        return -1;

    padfRefX = static_cast<double *>(CPLMalloc(1024 * sizeof(double)));
    padfRefY = static_cast<double *>(CPLMalloc(1024 * sizeof(double)));
    padfRefResultX = static_cast<double *>(CPLMalloc(1024 * sizeof(double)));
    padfRefResultY = static_cast<double *>(CPLMalloc(1024 * sizeof(double)));

    for (int i = 0; i < 1024; i++)
    {
        padfRefX[i] = 2 + i / 1024.;
        padfRefY[i] = 49 + i / 1024.;
    }
    memcpy(padfRefResultX, padfRefX, 1024 * sizeof(double));
    memcpy(padfRefResultY, padfRefY, 1024 * sizeof(double));

    poCT->Transform(1024, padfRefResultX, padfRefResultY, nullptr, nullptr);

    for (int i = 0; i < nThreads; i++)
        CPLCreateThread(ReprojFunc, nullptr);

    while (nIter < nCountIter)
        CPLSleep(0.001);

    return 0;
}

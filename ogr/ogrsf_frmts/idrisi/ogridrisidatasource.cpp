/******************************************************************************
 *
 * Project:  Idrisi Translator
 * Purpose:  Implements OGRIdrisiDataSource class
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2011-2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_conv.h"
#include "cpl_string.h"
#include "idrisi.h"
#include "ogr_idrisi.h"

/************************************************************************/
/*                        OGRIdrisiDataSource()                         */
/************************************************************************/

OGRIdrisiDataSource::OGRIdrisiDataSource() = default;

/************************************************************************/
/*                       ~OGRIdrisiDataSource()                         */
/************************************************************************/

OGRIdrisiDataSource::~OGRIdrisiDataSource()

{
    for (int i = 0; i < nLayers; i++)
        delete papoLayers[i];
    CPLFree(papoLayers);
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRIdrisiDataSource::GetLayer(int iLayer)

{
    if (iLayer < 0 || iLayer >= nLayers)
        return nullptr;

    return papoLayers[iLayer];
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRIdrisiDataSource::Open(const char *pszFilename)

{
    VSILFILE *fpVCT = VSIFOpenL(pszFilename, "rb");
    if (fpVCT == nullptr)
        return FALSE;

    // --------------------------------------------------------------------
    //      Look for .vdc file
    // --------------------------------------------------------------------
    std::string osVDCFilename = CPLResetExtensionSafe(pszFilename, "vdc");
    VSILFILE *fpVDC = VSIFOpenL(osVDCFilename.c_str(), "rb");
    if (fpVDC == nullptr)
    {
        osVDCFilename = CPLResetExtensionSafe(pszFilename, "VDC");
        fpVDC = VSIFOpenL(osVDCFilename.c_str(), "rb");
    }

    char **papszVDC = nullptr;
    if (fpVDC != nullptr)
    {
        VSIFCloseL(fpVDC);
        fpVDC = nullptr;

        CPLPushErrorHandler(CPLQuietErrorHandler);
        papszVDC = CSLLoad2(osVDCFilename.c_str(), 1024, 256, nullptr);
        CPLPopErrorHandler();
        CPLErrorReset();
    }

    OGRwkbGeometryType eType = wkbUnknown;

    char *pszWTKString = nullptr;
    if (papszVDC != nullptr)
    {
        CSLSetNameValueSeparator(papszVDC, ":");

        const char *pszVersion = CSLFetchNameValue(papszVDC, "file format");

        if (pszVersion == nullptr || !EQUAL(pszVersion, "IDRISI Vector A.1"))
        {
            CSLDestroy(papszVDC);
            VSIFCloseL(fpVCT);
            return FALSE;
        }

        const char *pszRefSystem = CSLFetchNameValue(papszVDC, "ref. system");
        const char *pszRefUnits = CSLFetchNameValue(papszVDC, "ref. units");

        if (pszRefSystem != nullptr && pszRefUnits != nullptr)
        {
            OGRSpatialReference oSRS;
            IdrisiGeoReference2Wkt(pszFilename, pszRefSystem, pszRefUnits,
                                   oSRS);
            if (!oSRS.IsEmpty())
            {
                oSRS.exportToWkt(&pszWTKString);
            }
        }
    }

    GByte chType = 0;
    if (VSIFReadL(&chType, 1, 1, fpVCT) != 1)
    {
        VSIFCloseL(fpVCT);
        CSLDestroy(papszVDC);
        CPLFree(pszWTKString);
        return FALSE;
    }

    if (chType == 1)
        eType = wkbPoint;
    else if (chType == 2)
        eType = wkbLineString;
    else if (chType == 3)
        eType = wkbPolygon;
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unsupported geometry type : %d",
                 static_cast<int>(chType));
        VSIFCloseL(fpVCT);
        CSLDestroy(papszVDC);
        CPLFree(pszWTKString);
        return FALSE;
    }

    const char *pszMinX = CSLFetchNameValue(papszVDC, "min. X");
    const char *pszMaxX = CSLFetchNameValue(papszVDC, "max. X");
    const char *pszMinY = CSLFetchNameValue(papszVDC, "min. Y");
    const char *pszMaxY = CSLFetchNameValue(papszVDC, "max. Y");

    OGRIdrisiLayer *poLayer =
        new OGRIdrisiLayer(pszFilename, CPLGetBasenameSafe(pszFilename).c_str(),
                           fpVCT, eType, pszWTKString);
    papoLayers = static_cast<OGRLayer **>(CPLMalloc(sizeof(OGRLayer *)));
    papoLayers[nLayers++] = poLayer;

    if (pszMinX != nullptr && pszMaxX != nullptr && pszMinY != nullptr &&
        pszMaxY != nullptr)
    {
        poLayer->SetExtent(CPLAtof(pszMinX), CPLAtof(pszMinY), CPLAtof(pszMaxX),
                           CPLAtof(pszMaxY));
    }

    CPLFree(pszWTKString);

    CSLDestroy(papszVDC);

    return TRUE;
}

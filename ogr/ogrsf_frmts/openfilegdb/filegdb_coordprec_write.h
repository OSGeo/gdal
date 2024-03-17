/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Open FileGDB OGR driver.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef FILEGDB_COORDPREC_WRITE_H
#define FILEGDB_COORDPREC_WRITE_H

#include "ogr_spatialref.h"
#include "ogr_geomcoordinateprecision.h"

/*************************************************************************/
/*                      GDBGridSettingsFromOGR()                         */
/*************************************************************************/

/** Compute grid settings from coordinate precision of source geometry field
 * and layer creation options.
 * The "FileGeodatabase" key of the output oFormatSpecificOptions will be
 * set with the values.
 */
static OGRGeomCoordinatePrecision
GDBGridSettingsFromOGR(const OGRGeomFieldDefn *poSrcGeomFieldDefn,
                       CSLConstList aosLayerCreationOptions)
{
    const auto poSRS = poSrcGeomFieldDefn->GetSpatialRef();

    double dfXOrigin;
    double dfYOrigin;
    double dfXYScale;
    double dfZOrigin = -100000;
    double dfMOrigin = -100000;
    double dfMScale = 10000;
    double dfXYTolerance;
    // default tolerance is 1mm in the units of the coordinate system
    double dfZTolerance =
        0.001 * (poSRS ? poSRS->GetTargetLinearUnits("VERT_CS") : 1.0);
    double dfZScale = 1 / dfZTolerance * 10;
    double dfMTolerance = 0.001;

    if (poSRS == nullptr || poSRS->IsProjected())
    {
        // default tolerance is 1mm in the units of the coordinate system
        dfXYTolerance =
            0.001 * (poSRS ? poSRS->GetTargetLinearUnits("PROJCS") : 1.0);
        // default scale is 10x the tolerance
        dfXYScale = 1 / dfXYTolerance * 10;

        // Ideally we would use the same X/Y origins as ArcGIS, but we need
        // the algorithm they use.
        dfXOrigin = -2147483647;
        dfYOrigin = -2147483647;
    }
    else
    {
        dfXOrigin = -400;
        dfYOrigin = -400;
        dfXYScale = 1000000000;
        dfXYTolerance = 0.000000008983153;
    }

    const auto &oSrcCoordPrec = poSrcGeomFieldDefn->GetCoordinatePrecision();

    if (oSrcCoordPrec.dfXYResolution != OGRGeomCoordinatePrecision::UNKNOWN)
    {
        dfXYScale = 1.0 / oSrcCoordPrec.dfXYResolution;
        dfXYTolerance = oSrcCoordPrec.dfXYResolution / 10.0;
    }

    if (oSrcCoordPrec.dfZResolution != OGRGeomCoordinatePrecision::UNKNOWN)
    {
        dfZScale = 1.0 / oSrcCoordPrec.dfZResolution;
        dfZTolerance = oSrcCoordPrec.dfZResolution / 10.0;
    }

    if (oSrcCoordPrec.dfMResolution != OGRGeomCoordinatePrecision::UNKNOWN)
    {
        dfMScale = 1.0 / oSrcCoordPrec.dfMResolution;
        dfMTolerance = oSrcCoordPrec.dfMResolution / 10.0;
    }

    const char *const paramNames[] = {
        "XOrigin", "YOrigin", "XYScale",     "ZOrigin",    "ZScale",
        "MOrigin", "MScale",  "XYTolerance", "ZTolerance", "MTolerance"};
    double *pGridValues[] = {
        &dfXOrigin, &dfYOrigin, &dfXYScale,     &dfZOrigin,    &dfZScale,
        &dfMOrigin, &dfMScale,  &dfXYTolerance, &dfZTolerance, &dfMTolerance};
    static_assert(CPL_ARRAYSIZE(paramNames) == CPL_ARRAYSIZE(pGridValues));

    const auto oIterCoordPrecFileGeodatabase =
        oSrcCoordPrec.oFormatSpecificOptions.find("FileGeodatabase");

    /*
     * Use coordinate precision layer creation options in priority.
     * Otherwise use the settings from the "FileGeodatabase" entry in
     * oSrcCoordPrec.oFormatSpecificOptions when set.
     * Otherwise, use above defaults.
     */
    CPLStringList aosCoordinatePrecisionOptions;
    for (size_t i = 0; i < CPL_ARRAYSIZE(paramNames); i++)
    {
        const char *pszVal = CSLFetchNameValueDef(
            aosLayerCreationOptions, paramNames[i],
            oIterCoordPrecFileGeodatabase !=
                    oSrcCoordPrec.oFormatSpecificOptions.end()
                ? oIterCoordPrecFileGeodatabase->second.FetchNameValue(
                      paramNames[i])
                : nullptr);
        if (pszVal)
        {
            *(pGridValues[i]) = CPLAtof(pszVal);
            if (strstr(paramNames[i], "Scale") ||
                strstr(paramNames[i], "Tolerance"))
            {
                if (*(pGridValues[i]) <= 0)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "%s should be strictly greater than zero",
                             paramNames[i]);
                }
            }
        }
        aosCoordinatePrecisionOptions.SetNameValue(
            paramNames[i], CPLSPrintf("%.15g", *(pGridValues[i])));
    }

    OGRGeomCoordinatePrecision oCoordPrec;
    oCoordPrec.dfXYResolution = 1.0 / dfXYScale;
    oCoordPrec.dfZResolution = 1.0 / dfZScale;
    oCoordPrec.dfMResolution = 1.0 / dfMScale;
    oCoordPrec.oFormatSpecificOptions["FileGeodatabase"] =
        std::move(aosCoordinatePrecisionOptions);

    return oCoordPrec;
}

#endif /* FILEGDB_COORDPREC_WRITE_H */

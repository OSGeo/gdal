/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGeomCoordinatePrecision.
 * Author:   Even Rouault <even dot rouault at spatialys.com>
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

#include "ogr_core.h"
#include "ogr_api.h"
#include "ogr_spatialref.h"
#include "ogr_geomcoordinateprecision.h"

#include <algorithm>
#include <cmath>

/************************************************************************/
/*                    OGRGeomCoordinatePrecisionCreate()                */
/************************************************************************/

/** Creates a new instance of OGRGeomCoordinatePrecision.
 *
 * The default X,Y,Z,M resolutions are set to OGR_GEOM_COORD_PRECISION_UNKNOWN.
 *
 * @since GDAL 3.9
 */
OGRGeomCoordinatePrecisionH OGRGeomCoordinatePrecisionCreate(void)
{
    static_assert(OGR_GEOM_COORD_PRECISION_UNKNOWN ==
                  OGRGeomCoordinatePrecision::UNKNOWN);

    return new OGRGeomCoordinatePrecision();
}

/************************************************************************/
/*                    OGRGeomCoordinatePrecisionDestroy()               */
/************************************************************************/

/** Destroy a OGRGeomCoordinatePrecision.
 *
 * @param hGeomCoordPrec OGRGeomCoordinatePrecision instance or nullptr
 * @since GDAL 3.9
 */
void OGRGeomCoordinatePrecisionDestroy(
    OGRGeomCoordinatePrecisionH hGeomCoordPrec)
{
    delete hGeomCoordPrec;
}

/************************************************************************/
/*                 OGRGeomCoordinatePrecisionGetXYResolution()          */
/************************************************************************/

/** Get the X/Y resolution of a OGRGeomCoordinatePrecision
 *
 * @param hGeomCoordPrec OGRGeomCoordinatePrecision instance (must not be null)
 * @return the the X/Y resolution of a OGRGeomCoordinatePrecision or
 * OGR_GEOM_COORD_PRECISION_UNKNOWN
 * @since GDAL 3.9
 */
double OGRGeomCoordinatePrecisionGetXYResolution(
    OGRGeomCoordinatePrecisionH hGeomCoordPrec)
{
    VALIDATE_POINTER1(hGeomCoordPrec,
                      "OGRGeomCoordinatePrecisionGetXYResolution", 0);
    return hGeomCoordPrec->dfXYResolution;
}

/************************************************************************/
/*                 OGRGeomCoordinatePrecisionGetZResolution()           */
/************************************************************************/

/** Get the Z resolution of a OGRGeomCoordinatePrecision
 *
 * @param hGeomCoordPrec OGRGeomCoordinatePrecision instance (must not be null)
 * @return the the Z resolution of a OGRGeomCoordinatePrecision or
 * OGR_GEOM_COORD_PRECISION_UNKNOWN
 * @since GDAL 3.9
 */
double OGRGeomCoordinatePrecisionGetZResolution(
    OGRGeomCoordinatePrecisionH hGeomCoordPrec)
{
    VALIDATE_POINTER1(hGeomCoordPrec,
                      "OGRGeomCoordinatePrecisionGetZResolution", 0);
    return hGeomCoordPrec->dfZResolution;
}

/************************************************************************/
/*                 OGRGeomCoordinatePrecisionGetMResolution()           */
/************************************************************************/

/** Get the M resolution of a OGRGeomCoordinatePrecision
 *
 * @param hGeomCoordPrec OGRGeomCoordinatePrecision instance (must not be null)
 * @return the the M resolution of a OGRGeomCoordinatePrecision or
 * OGR_GEOM_COORD_PRECISION_UNKNOWN
 * @since GDAL 3.9
 */
double OGRGeomCoordinatePrecisionGetMResolution(
    OGRGeomCoordinatePrecisionH hGeomCoordPrec)
{
    VALIDATE_POINTER1(hGeomCoordPrec,
                      "OGRGeomCoordinatePrecisionGetMResolution", 0);
    return hGeomCoordPrec->dfMResolution;
}

/************************************************************************/
/*                  OGRGeomCoordinatePrecisionGetFormats()              */
/************************************************************************/

/** Get the list of format names for coordinate precision format specific
 * options.
 *
 * An example of a supported value for pszFormatName is
 * "FileGeodatabase" for layers of the OpenFileGDB driver.
 *
 * The returned values may be used for the pszFormatName argument of
 * OGRGeomCoordinatePrecisionGetFormatSpecificOptions().
 *
 * @param hGeomCoordPrec OGRGeomCoordinatePrecision instance (must not be null)
 * @return a null-terminated list to free with CSLDestroy(), or nullptr.
 * @since GDAL 3.9
 */
char **
OGRGeomCoordinatePrecisionGetFormats(OGRGeomCoordinatePrecisionH hGeomCoordPrec)
{
    VALIDATE_POINTER1(hGeomCoordPrec, "OGRGeomCoordinatePrecisionGetFormats",
                      nullptr);
    CPLStringList aosFormats;
    for (const auto &kv : hGeomCoordPrec->oFormatSpecificOptions)
    {
        aosFormats.AddString(kv.first.c_str());
    }
    return aosFormats.StealList();
}

/************************************************************************/
/*           OGRGeomCoordinatePrecisionGetFormatSpecificOptions()       */
/************************************************************************/

/** Get format specific coordinate precision options.
 *
 * An example of a supported value for pszFormatName is
 * "FileGeodatabase" for layers of the OpenFileGDB driver.
 *
 * @param hGeomCoordPrec OGRGeomCoordinatePrecision instance (must not be null)
 * @param pszFormatName A format name (one of those returned by
 * OGRGeomCoordinatePrecisionGetFormats())
 * @return a null-terminated list, or nullptr. The list must *not* be freed,
 * and is owned by hGeomCoordPrec
 * @since GDAL 3.9
 */
CSLConstList OGRGeomCoordinatePrecisionGetFormatSpecificOptions(
    OGRGeomCoordinatePrecisionH hGeomCoordPrec, const char *pszFormatName)
{
    VALIDATE_POINTER1(hGeomCoordPrec,
                      "OGRGeomCoordinatePrecisionGetFormatSpecificOptions",
                      nullptr);
    const auto oIter =
        hGeomCoordPrec->oFormatSpecificOptions.find(pszFormatName);
    if (oIter == hGeomCoordPrec->oFormatSpecificOptions.end())
    {
        return nullptr;
    }
    return oIter->second.List();
}

/************************************************************************/
/*          OGRGeomCoordinatePrecisionSetFormatSpecificOptions()        */
/************************************************************************/

/** Set format specific coordinate precision options.
 *
 * @param hGeomCoordPrec OGRGeomCoordinatePrecision instance (must not be null)
 * @param pszFormatName A format name (must not be null)
 * @param papszOptions null-terminated list of options.
 * @since GDAL 3.9
 */
void OGRGeomCoordinatePrecisionSetFormatSpecificOptions(
    OGRGeomCoordinatePrecisionH hGeomCoordPrec, const char *pszFormatName,
    CSLConstList papszOptions)
{
    VALIDATE_POINTER0(hGeomCoordPrec,
                      "OGRGeomCoordinatePrecisionSetFormatSpecificOptions");
    hGeomCoordPrec->oFormatSpecificOptions[pszFormatName] = papszOptions;
}

/************************************************************************/
/*                      OGRGeomCoordinatePrecisionSet()                 */
/************************************************************************/

/**
 * \brief Set the resolution of the geometry coordinate components.
 *
 * For the X, Y and Z ordinates, the precision should be expressed in the units
 * of the CRS of the geometry. So typically degrees for geographic CRS, or
 * meters/feet/US-feet for projected CRS.
 * Users might use OGRGeomCoordinatePrecisionSetFromMeter() for an even more
 * convenient interface.
 *
 * For a projected CRS with meters as linear unit, 1e-3 corresponds to a
 * millimetric precision.
 * For a geographic CRS in 8.9e-9 corresponds to a millimetric precision
 * (for a Earth CRS)
 *
 * Resolution should be stricty positive, or set to
 * OGR_GEOM_COORD_PRECISION_UNKNOWN when unknown.
 *
 * @param hGeomCoordPrec OGRGeomCoordinatePrecision instance (must not be null)
 * @param dfXYResolution Resolution for for X and Y coordinates.
 * @param dfZResolution Resolution for for Z coordinates.
 * @param dfMResolution Resolution for for M coordinates.
 * @since GDAL 3.9
 */

void OGRGeomCoordinatePrecisionSet(OGRGeomCoordinatePrecisionH hGeomCoordPrec,
                                   double dfXYResolution, double dfZResolution,
                                   double dfMResolution)
{
    VALIDATE_POINTER0(hGeomCoordPrec, "OGRGeomCoordinatePrecisionSet");
    hGeomCoordPrec->dfXYResolution = dfXYResolution;
    hGeomCoordPrec->dfZResolution = dfZResolution;
    hGeomCoordPrec->dfMResolution = dfMResolution;
}

/************************************************************************/
/*                  OGRGeomCoordinatePrecisionSetFromMeter()            */
/************************************************************************/

/**
 * \brief Set the resolution of the geometry coordinate components.
 *
 * For the X, Y and Z ordinates, the precision should be expressed in meter,
 * e.g 1e-3 for millimetric precision.
 *
 * Resolution should be stricty positive, or set to
 * OGR_GEOM_COORD_PRECISION_UNKNOWN when unknown.
 *
 * @param hGeomCoordPrec OGRGeomCoordinatePrecision instance (must not be null)
 * @param hSRS Spatial reference system, used for metric to SRS unit conversion
 *             (must not be null)
 * @param dfXYMeterResolution Resolution for for X and Y coordinates, in meter.
 * @param dfZMeterResolution Resolution for for Z coordinates, in meter.
 * @param dfMResolution Resolution for for M coordinates.
 * @since GDAL 3.9
 */
void OGRGeomCoordinatePrecisionSetFromMeter(
    OGRGeomCoordinatePrecisionH hGeomCoordPrec, OGRSpatialReferenceH hSRS,
    double dfXYMeterResolution, double dfZMeterResolution, double dfMResolution)
{
    VALIDATE_POINTER0(hGeomCoordPrec, "OGRGeomCoordinatePrecisionSet");
    VALIDATE_POINTER0(hSRS, "OGRGeomCoordinatePrecisionSet");
    return hGeomCoordPrec->SetFromMeter(OGRSpatialReference::FromHandle(hSRS),
                                        dfXYMeterResolution, dfZMeterResolution,
                                        dfMResolution);
}

/************************************************************************/
/*                           GetConversionFactors()                     */
/************************************************************************/

static void GetConversionFactors(const OGRSpatialReference *poSRS,
                                 double &dfXYFactor, double &dfZFactor)
{
    dfXYFactor = 1;
    dfZFactor = 1;

    if (poSRS)
    {
        if (poSRS->IsGeographic())
        {
            dfXYFactor = poSRS->GetSemiMajor(nullptr) * M_PI / 180;
        }
        else
        {
            dfXYFactor = poSRS->GetLinearUnits(nullptr);
        }

        if (poSRS->GetAxesCount() == 3)
        {
            poSRS->GetAxis(nullptr, 2, nullptr, &dfZFactor);
        }
    }
}

/************************************************************************/
/*                OGRGeomCoordinatePrecision::SetFromMeter()            */
/************************************************************************/

/**
 * \brief Set the resolution of the geometry coordinate components.
 *
 * For the X, Y and Z coordinates, the precision should be expressed in meter,
 * e.g 1e-3 for millimetric precision.
 *
 * Resolution should be stricty positive, or set to
 * OGRGeomCoordinatePrecision::UNKNOWN when unknown.
 *
 * @param poSRS Spatial reference system, used for metric to SRS unit conversion
 *              (must not be null)
 * @param dfXYMeterResolution Resolution for for X and Y coordinates, in meter.
 * @param dfZMeterResolution Resolution for for Z coordinates, in meter.
 * @param dfMResolutionIn Resolution for for M coordinates.
 * @since GDAL 3.9
 */
void OGRGeomCoordinatePrecision::SetFromMeter(const OGRSpatialReference *poSRS,
                                              double dfXYMeterResolution,
                                              double dfZMeterResolution,
                                              double dfMResolutionIn)
{
    double dfXYFactor = 1;
    double dfZFactor = 1;
    GetConversionFactors(poSRS, dfXYFactor, dfZFactor);

    dfXYResolution = dfXYMeterResolution / dfXYFactor;
    dfZResolution = dfZMeterResolution / dfZFactor;
    dfMResolution = dfMResolutionIn;
}

/************************************************************************/
/*             OGRGeomCoordinatePrecision::ConvertToOtherSRS()          */
/************************************************************************/

/**
 * \brief Return equivalent coordinate precision setting taking into account
 * a change of SRS.
 *
 * @param poSRSSrc Spatial reference system of the current instance
 *                 (if null, meter unit is assumed)
 * @param poSRSDst Spatial reference system of the returned instance
 *                 (if null, meter unit is assumed)
 * @return a new OGRGeomCoordinatePrecision instance, with a poSRSDst SRS.
 * @since GDAL 3.9
 */
OGRGeomCoordinatePrecision OGRGeomCoordinatePrecision::ConvertToOtherSRS(
    const OGRSpatialReference *poSRSSrc,
    const OGRSpatialReference *poSRSDst) const
{
    double dfXYFactorSrc = 1;
    double dfZFactorSrc = 1;
    GetConversionFactors(poSRSSrc, dfXYFactorSrc, dfZFactorSrc);

    double dfXYFactorDst = 1;
    double dfZFactorDst = 1;
    GetConversionFactors(poSRSDst, dfXYFactorDst, dfZFactorDst);

    OGRGeomCoordinatePrecision oNewPrec;
    oNewPrec.dfXYResolution = dfXYResolution * dfXYFactorSrc / dfXYFactorDst;
    oNewPrec.dfZResolution = dfZResolution * dfZFactorSrc / dfZFactorDst;
    oNewPrec.dfMResolution = dfMResolution;

    // Only preserve source forma specific options if no reprojection is
    // involved
    if ((!poSRSSrc && !poSRSDst) ||
        (poSRSSrc && poSRSDst && poSRSSrc->IsSame(poSRSDst)))
    {
        oNewPrec.oFormatSpecificOptions = oFormatSpecificOptions;
    }

    return oNewPrec;
}

/************************************************************************/
/*           OGRGeomCoordinatePrecision::ResolutionToPrecision()        */
/************************************************************************/

/**
 * \brief Return the number of decimal digits after the decimal point to
 * get the specified resolution.
 *
 * @since GDAL 3.9
 */

/* static */
int OGRGeomCoordinatePrecision::ResolutionToPrecision(double dfResolution)
{
    return static_cast<int>(
        std::ceil(std::log10(1. / std::min(1.0, dfResolution))));
}

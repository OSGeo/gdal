/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGRSpatialReference translation to/from netCDF CF-1 georeferencing.
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam
 * Copyright (c) 2007-2024, Even Rouault <even.rouault at spatialys.com>
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
#include "ogrsf_frmts.h"
#include "ogr_srs_cf1.h"

/************************************************************************/
/*                         OSRImportFromCF1()                           */
/************************************************************************/

/**
 * \brief Import a CRS from netCDF CF-1 definitions.
 *
 * This function is the same as OGRSpatialReference::importFromCF1().
 * @since 3.9
 */

OGRErr OSRImportFromCF1(OGRSpatialReferenceH hSRS, CSLConstList papszKeyValues,
                        const char *pszUnits)

{
    VALIDATE_POINTER1(hSRS, "OSRImportFromCF1", OGRERR_FAILURE);

    return OGRSpatialReference::FromHandle(hSRS)->importFromCF1(papszKeyValues,
                                                                pszUnits);
}

/************************************************************************/
/*                          FetchDoubleParam()                          */
/************************************************************************/

static double FetchDoubleParam(CSLConstList papszKeyValues,
                               const char *pszParam, double dfDefault)

{
    const char *pszValue = CSLFetchNameValue(papszKeyValues, pszParam);
    if (pszValue)
    {
        return CPLAtofM(pszValue);
    }

    return dfDefault;
}

/************************************************************************/
/*                           NCDFTokenizeArray()                        */
/************************************************************************/

// Parse a string, and return as a string list.
// If it an array of the form {a,b}, then tokenize it.
// Otherwise, return a copy.
static char **NCDFTokenizeArray(const char *pszValue)
{
    if (pszValue == nullptr || EQUAL(pszValue, ""))
        return nullptr;

    char **papszValues = nullptr;
    const int nLen = static_cast<int>(strlen(pszValue));

    if (pszValue[0] == '{' && nLen > 2 && pszValue[nLen - 1] == '}')
    {
        char *pszTemp = static_cast<char *>(CPLMalloc((nLen - 2) + 1));
        strncpy(pszTemp, pszValue + 1, nLen - 2);
        pszTemp[nLen - 2] = '\0';
        papszValues = CSLTokenizeString2(pszTemp, ",", CSLT_ALLOWEMPTYTOKENS);
        CPLFree(pszTemp);
    }
    else
    {
        papszValues = reinterpret_cast<char **>(CPLCalloc(2, sizeof(char *)));
        papszValues[0] = CPLStrdup(pszValue);
        papszValues[1] = nullptr;
    }

    return papszValues;
}

/************************************************************************/
/*                           FetchStandardParallels()                   */
/************************************************************************/

static std::vector<std::string>
FetchStandardParallels(CSLConstList papszKeyValues)
{
    // cf-1.0 tags
    const char *pszValue =
        CSLFetchNameValue(papszKeyValues, CF_PP_STD_PARALLEL);

    std::vector<std::string> ret;
    if (pszValue != nullptr)
    {
        CPLStringList aosValues;
        if (pszValue[0] != '{' &&
            (strchr(pszValue, ',') != nullptr ||
             CPLString(pszValue).Trim().find(' ') != std::string::npos))
        {
            // Some files like
            // ftp://data.knmi.nl/download/KNW-NetCDF-3D/1.0/noversion/2013/11/14/KNW-1.0_H37-ERA_NL_20131114.nc
            // do not use standard formatting for arrays, but just space
            // separated syntax
            aosValues = CSLTokenizeString2(pszValue, ", ", 0);
        }
        else
        {
            aosValues = NCDFTokenizeArray(pszValue);
        }
        for (int i = 0; i < aosValues.size(); i++)
        {
            ret.push_back(aosValues[i]);
        }
    }
    // Try gdal tags.
    else
    {
        pszValue = CSLFetchNameValue(papszKeyValues, CF_PP_STD_PARALLEL_1);

        if (pszValue != nullptr)
            ret.push_back(pszValue);

        pszValue = CSLFetchNameValue(papszKeyValues, CF_PP_STD_PARALLEL_2);

        if (pszValue != nullptr)
            ret.push_back(pszValue);
    }

    return ret;
}

/************************************************************************/
/*                          importFromCF1()                             */
/************************************************************************/

/**
 * \brief Import a CRS from netCDF CF-1 definitions.
 *
 * http://cfconventions.org/cf-conventions/cf-conventions.html#appendix-grid-mappings
 *
 * This function is the equivalent of the C function OSRImportFromCF1().
 *
 * @param papszKeyValues Key/value pairs from the grid mapping variable.
 * Multi-valued parameters (typically "standard_parallel") should be comma
 * separated.
 *
 * @param pszUnits Value of the "units" attribute of the X/Y arrays. May be nullptr
 *
 * @return OGRERR_NONE on success or an error code in case of failure.
 * @since 3.9
 */

OGRErr OGRSpatialReference::importFromCF1(CSLConstList papszKeyValues,
                                          const char *pszUnits)
{
    // Import from "spatial_ref" or "crs_wkt" attributes in priority
    const char *pszWKT = CSLFetchNameValue(papszKeyValues, NCDF_SPATIAL_REF);
    if (!pszWKT)
    {
        pszWKT = CSLFetchNameValue(papszKeyValues, NCDF_CRS_WKT);
    }
    if (pszWKT)
        return importFromWkt(pszWKT);

    const char *pszGridMappingName =
        CSLFetchNameValue(papszKeyValues, CF_GRD_MAPPING_NAME);

    // Some files such as
    // http://www.ecad.eu/download/ensembles/data/Grid_0.44deg_rot/tg_0.44deg_rot_v16.0.nc.gz
    // lack an explicit projection_var:grid_mapping_name attribute
    if (pszGridMappingName == nullptr &&
        CSLFetchNameValue(papszKeyValues, CF_PP_GRID_NORTH_POLE_LONGITUDE) !=
            nullptr)
    {
        pszGridMappingName = CF_PT_ROTATED_LATITUDE_LONGITUDE;
    }

    if (pszGridMappingName == nullptr)
        return OGRERR_FAILURE;

    bool bGotGeogCS = false;

    // Check for datum/spheroid information.
    double dfEarthRadius =
        FetchDoubleParam(papszKeyValues, CF_PP_EARTH_RADIUS, -1.0);

    const double dfLonPrimeMeridian =
        FetchDoubleParam(papszKeyValues, CF_PP_LONG_PRIME_MERIDIAN, 0.0);

    const char *pszPMName =
        CSLFetchNameValue(papszKeyValues, CF_PRIME_MERIDIAN_NAME);

    // Should try to find PM name from its value if not Greenwich.
    if (pszPMName == nullptr && !CPLIsEqual(dfLonPrimeMeridian, 0.0))
        pszPMName = "unknown";

    double dfInverseFlattening =
        FetchDoubleParam(papszKeyValues, CF_PP_INVERSE_FLATTENING, -1.0);

    double dfSemiMajorAxis =
        FetchDoubleParam(papszKeyValues, CF_PP_SEMI_MAJOR_AXIS, -1.0);

    const double dfSemiMinorAxis =
        FetchDoubleParam(papszKeyValues, CF_PP_SEMI_MINOR_AXIS, -1.0);

    // See if semi-major exists if radius doesn't.
    if (dfEarthRadius < 0.0)
        dfEarthRadius = dfSemiMajorAxis;

    // If still no radius, check old tag.
    if (dfEarthRadius < 0.0)
        dfEarthRadius =
            FetchDoubleParam(papszKeyValues, CF_PP_EARTH_RADIUS_OLD, -1.0);

    const char *pszEllipsoidName =
        CSLFetchNameValue(papszKeyValues, CF_REFERENCE_ELLIPSOID_NAME);

    const char *pszDatumName =
        CSLFetchNameValue(papszKeyValues, CF_HORIZONTAL_DATUM_NAME);

    const char *pszGeogName =
        CSLFetchNameValue(papszKeyValues, CF_GEOGRAPHIC_CRS_NAME);
    if (pszGeogName == nullptr)
        pszGeogName = "unknown";

    bool bRotatedPole = false;

    // Has radius value.
    if (dfEarthRadius > 0.0)
    {
        // Check for inv_flat tag.
        if (dfInverseFlattening < 0.0)
        {
            // No inv_flat tag, check for semi_minor.
            if (dfSemiMinorAxis < 0.0)
            {
                // No way to get inv_flat, use sphere.
                SetGeogCS(pszGeogName, pszDatumName,
                          pszEllipsoidName ? pszEllipsoidName : "Sphere",
                          dfEarthRadius, 0.0, pszPMName, dfLonPrimeMeridian);
                bGotGeogCS = true;
            }
            else
            {
                if (dfSemiMajorAxis < 0.0)
                    dfSemiMajorAxis = dfEarthRadius;
                // set inv_flat using semi_minor/major
                dfInverseFlattening =
                    OSRCalcInvFlattening(dfSemiMajorAxis, dfSemiMinorAxis);

                SetGeogCS(pszGeogName, pszDatumName,
                          pszEllipsoidName ? pszEllipsoidName : "Spheroid",
                          dfEarthRadius, dfInverseFlattening, pszPMName,
                          dfLonPrimeMeridian);
                bGotGeogCS = true;
            }
        }
        else
        {
            SetGeogCS(pszGeogName, pszDatumName,
                      pszEllipsoidName ? pszEllipsoidName : "Spheroid",
                      dfEarthRadius, dfInverseFlattening, pszPMName,
                      dfLonPrimeMeridian);
            bGotGeogCS = true;
        }

        if (bGotGeogCS)
            CPLDebug("GDAL_netCDF", "got spheroid from CF: (%f , %f)",
                     dfEarthRadius, dfInverseFlattening);
    }

    // Transverse Mercator.
    if (EQUAL(pszGridMappingName, CF_PT_TM))
    {
        const double dfScale =
            FetchDoubleParam(papszKeyValues, CF_PP_SCALE_FACTOR_MERIDIAN, 1.0);

        const double dfCenterLon =
            FetchDoubleParam(papszKeyValues, CF_PP_LONG_CENTRAL_MERIDIAN, 0.0);

        const double dfCenterLat =
            FetchDoubleParam(papszKeyValues, CF_PP_LAT_PROJ_ORIGIN, 0.0);

        const double dfFalseEasting =
            FetchDoubleParam(papszKeyValues, CF_PP_FALSE_EASTING, 0.0);

        const double dfFalseNorthing =
            FetchDoubleParam(papszKeyValues, CF_PP_FALSE_NORTHING, 0.0);

        SetTM(dfCenterLat, dfCenterLon, dfScale, dfFalseEasting,
              dfFalseNorthing);

        if (!bGotGeogCS)
            SetWellKnownGeogCS("WGS84");
    }

    // Albers Equal Area.
    if (EQUAL(pszGridMappingName, CF_PT_AEA))
    {
        const double dfCenterLon =
            FetchDoubleParam(papszKeyValues, CF_PP_LONG_CENTRAL_MERIDIAN, 0.0);

        const double dfFalseEasting =
            FetchDoubleParam(papszKeyValues, CF_PP_FALSE_EASTING, 0.0);

        const double dfFalseNorthing =
            FetchDoubleParam(papszKeyValues, CF_PP_FALSE_NORTHING, 0.0);

        const auto aosStdParallels = FetchStandardParallels(papszKeyValues);

        double dfStdP1 = 0;
        double dfStdP2 = 0;
        if (aosStdParallels.size() == 1)
        {
            // TODO CF-1 standard says it allows AEA to be encoded
            // with only 1 standard parallel.  How should this
            // actually map to a 2StdP OGC WKT version?
            CPLError(CE_Warning, CPLE_NotSupported,
                     "NetCDF driver import of AEA-1SP is not tested, "
                     "using identical std. parallels.");
            dfStdP1 = CPLAtofM(aosStdParallels[0].c_str());
            dfStdP2 = dfStdP1;
        }
        else if (aosStdParallels.size() == 2)
        {
            dfStdP1 = CPLAtofM(aosStdParallels[0].c_str());
            dfStdP2 = CPLAtofM(aosStdParallels[1].c_str());
        }
        // Old default.
        else
        {
            dfStdP1 =
                FetchDoubleParam(papszKeyValues, CF_PP_STD_PARALLEL_1, 0.0);

            dfStdP2 =
                FetchDoubleParam(papszKeyValues, CF_PP_STD_PARALLEL_2, 0.0);
        }

        const double dfCenterLat =
            FetchDoubleParam(papszKeyValues, CF_PP_LAT_PROJ_ORIGIN, 0.0);

        SetACEA(dfStdP1, dfStdP2, dfCenterLat, dfCenterLon, dfFalseEasting,
                dfFalseNorthing);

        if (!bGotGeogCS)
            SetWellKnownGeogCS("WGS84");
    }

    // Cylindrical Equal Area
    else if (EQUAL(pszGridMappingName, CF_PT_CEA) ||
             EQUAL(pszGridMappingName, CF_PT_LCEA))
    {
        const auto aosStdParallels = FetchStandardParallels(papszKeyValues);

        double dfStdP1 = 0;
        if (!aosStdParallels.empty())
        {
            dfStdP1 = CPLAtofM(aosStdParallels[0].c_str());
        }
        else
        {
            // TODO: Add support for 'scale_factor_at_projection_origin'
            // variant to standard parallel.  Probably then need to calc
            // a std parallel equivalent.
            CPLError(CE_Failure, CPLE_NotSupported,
                     "NetCDF driver does not support import of CF-1 LCEA "
                     "'scale_factor_at_projection_origin' variant yet.");
        }

        const double dfCentralMeridian =
            FetchDoubleParam(papszKeyValues, CF_PP_LONG_CENTRAL_MERIDIAN, 0.0);

        const double dfFalseEasting =
            FetchDoubleParam(papszKeyValues, CF_PP_FALSE_EASTING, 0.0);

        const double dfFalseNorthing =
            FetchDoubleParam(papszKeyValues, CF_PP_FALSE_NORTHING, 0.0);

        SetCEA(dfStdP1, dfCentralMeridian, dfFalseEasting, dfFalseNorthing);

        if (!bGotGeogCS)
            SetWellKnownGeogCS("WGS84");
    }

    // lambert_azimuthal_equal_area.
    else if (EQUAL(pszGridMappingName, CF_PT_LAEA))
    {
        const double dfCenterLon =
            FetchDoubleParam(papszKeyValues, CF_PP_LON_PROJ_ORIGIN, 0.0);

        const double dfCenterLat =
            FetchDoubleParam(papszKeyValues, CF_PP_LAT_PROJ_ORIGIN, 0.0);

        const double dfFalseEasting =
            FetchDoubleParam(papszKeyValues, CF_PP_FALSE_EASTING, 0.0);

        const double dfFalseNorthing =
            FetchDoubleParam(papszKeyValues, CF_PP_FALSE_NORTHING, 0.0);

        SetLAEA(dfCenterLat, dfCenterLon, dfFalseEasting, dfFalseNorthing);

        if (!bGotGeogCS)
            SetWellKnownGeogCS("WGS84");

        if (GetAttrValue("DATUM") != nullptr &&
            EQUAL(GetAttrValue("DATUM"), "WGS_1984"))
        {
            SetProjCS("LAEA (WGS84)");
        }
    }

    // Azimuthal Equidistant.
    else if (EQUAL(pszGridMappingName, CF_PT_AE))
    {
        const double dfCenterLon =
            FetchDoubleParam(papszKeyValues, CF_PP_LON_PROJ_ORIGIN, 0.0);

        const double dfCenterLat =
            FetchDoubleParam(papszKeyValues, CF_PP_LAT_PROJ_ORIGIN, 0.0);

        const double dfFalseEasting =
            FetchDoubleParam(papszKeyValues, CF_PP_FALSE_EASTING, 0.0);

        const double dfFalseNorthing =
            FetchDoubleParam(papszKeyValues, CF_PP_FALSE_NORTHING, 0.0);

        SetAE(dfCenterLat, dfCenterLon, dfFalseEasting, dfFalseNorthing);

        if (!bGotGeogCS)
            SetWellKnownGeogCS("WGS84");
    }

    // Lambert conformal conic.
    else if (EQUAL(pszGridMappingName, CF_PT_LCC))
    {
        const double dfCenterLon =
            FetchDoubleParam(papszKeyValues, CF_PP_LONG_CENTRAL_MERIDIAN, 0.0);

        const double dfCenterLat =
            FetchDoubleParam(papszKeyValues, CF_PP_LAT_PROJ_ORIGIN, 0.0);

        const double dfFalseEasting =
            FetchDoubleParam(papszKeyValues, CF_PP_FALSE_EASTING, 0.0);

        const double dfFalseNorthing =
            FetchDoubleParam(papszKeyValues, CF_PP_FALSE_NORTHING, 0.0);

        const auto aosStdParallels = FetchStandardParallels(papszKeyValues);

        // 2SP variant.
        if (aosStdParallels.size() == 2)
        {
            const double dfStdP1 = CPLAtofM(aosStdParallels[0].c_str());
            const double dfStdP2 = CPLAtofM(aosStdParallels[1].c_str());
            SetLCC(dfStdP1, dfStdP2, dfCenterLat, dfCenterLon, dfFalseEasting,
                   dfFalseNorthing);
        }
        // 1SP variant (with standard_parallel or center lon).
        // See comments in netcdfdataset.h for this projection.
        else
        {
            double dfScale = FetchDoubleParam(papszKeyValues,
                                              CF_PP_SCALE_FACTOR_ORIGIN, -1.0);

            // CF definition, without scale factor.
            if (CPLIsEqual(dfScale, -1.0))
            {
                double dfStdP1;
                // With standard_parallel.
                if (aosStdParallels.size() == 1)
                    dfStdP1 = CPLAtofM(aosStdParallels[0].c_str());
                // With center lon instead.
                else
                    dfStdP1 = dfCenterLat;
                // dfStdP2 = dfStdP1;

                // Test if we should actually compute scale factor.
                if (!CPLIsEqual(dfStdP1, dfCenterLat))
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                             "NetCDF driver import of LCC-1SP with "
                             "standard_parallel1 != "
                             "latitude_of_projection_origin "
                             "(which forces a computation of scale_factor) "
                             "is experimental (bug #3324)");
                    // Use Snyder eq. 15-4 to compute dfScale from
                    // dfStdP1 and dfCenterLat.  Only tested for
                    // dfStdP1=dfCenterLat and (25,26), needs more data
                    // for testing.  Other option: use the 2SP variant -
                    // how to compute new standard parallels?
                    dfScale =
                        (cos(dfStdP1) *
                         pow(tan(M_PI / 4 + dfStdP1 / 2), sin(dfStdP1))) /
                        (cos(dfCenterLat) * pow(tan(M_PI / 4 + dfCenterLat / 2),
                                                sin(dfCenterLat)));
                }
                // Default is 1.0.
                else
                {
                    dfScale = 1.0;
                }

                SetLCC1SP(dfCenterLat, dfCenterLon, dfScale, dfFalseEasting,
                          dfFalseNorthing);
                // Store dfStdP1 so we can output it to CF later.
                SetNormProjParm(SRS_PP_STANDARD_PARALLEL_1, dfStdP1);
            }
            // OGC/PROJ.4 definition with scale factor.
            else
            {
                SetLCC1SP(dfCenterLat, dfCenterLon, dfScale, dfFalseEasting,
                          dfFalseNorthing);
            }
        }

        if (!bGotGeogCS)
            SetWellKnownGeogCS("WGS84");
    }

    // Is this Latitude/Longitude Grid explicitly?

    else if (EQUAL(pszGridMappingName, CF_PT_LATITUDE_LONGITUDE))
    {
        if (!bGotGeogCS)
            SetWellKnownGeogCS("WGS84");
    }

    // Mercator.
    else if (EQUAL(pszGridMappingName, CF_PT_MERCATOR))
    {

        // If there is a standard_parallel, know it is Mercator 2SP.
        const auto aosStdParallels = FetchStandardParallels(papszKeyValues);

        if (!aosStdParallels.empty())
        {
            // CF-1 Mercator 2SP always has lat centered at equator.
            const double dfStdP1 = CPLAtofM(aosStdParallels[0].c_str());

            const double dfCenterLat = 0.0;

            const double dfCenterLon =
                FetchDoubleParam(papszKeyValues, CF_PP_LON_PROJ_ORIGIN, 0.0);

            const double dfFalseEasting =
                FetchDoubleParam(papszKeyValues, CF_PP_FALSE_EASTING, 0.0);

            const double dfFalseNorthing =
                FetchDoubleParam(papszKeyValues, CF_PP_FALSE_NORTHING, 0.0);

            SetMercator2SP(dfStdP1, dfCenterLat, dfCenterLon, dfFalseEasting,
                           dfFalseNorthing);
        }
        else
        {
            const double dfCenterLon =
                FetchDoubleParam(papszKeyValues, CF_PP_LON_PROJ_ORIGIN, 0.0);

            const double dfCenterLat =
                FetchDoubleParam(papszKeyValues, CF_PP_LAT_PROJ_ORIGIN, 0.0);

            const double dfScale = FetchDoubleParam(
                papszKeyValues, CF_PP_SCALE_FACTOR_ORIGIN, 1.0);

            const double dfFalseEasting =
                FetchDoubleParam(papszKeyValues, CF_PP_FALSE_EASTING, 0.0);

            const double dfFalseNorthing =
                FetchDoubleParam(papszKeyValues, CF_PP_FALSE_NORTHING, 0.0);

            SetMercator(dfCenterLat, dfCenterLon, dfScale, dfFalseEasting,
                        dfFalseNorthing);
        }

        if (!bGotGeogCS)
            SetWellKnownGeogCS("WGS84");
    }

    // Orthographic.
    else if (EQUAL(pszGridMappingName, CF_PT_ORTHOGRAPHIC))
    {
        const double dfCenterLon =
            FetchDoubleParam(papszKeyValues, CF_PP_LON_PROJ_ORIGIN, 0.0);

        const double dfCenterLat =
            FetchDoubleParam(papszKeyValues, CF_PP_LAT_PROJ_ORIGIN, 0.0);

        const double dfFalseEasting =
            FetchDoubleParam(papszKeyValues, CF_PP_FALSE_EASTING, 0.0);

        const double dfFalseNorthing =
            FetchDoubleParam(papszKeyValues, CF_PP_FALSE_NORTHING, 0.0);

        SetOrthographic(dfCenterLat, dfCenterLon, dfFalseEasting,
                        dfFalseNorthing);

        if (!bGotGeogCS)
            SetWellKnownGeogCS("WGS84");
    }

    // Polar Stereographic.
    else if (EQUAL(pszGridMappingName, CF_PT_POLAR_STEREO))
    {
        const auto aosStdParallels = FetchStandardParallels(papszKeyValues);

        const double dfCenterLon =
            FetchDoubleParam(papszKeyValues, CF_PP_VERT_LONG_FROM_POLE, 0.0);

        const double dfFalseEasting =
            FetchDoubleParam(papszKeyValues, CF_PP_FALSE_EASTING, 0.0);

        const double dfFalseNorthing =
            FetchDoubleParam(papszKeyValues, CF_PP_FALSE_NORTHING, 0.0);

        // CF allows the use of standard_parallel (lat_ts) OR
        // scale_factor (k0), make sure we have standard_parallel, using
        // Snyder eq. 22-7 with k=1 and lat=standard_parallel.
        if (!aosStdParallels.empty())
        {
            const double dfStdP1 = CPLAtofM(aosStdParallels[0].c_str());

            // Polar Stereographic Variant B with latitude of standard
            // parallel
            SetPS(dfStdP1, dfCenterLon, 1.0, dfFalseEasting, dfFalseNorthing);
        }
        else
        {
            // Fetch latitude_of_projection_origin (+90/-90).
            double dfLatProjOrigin =
                FetchDoubleParam(papszKeyValues, CF_PP_LAT_PROJ_ORIGIN, 0.0);
            if (!CPLIsEqual(dfLatProjOrigin, 90.0) &&
                !CPLIsEqual(dfLatProjOrigin, -90.0))
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Polar Stereographic must have a %s "
                         "parameter equal to +90 or -90.",
                         CF_PP_LAT_PROJ_ORIGIN);
                dfLatProjOrigin = 90.0;
            }

            const double dfScale = FetchDoubleParam(
                papszKeyValues, CF_PP_SCALE_FACTOR_ORIGIN, 1.0);

            // Polar Stereographic Variant A with scale factor at
            // natural origin and latitude of origin = +/- 90
            SetPS(dfLatProjOrigin, dfCenterLon, dfScale, dfFalseEasting,
                  dfFalseNorthing);
        }

        if (!bGotGeogCS)
            SetWellKnownGeogCS("WGS84");
    }

    // Stereographic.
    else if (EQUAL(pszGridMappingName, CF_PT_STEREO))
    {
        const double dfCenterLon =
            FetchDoubleParam(papszKeyValues, CF_PP_LON_PROJ_ORIGIN, 0.0);

        const double dfCenterLat =
            FetchDoubleParam(papszKeyValues, CF_PP_LAT_PROJ_ORIGIN, 0.0);

        const double dfScale =
            FetchDoubleParam(papszKeyValues, CF_PP_SCALE_FACTOR_ORIGIN, 1.0);

        const double dfFalseEasting =
            FetchDoubleParam(papszKeyValues, CF_PP_FALSE_EASTING, 0.0);

        const double dfFalseNorthing =
            FetchDoubleParam(papszKeyValues, CF_PP_FALSE_NORTHING, 0.0);

        SetStereographic(dfCenterLat, dfCenterLon, dfScale, dfFalseEasting,
                         dfFalseNorthing);

        if (!bGotGeogCS)
            SetWellKnownGeogCS("WGS84");
    }

    // Geostationary.
    else if (EQUAL(pszGridMappingName, CF_PT_GEOS))
    {
        const double dfCenterLon =
            FetchDoubleParam(papszKeyValues, CF_PP_LON_PROJ_ORIGIN, 0.0);

        const double dfSatelliteHeight = FetchDoubleParam(
            papszKeyValues, CF_PP_PERSPECTIVE_POINT_HEIGHT, 35785831.0);

        const char *pszSweepAxisAngle =
            CSLFetchNameValue(papszKeyValues, CF_PP_SWEEP_ANGLE_AXIS);

        const double dfFalseEasting =
            FetchDoubleParam(papszKeyValues, CF_PP_FALSE_EASTING, 0.0);

        const double dfFalseNorthing =
            FetchDoubleParam(papszKeyValues, CF_PP_FALSE_NORTHING, 0.0);

        SetGEOS(dfCenterLon, dfSatelliteHeight, dfFalseEasting,
                dfFalseNorthing);

        if (!bGotGeogCS)
            SetWellKnownGeogCS("WGS84");

        if (pszSweepAxisAngle != nullptr && EQUAL(pszSweepAxisAngle, "x"))
        {
            char *pszProj4 = nullptr;
            exportToProj4(&pszProj4);
            CPLString osProj4 = pszProj4;
            osProj4 += " +sweep=x";
            SetExtension(GetRoot()->GetValue(), "PROJ4", osProj4);
            CPLFree(pszProj4);
        }
    }

    else if (EQUAL(pszGridMappingName, CF_PT_ROTATED_LATITUDE_LONGITUDE))
    {
        const double dfGridNorthPoleLong = FetchDoubleParam(
            papszKeyValues, CF_PP_GRID_NORTH_POLE_LONGITUDE, 0.0);
        const double dfGridNorthPoleLat = FetchDoubleParam(
            papszKeyValues, CF_PP_GRID_NORTH_POLE_LATITUDE, 0.0);
        const double dfNorthPoleGridLong = FetchDoubleParam(
            papszKeyValues, CF_PP_NORTH_POLE_GRID_LONGITUDE, 0.0);

        bRotatedPole = true;
        SetDerivedGeogCRSWithPoleRotationNetCDFCFConvention(
            "Rotated_pole", dfGridNorthPoleLat, dfGridNorthPoleLong,
            dfNorthPoleGridLong);
    }

    if (IsProjected())
    {
        const char *pszProjectedCRSName =
            CSLFetchNameValue(papszKeyValues, CF_PROJECTED_CRS_NAME);
        if (pszProjectedCRSName)
            SetProjCS(pszProjectedCRSName);
    }

    // Add units to PROJCS.
    if (IsGeographic() && !bRotatedPole)
    {
        SetAngularUnits(SRS_UA_DEGREE, CPLAtof(SRS_UA_DEGREE_CONV));
        SetAuthority("GEOGCS|UNIT", "EPSG", 9122);
    }
    else if (pszUnits != nullptr && !EQUAL(pszUnits, ""))
    {
        if (EQUAL(pszUnits, "m") || EQUAL(pszUnits, "metre") ||
            EQUAL(pszUnits, "meter"))
        {
            SetLinearUnits("metre", 1.0);
            SetAuthority("PROJCS|UNIT", "EPSG", 9001);
        }
        else if (EQUAL(pszUnits, "km"))
        {
            SetLinearUnits("kilometre", 1000.0);
            SetAuthority("PROJCS|UNIT", "EPSG", 9036);
        }
        else if (EQUAL(pszUnits, "US_survey_foot") ||
                 EQUAL(pszUnits, "US_survey_feet"))
        {
            SetLinearUnits("US survey foot", CPLAtof(SRS_UL_US_FOOT_CONV));
            SetAuthority("PROJCS|UNIT", "EPSG", 9003);
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Unhandled X/Y axis unit %s. SRS will ignore "
                     "axis unit and be likely wrong.",
                     pszUnits);
        }
    }

    return OGRERR_NONE;
}

/* -------------------------------------------------------------------- */
/*         CF-1 to GDAL mappings                                        */
/* -------------------------------------------------------------------- */

/* Following are a series of mappings from CF-1 convention parameters
 * for each projection, to the equivalent in OGC WKT used internally by GDAL.
 * See: http://cf-pcmdi.llnl.gov/documents/cf-conventions/1.5/apf.html
 */

/* A struct allowing us to map between GDAL(OGC WKT) and CF-1 attributes */
typedef struct
{
    const char *CF_ATT;
    const char *WKT_ATT;
    // TODO: mappings may need default values, like scale factor?
    // double defval;
} oNetcdfSRS_PP;

// default mappings, for the generic case
/* These 'generic' mappings are based on what was previously in the
   poNetCDFSRS struct. They will be used as a fallback in case none
   of the others match (i.e. you are exporting a projection that has
   no CF-1 equivalent).
   They are not used for known CF-1 projections since there is not a
   unique 2-way projection-independent
   mapping between OGC WKT params and CF-1 ones: it varies per-projection.
*/

static const oNetcdfSRS_PP poGenericMappings[] = {
    /* scale_factor is handled as a special case, write 2 values */
    {CF_PP_STD_PARALLEL_1, SRS_PP_STANDARD_PARALLEL_1},
    {CF_PP_STD_PARALLEL_2, SRS_PP_STANDARD_PARALLEL_2},
    {CF_PP_LONG_CENTRAL_MERIDIAN, SRS_PP_CENTRAL_MERIDIAN},
    {CF_PP_LONG_CENTRAL_MERIDIAN, SRS_PP_LONGITUDE_OF_CENTER},
    {CF_PP_LON_PROJ_ORIGIN, SRS_PP_LONGITUDE_OF_ORIGIN},
    // Multiple mappings to LAT_PROJ_ORIGIN
    {CF_PP_LAT_PROJ_ORIGIN, SRS_PP_LATITUDE_OF_ORIGIN},
    {CF_PP_LAT_PROJ_ORIGIN, SRS_PP_LATITUDE_OF_CENTER},
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING},
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING},
    {nullptr, nullptr},
};

// Albers equal area
//
// grid_mapping_name = albers_conical_equal_area
// WKT: Albers_Conic_Equal_Area
// EPSG:9822
//
// Map parameters:
//
//    * standard_parallel - There may be 1 or 2 values.
//    * longitude_of_central_meridian
//    * latitude_of_projection_origin
//    * false_easting
//    * false_northing
//
static const oNetcdfSRS_PP poAEAMappings[] = {
    {CF_PP_STD_PARALLEL_1, SRS_PP_STANDARD_PARALLEL_1},
    {CF_PP_STD_PARALLEL_2, SRS_PP_STANDARD_PARALLEL_2},
    {CF_PP_LAT_PROJ_ORIGIN, SRS_PP_LATITUDE_OF_CENTER},
    {CF_PP_LONG_CENTRAL_MERIDIAN, SRS_PP_LONGITUDE_OF_CENTER},
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING},
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING},
    {nullptr, nullptr}};

// Azimuthal equidistant
//
// grid_mapping_name = azimuthal_equidistant
// WKT: Azimuthal_Equidistant
//
// Map parameters:
//
//    * longitude_of_projection_origin
//    * latitude_of_projection_origin
//    * false_easting
//    * false_northing
//
static const oNetcdfSRS_PP poAEMappings[] = {
    {CF_PP_LAT_PROJ_ORIGIN, SRS_PP_LATITUDE_OF_CENTER},
    {CF_PP_LON_PROJ_ORIGIN, SRS_PP_LONGITUDE_OF_CENTER},
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING},
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING},
    {nullptr, nullptr}};

// Lambert azimuthal equal area
//
// grid_mapping_name = lambert_azimuthal_equal_area
// WKT: Lambert_Azimuthal_Equal_Area
//
// Map parameters:
//
//    * longitude_of_projection_origin
//    * latitude_of_projection_origin
//    * false_easting
//    * false_northing
//
static const oNetcdfSRS_PP poLAEAMappings[] = {
    {CF_PP_LAT_PROJ_ORIGIN, SRS_PP_LATITUDE_OF_CENTER},
    {CF_PP_LON_PROJ_ORIGIN, SRS_PP_LONGITUDE_OF_CENTER},
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING},
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING},
    {nullptr, nullptr}};

// Lambert conformal
//
// grid_mapping_name = lambert_conformal_conic
// WKT: Lambert_Conformal_Conic_1SP / Lambert_Conformal_Conic_2SP
//
// Map parameters:
//
//    * standard_parallel - There may be 1 or 2 values.
//    * longitude_of_central_meridian
//    * latitude_of_projection_origin
//    * false_easting
//    * false_northing
//
// See
// http://www.remotesensing.org/geotiff/proj_list/lambert_conic_conformal_1sp.html

// Lambert conformal conic - 1SP
/* See bug # 3324
   It seems that the missing scale factor can be computed from
   standard_parallel1 and latitude_of_projection_origin. If both are equal (the
   common case) then scale factor=1, else use Snyder eq. 15-4. We save in the
   WKT standard_parallel1 for export to CF, but do not export scale factor. If a
   WKT has a scale factor != 1 and no standard_parallel1 then export is not CF,
   but we output scale factor for compat. is there a formula for that?
*/
static const oNetcdfSRS_PP poLCC1SPMappings[] = {
    {CF_PP_STD_PARALLEL_1, SRS_PP_STANDARD_PARALLEL_1},
    {CF_PP_LAT_PROJ_ORIGIN, SRS_PP_LATITUDE_OF_ORIGIN},
    {CF_PP_LONG_CENTRAL_MERIDIAN, SRS_PP_CENTRAL_MERIDIAN},
    {CF_PP_SCALE_FACTOR_ORIGIN, SRS_PP_SCALE_FACTOR}, /* special case */
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING},
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING},
    {nullptr, nullptr}};

// Lambert conformal conic - 2SP
static const oNetcdfSRS_PP poLCC2SPMappings[] = {
    {CF_PP_STD_PARALLEL_1, SRS_PP_STANDARD_PARALLEL_1},
    {CF_PP_STD_PARALLEL_2, SRS_PP_STANDARD_PARALLEL_2},
    {CF_PP_LAT_PROJ_ORIGIN, SRS_PP_LATITUDE_OF_ORIGIN},
    {CF_PP_LONG_CENTRAL_MERIDIAN, SRS_PP_CENTRAL_MERIDIAN},
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING},
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING},
    {nullptr, nullptr}};

// Lambert cylindrical equal area
//
// grid_mapping_name = lambert_cylindrical_equal_area
// WKT: Cylindrical_Equal_Area
// EPSG:9834 (Spherical) and EPSG:9835
//
// Map parameters:
//
//    * longitude_of_central_meridian
//    * either standard_parallel or scale_factor_at_projection_origin
//    * false_easting
//    * false_northing
//
// NB: CF-1 specifies a 'scale_factor_at_projection' alternative
//  to std_parallel ... but no reference to this in EPSG/remotesensing.org
//  ignore for now.
//
static const oNetcdfSRS_PP poLCEAMappings[] = {
    {CF_PP_STD_PARALLEL_1, SRS_PP_STANDARD_PARALLEL_1},
    {CF_PP_LONG_CENTRAL_MERIDIAN, SRS_PP_CENTRAL_MERIDIAN},
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING},
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING},
    {nullptr, nullptr}};

// Latitude-Longitude
//
// grid_mapping_name = latitude_longitude
//
// Map parameters:
//
//    * None
//
// NB: handled as a special case - !isProjected()

// Mercator
//
// grid_mapping_name = mercator
// WKT: Mercator_1SP / Mercator_2SP
//
// Map parameters:
//
//    * longitude_of_projection_origin
//    * either standard_parallel or scale_factor_at_projection_origin
//    * false_easting
//    * false_northing

// Mercator 1 Standard Parallel (EPSG:9804)
static const oNetcdfSRS_PP poM1SPMappings[] = {
    {CF_PP_LON_PROJ_ORIGIN, SRS_PP_CENTRAL_MERIDIAN},
    // LAT_PROJ_ORIGIN is always equator (0) in CF-1
    {CF_PP_SCALE_FACTOR_ORIGIN, SRS_PP_SCALE_FACTOR},
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING},
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING},
    {nullptr, nullptr}};

// Mercator 2 Standard Parallel
static const oNetcdfSRS_PP poM2SPMappings[] = {
    {CF_PP_LON_PROJ_ORIGIN, SRS_PP_CENTRAL_MERIDIAN},
    {CF_PP_STD_PARALLEL_1, SRS_PP_STANDARD_PARALLEL_1},
    // From best understanding of this projection, only
    // actually specify one SP - it is the same N/S of equator.
    // {CF_PP_STD_PARALLEL_2, SRS_PP_LATITUDE_OF_ORIGIN},
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING},
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING},
    {nullptr, nullptr}};

// Orthographic
// grid_mapping_name = orthographic
// WKT: Orthographic
//
// Map parameters:
//
//    * longitude_of_projection_origin
//    * latitude_of_projection_origin
//    * false_easting
//    * false_northing
//
static const oNetcdfSRS_PP poOrthoMappings[] = {
    {CF_PP_LAT_PROJ_ORIGIN, SRS_PP_LATITUDE_OF_ORIGIN},
    {CF_PP_LON_PROJ_ORIGIN, SRS_PP_CENTRAL_MERIDIAN},
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING},
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING},
    {nullptr, nullptr}};

// Polar stereographic
//
// grid_mapping_name = polar_stereographic
// WKT: Polar_Stereographic
//
// Map parameters:
//
//    * straight_vertical_longitude_from_pole
//    * latitude_of_projection_origin - Either +90. or -90.
//    * Either standard_parallel or scale_factor_at_projection_origin
//    * false_easting
//    * false_northing

static const oNetcdfSRS_PP poPSmappings[] = {
    /* {CF_PP_STD_PARALLEL_1, SRS_PP_LATITUDE_OF_ORIGIN}, */
    /* {CF_PP_SCALE_FACTOR_ORIGIN, SRS_PP_SCALE_FACTOR},   */
    {CF_PP_VERT_LONG_FROM_POLE, SRS_PP_CENTRAL_MERIDIAN},
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING},
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING},
    {nullptr, nullptr}};

// Rotated Pole
//
// grid_mapping_name = rotated_latitude_longitude
// WKT: N/A
//
// Map parameters:
//
//    * grid_north_pole_latitude
//    * grid_north_pole_longitude
//    * north_pole_grid_longitude - This parameter is optional (default is 0.).

// No WKT equivalent

// Stereographic
//
// grid_mapping_name = stereographic
// WKT: Stereographic (and/or Oblique_Stereographic??)
//
// Map parameters:
//
//    * longitude_of_projection_origin
//    * latitude_of_projection_origin
//    * scale_factor_at_projection_origin
//    * false_easting
//    * false_northing
//
// NB: see bug#4267 Stereographic vs. Oblique_Stereographic
//
static const oNetcdfSRS_PP poStMappings[] = {
    {CF_PP_LAT_PROJ_ORIGIN, SRS_PP_LATITUDE_OF_ORIGIN},
    {CF_PP_LON_PROJ_ORIGIN, SRS_PP_CENTRAL_MERIDIAN},
    {CF_PP_SCALE_FACTOR_ORIGIN, SRS_PP_SCALE_FACTOR},
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING},
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING},
    {nullptr, nullptr}};

// Transverse Mercator
//
// grid_mapping_name = transverse_mercator
// WKT: Transverse_Mercator
//
// Map parameters:
//
//    * scale_factor_at_central_meridian
//    * longitude_of_central_meridian
//    * latitude_of_projection_origin
//    * false_easting
//    * false_northing
//
static const oNetcdfSRS_PP poTMMappings[] = {
    {CF_PP_SCALE_FACTOR_MERIDIAN, SRS_PP_SCALE_FACTOR},
    {CF_PP_LONG_CENTRAL_MERIDIAN, SRS_PP_CENTRAL_MERIDIAN},
    {CF_PP_LAT_PROJ_ORIGIN, SRS_PP_LATITUDE_OF_ORIGIN},
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING},
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING},
    {nullptr, nullptr}};

// Vertical perspective
//
// grid_mapping_name = vertical_perspective
// WKT: ???
//
// Map parameters:
//
//    * latitude_of_projection_origin
//    * longitude_of_projection_origin
//    * perspective_point_height
//    * false_easting
//    * false_northing
//
// TODO: see how to map this to OGR

static const oNetcdfSRS_PP poGEOSMappings[] = {
    {CF_PP_LON_PROJ_ORIGIN, SRS_PP_CENTRAL_MERIDIAN},
    {CF_PP_PERSPECTIVE_POINT_HEIGHT, SRS_PP_SATELLITE_HEIGHT},
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING},
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING},
    /* { CF_PP_SWEEP_ANGLE_AXIS, .... } handled as a proj.4 extension */
    {nullptr, nullptr}};

/* Mappings for various projections, including netcdf and GDAL projection names
   and corresponding oNetcdfSRS_PP mapping struct.
   A NULL mappings value means that the projection is not included in the CF
   standard and the generic mapping (poGenericMappings) will be used. */
typedef struct
{
    const char *CF_SRS;
    const char *WKT_SRS;
    const oNetcdfSRS_PP *mappings;
} oNetcdfSRS_PT;

static const oNetcdfSRS_PT poNetcdfSRS_PT[] = {
    {CF_PT_AEA, SRS_PT_ALBERS_CONIC_EQUAL_AREA, poAEAMappings},
    {CF_PT_AE, SRS_PT_AZIMUTHAL_EQUIDISTANT, poAEMappings},
    {"cassini_soldner", SRS_PT_CASSINI_SOLDNER, nullptr},
    {CF_PT_LCEA, SRS_PT_CYLINDRICAL_EQUAL_AREA, poLCEAMappings},
    {"eckert_iv", SRS_PT_ECKERT_IV, nullptr},
    {"eckert_vi", SRS_PT_ECKERT_VI, nullptr},
    {"equidistant_conic", SRS_PT_EQUIDISTANT_CONIC, nullptr},
    {"equirectangular", SRS_PT_EQUIRECTANGULAR, nullptr},
    {"gall_stereographic", SRS_PT_GALL_STEREOGRAPHIC, nullptr},
    {CF_PT_GEOS, SRS_PT_GEOSTATIONARY_SATELLITE, poGEOSMappings},
    {"goode_homolosine", SRS_PT_GOODE_HOMOLOSINE, nullptr},
    {"gnomonic", SRS_PT_GNOMONIC, nullptr},
    {"hotine_oblique_mercator", SRS_PT_HOTINE_OBLIQUE_MERCATOR, nullptr},
    {"hotine_oblique_mercator_2P",
     SRS_PT_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN, nullptr},
    {"laborde_oblique_mercator", SRS_PT_LABORDE_OBLIQUE_MERCATOR, nullptr},
    {CF_PT_LCC, SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP, poLCC1SPMappings},
    {CF_PT_LCC, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP, poLCC2SPMappings},
    {CF_PT_LAEA, SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA, poLAEAMappings},
    {CF_PT_MERCATOR, SRS_PT_MERCATOR_1SP, poM1SPMappings},
    {CF_PT_MERCATOR, SRS_PT_MERCATOR_2SP, poM2SPMappings},
    {"miller_cylindrical", SRS_PT_MILLER_CYLINDRICAL, nullptr},
    {"mollweide", SRS_PT_MOLLWEIDE, nullptr},
    {"new_zealand_map_grid", SRS_PT_NEW_ZEALAND_MAP_GRID, nullptr},
    /* for now map to STEREO, see bug #4267 */
    {"oblique_stereographic", SRS_PT_OBLIQUE_STEREOGRAPHIC, nullptr},
    /* {STEREO, SRS_PT_OBLIQUE_STEREOGRAPHIC, poStMappings },  */
    {CF_PT_ORTHOGRAPHIC, SRS_PT_ORTHOGRAPHIC, poOrthoMappings},
    {CF_PT_POLAR_STEREO, SRS_PT_POLAR_STEREOGRAPHIC, poPSmappings},
    {"polyconic", SRS_PT_POLYCONIC, nullptr},
    {"robinson", SRS_PT_ROBINSON, nullptr},
    {"sinusoidal", SRS_PT_SINUSOIDAL, nullptr},
    {CF_PT_STEREO, SRS_PT_STEREOGRAPHIC, poStMappings},
    {"swiss_oblique_cylindrical", SRS_PT_SWISS_OBLIQUE_CYLINDRICAL, nullptr},
    {CF_PT_TM, SRS_PT_TRANSVERSE_MERCATOR, poTMMappings},
    {"TM_south_oriented", SRS_PT_TRANSVERSE_MERCATOR_SOUTH_ORIENTED, nullptr},
    {nullptr, nullptr, nullptr},
};

/* Write any needed projection attributes *
 * poPROJCS: ptr to proj crd system
 * pszProjection: name of projection system in GDAL WKT
 *
 * The function first looks for the oNetcdfSRS_PP mapping object
 * that corresponds to the input projection name. If none is found
 * the generic mapping is used.  In the case of specific mappings,
 * the driver looks for each attribute listed in the mapping object
 * and then looks up the value within the OGR_SRSNode. In the case
 * of the generic mapping, the lookup is reversed (projection params,
 * then mapping).  For more generic code, GDAL->NETCDF
 * mappings and the associated value are saved in std::map objects.
 */

// NOTE: modifications by ET to combine the specific and generic mappings.

static std::vector<std::pair<std::string, double>>
NCDFGetProjAttribs(const OGR_SRSNode *poPROJCS, const char *pszProjection)
{
    const oNetcdfSRS_PP *poMap = nullptr;
    int nMapIndex = -1;

    // Find the appropriate mapping.
    for (int iMap = 0; poNetcdfSRS_PT[iMap].WKT_SRS != nullptr; iMap++)
    {
        if (EQUAL(pszProjection, poNetcdfSRS_PT[iMap].WKT_SRS))
        {
            nMapIndex = iMap;
            poMap = poNetcdfSRS_PT[iMap].mappings;
            break;
        }
    }

    // ET TODO if projection name is not found, should we do something special?
    if (nMapIndex == -1)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "projection name %s not found in the lookup tables!",
                 pszProjection);
    }
    // If no mapping was found or assigned, set the generic one.
    if (!poMap)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "projection name %s in not part of the CF standard, "
                 "will not be supported by CF!",
                 pszProjection);
        poMap = poGenericMappings;
    }

    // Initialize local map objects.

    // Attribute <GDAL,NCDF> and Value <NCDF,value> mappings
    std::map<std::string, std::string> oAttMap;
    for (int iMap = 0; poMap[iMap].WKT_ATT != nullptr; iMap++)
    {
        oAttMap[poMap[iMap].WKT_ATT] = poMap[iMap].CF_ATT;
    }

    const char *pszParamVal = nullptr;
    std::map<std::string, double> oValMap;
    for (int iChild = 0; iChild < poPROJCS->GetChildCount(); iChild++)
    {
        const OGR_SRSNode *poNode = poPROJCS->GetChild(iChild);
        if (!EQUAL(poNode->GetValue(), "PARAMETER") ||
            poNode->GetChildCount() != 2)
            continue;
        const char *pszParamStr = poNode->GetChild(0)->GetValue();
        pszParamVal = poNode->GetChild(1)->GetValue();

        oValMap[pszParamStr] = CPLAtof(pszParamVal);
    }

    // Results to write.
    std::vector<std::pair<std::string, double>> oOutList;

    // Lookup mappings and fill output vector.
    if (poMap != poGenericMappings)
    {
        // special case for PS (Polar Stereographic) grid.
        if (EQUAL(pszProjection, SRS_PT_POLAR_STEREOGRAPHIC))
        {
            const double dfLat = oValMap[SRS_PP_LATITUDE_OF_ORIGIN];

            auto oScaleFactorIter = oValMap.find(SRS_PP_SCALE_FACTOR);
            if (oScaleFactorIter != oValMap.end())
            {
                // Polar Stereographic (variant A)
                const double dfScaleFactor = oScaleFactorIter->second;
                // dfLat should be +/- 90
                oOutList.push_back(
                    std::make_pair(std::string(CF_PP_LAT_PROJ_ORIGIN), dfLat));
                oOutList.push_back(std::make_pair(
                    std::string(CF_PP_SCALE_FACTOR_ORIGIN), dfScaleFactor));
            }
            else
            {
                // Polar Stereographic (variant B)
                const double dfLatPole = (dfLat > 0) ? 90.0 : -90.0;
                oOutList.push_back(std::make_pair(
                    std::string(CF_PP_LAT_PROJ_ORIGIN), dfLatPole));
                oOutList.push_back(
                    std::make_pair(std::string(CF_PP_STD_PARALLEL), dfLat));
            }
        }

        // Specific mapping, loop over mapping values.
        for (const auto &oAttIter : oAttMap)
        {
            const std::string &osGDALAtt = oAttIter.first;
            const std::string &osNCDFAtt = oAttIter.second;
            const auto oValIter = oValMap.find(osGDALAtt);

            if (oValIter != oValMap.end())
            {
                double dfValue = oValIter->second;
                bool bWriteVal = true;

                // special case for LCC-1SP
                //   See comments in netcdfdataset.h for this projection.
                if (EQUAL(SRS_PP_SCALE_FACTOR, osGDALAtt.c_str()) &&
                    EQUAL(pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP))
                {
                    // Default is to not write as it is not CF-1.
                    bWriteVal = false;
                    // Test if there is no standard_parallel1.
                    if (oValMap.find(std::string(CF_PP_STD_PARALLEL_1)) ==
                        oValMap.end())
                    {
                        // If scale factor != 1.0, write value for GDAL, but
                        // this is not supported by CF-1.
                        if (!CPLIsEqual(dfValue, 1.0))
                        {
                            CPLError(
                                CE_Failure, CPLE_NotSupported,
                                "NetCDF driver export of LCC-1SP with scale "
                                "factor != 1.0 and no standard_parallel1 is "
                                "not CF-1 (bug #3324).  Use the 2SP variant "
                                "which is supported by CF.");
                            bWriteVal = true;
                        }
                        // Else copy standard_parallel1 from
                        // latitude_of_origin, because scale_factor=1.0.
                        else
                        {
                            const auto oValIter2 = oValMap.find(
                                std::string(SRS_PP_LATITUDE_OF_ORIGIN));
                            if (oValIter2 != oValMap.end())
                            {
                                oOutList.push_back(std::make_pair(
                                    std::string(CF_PP_STD_PARALLEL_1),
                                    oValIter2->second));
                            }
                            else
                            {
                                CPLError(CE_Failure, CPLE_NotSupported,
                                         "NetCDF driver export of LCC-1SP with "
                                         "no standard_parallel1 "
                                         "and no latitude_of_origin is not "
                                         "supported (bug #3324).");
                            }
                        }
                    }
                }
                if (bWriteVal)
                    oOutList.push_back(std::make_pair(osNCDFAtt, dfValue));
            }
#ifdef NCDF_DEBUG
            else
            {
                CPLDebug("GDAL_netCDF", "NOT FOUND!");
            }
#endif
        }
    }
    else
    {
        // Generic mapping, loop over projected values.
        for (const auto &oValIter : oValMap)
        {
            const auto &osGDALAtt = oValIter.first;
            const double dfValue = oValIter.second;

            const auto &oAttIter = oAttMap.find(osGDALAtt);

            if (oAttIter != oAttMap.end())
            {
                oOutList.push_back(std::make_pair(oAttIter->second, dfValue));
            }
            /* for SRS_PP_SCALE_FACTOR write 2 mappings */
            else if (EQUAL(osGDALAtt.c_str(), SRS_PP_SCALE_FACTOR))
            {
                oOutList.push_back(std::make_pair(
                    std::string(CF_PP_SCALE_FACTOR_MERIDIAN), dfValue));
                oOutList.push_back(std::make_pair(
                    std::string(CF_PP_SCALE_FACTOR_ORIGIN), dfValue));
            }
            /* if not found insert the GDAL name */
            else
            {
                oOutList.push_back(std::make_pair(osGDALAtt, dfValue));
            }
        }
    }

    return oOutList;
}

/************************************************************************/
/*                           exportToCF1()                              */
/************************************************************************/

/**
 * \brief Export a CRS to netCDF CF-1 definitions.
 *
 * http://cfconventions.org/cf-conventions/cf-conventions.html#appendix-grid-mappings
 *
 * This function is the equivalent of the C function OSRExportToCF1().
 *
 * @param[out] ppszGridMappingName Pointer to the suggested name for the grid
 * mapping variable. ppszGridMappingName may be nullptr.
 * *ppszGridMappingName should be freed with CPLFree().
 *
 * @param[out] ppapszKeyValues Pointer to a null-terminated list of key/value pairs,
 * to write into the grid mapping variable. ppapszKeyValues may be
 * nullptr. *ppapszKeyValues should be freed with CSLDestroy()
 * Values may be of type string, double or a list of 2 double values (comma
 * separated).
 *
 * @param[out] ppszUnits Pointer to the value of the "units" attribute of the
 * X/Y arrays. ppszGridMappingName may be nullptr. *ppszUnits should be freed with
 * CPLFree().
 *
 * @param[in] papszOptions Options. Currently none supported
 *
 * @return OGRERR_NONE on success or an error code in case of failure.
 * @since 3.9
 */

OGRErr
OGRSpatialReference::exportToCF1(char **ppszGridMappingName,
                                 char ***ppapszKeyValues, char **ppszUnits,
                                 CPL_UNUSED CSLConstList papszOptions) const
{
    if (ppszGridMappingName)
        *ppszGridMappingName = nullptr;

    if (ppapszKeyValues)
        *ppapszKeyValues = nullptr;

    if (ppszUnits)
        *ppszUnits = nullptr;

    if (ppszGridMappingName || ppapszKeyValues)
    {
        bool bWriteWkt = true;

        struct Value
        {
            std::string key{};
            std::string valueStr{};
            std::vector<double> doubles{};
        };

        std::vector<Value> oParams;

        const auto addParamString =
            [&oParams](const char *key, const char *value)
        {
            Value v;
            v.key = key;
            v.valueStr = value;
            oParams.emplace_back(std::move(v));
        };

        const auto addParamDouble = [&oParams](const char *key, double value)
        {
            Value v;
            v.key = key;
            v.doubles.push_back(value);
            oParams.emplace_back(std::move(v));
        };

        const auto addParam2Double =
            [&oParams](const char *key, double value1, double value2)
        {
            Value v;
            v.key = key;
            v.doubles.push_back(value1);
            v.doubles.push_back(value2);
            oParams.emplace_back(std::move(v));
        };

        std::string osCFProjection;
        if (IsProjected())
        {
            // Write CF-1.5 compliant Projected attributes.

            const OGR_SRSNode *poPROJCS = GetAttrNode("PROJCS");
            if (poPROJCS == nullptr)
                return OGRERR_FAILURE;
            const char *pszProjName = GetAttrValue("PROJECTION");
            if (pszProjName == nullptr)
                return OGRERR_FAILURE;

            // Basic Projection info (grid_mapping and datum).
            for (int i = 0; poNetcdfSRS_PT[i].WKT_SRS != nullptr; i++)
            {
                if (EQUAL(poNetcdfSRS_PT[i].WKT_SRS, pszProjName))
                {
                    osCFProjection = poNetcdfSRS_PT[i].CF_SRS;
                    break;
                }
            }
            if (osCFProjection.empty())
                return OGRERR_FAILURE;

            addParamString(CF_GRD_MAPPING_NAME, osCFProjection.c_str());

            // Various projection attributes.
            // PDS: keep in sync with SetProjection function
            auto oOutList = NCDFGetProjAttribs(poPROJCS, pszProjName);

            /* Write all the values that were found */
            double dfStdP[2] = {0, 0};
            bool bFoundStdP1 = false;
            bool bFoundStdP2 = false;
            for (const auto &it : oOutList)
            {
                const char *pszParamVal = it.first.c_str();
                double dfValue = it.second;
                /* Handle the STD_PARALLEL attrib */
                if (EQUAL(pszParamVal, CF_PP_STD_PARALLEL_1))
                {
                    bFoundStdP1 = true;
                    dfStdP[0] = dfValue;
                }
                else if (EQUAL(pszParamVal, CF_PP_STD_PARALLEL_2))
                {
                    bFoundStdP2 = true;
                    dfStdP[1] = dfValue;
                }
                else
                {
                    addParamDouble(pszParamVal, dfValue);
                }
            }
            /* Now write the STD_PARALLEL attrib */
            if (bFoundStdP1)
            {
                /* one value  */
                if (!bFoundStdP2)
                {
                    addParamDouble(CF_PP_STD_PARALLEL, dfStdP[0]);
                }
                else
                {
                    // Two values.
                    addParam2Double(CF_PP_STD_PARALLEL, dfStdP[0], dfStdP[1]);
                }
            }

            if (EQUAL(pszProjName, SRS_PT_GEOSTATIONARY_SATELLITE))
            {
                const char *pszPredefProj4 =
                    GetExtension(GetRoot()->GetValue(), "PROJ4", nullptr);
                const char *pszSweepAxisAngle =
                    (pszPredefProj4 != nullptr &&
                     strstr(pszPredefProj4, "+sweep=x"))
                        ? "x"
                        : "y";
                addParamString(CF_PP_SWEEP_ANGLE_AXIS, pszSweepAxisAngle);
            }
        }
        else if (IsDerivedGeographic())
        {
            const OGR_SRSNode *poConversion = GetAttrNode("DERIVINGCONVERSION");
            if (poConversion == nullptr)
                return OGRERR_FAILURE;
            const char *pszMethod = GetAttrValue("METHOD");
            if (pszMethod == nullptr)
                return OGRERR_FAILURE;

            std::map<std::string, double> oValMap;
            for (int iChild = 0; iChild < poConversion->GetChildCount();
                 iChild++)
            {
                const OGR_SRSNode *poNode = poConversion->GetChild(iChild);
                if (!EQUAL(poNode->GetValue(), "PARAMETER") ||
                    poNode->GetChildCount() <= 2)
                    continue;
                const char *pszParamStr = poNode->GetChild(0)->GetValue();
                const char *pszParamVal = poNode->GetChild(1)->GetValue();
                oValMap[pszParamStr] = CPLAtof(pszParamVal);
            }

            constexpr const char *ROTATED_POLE_VAR_NAME = "rotated_pole";
            if (EQUAL(pszMethod, "PROJ ob_tran o_proj=longlat"))
            {
                // Not enough interoperable to be written as WKT
                bWriteWkt = false;

                const double dfLon0 = oValMap["lon_0"];
                const double dfLonp = oValMap["o_lon_p"];
                const double dfLatp = oValMap["o_lat_p"];

                osCFProjection = ROTATED_POLE_VAR_NAME;
                addParamString(CF_GRD_MAPPING_NAME,
                               CF_PT_ROTATED_LATITUDE_LONGITUDE);
                addParamDouble(CF_PP_GRID_NORTH_POLE_LONGITUDE, dfLon0 - 180);
                addParamDouble(CF_PP_GRID_NORTH_POLE_LATITUDE, dfLatp);
                addParamDouble(CF_PP_NORTH_POLE_GRID_LONGITUDE, dfLonp);
            }
            else if (EQUAL(pszMethod, "Pole rotation (netCDF CF convention)"))
            {
                // Not enough interoperable to be written as WKT
                bWriteWkt = false;

                const double dfGridNorthPoleLat =
                    oValMap["Grid north pole latitude (netCDF CF convention)"];
                const double dfGridNorthPoleLong =
                    oValMap["Grid north pole longitude (netCDF CF convention)"];
                const double dfNorthPoleGridLong =
                    oValMap["North pole grid longitude (netCDF CF convention)"];

                osCFProjection = ROTATED_POLE_VAR_NAME;
                addParamString(CF_GRD_MAPPING_NAME,
                               CF_PT_ROTATED_LATITUDE_LONGITUDE);
                addParamDouble(CF_PP_GRID_NORTH_POLE_LONGITUDE,
                               dfGridNorthPoleLong);
                addParamDouble(CF_PP_GRID_NORTH_POLE_LATITUDE,
                               dfGridNorthPoleLat);
                addParamDouble(CF_PP_NORTH_POLE_GRID_LONGITUDE,
                               dfNorthPoleGridLong);
            }
            else if (EQUAL(pszMethod, "Pole rotation (GRIB convention)"))
            {
                // Not enough interoperable to be written as WKT
                bWriteWkt = false;

                const double dfLatSouthernPole =
                    oValMap["Latitude of the southern pole (GRIB convention)"];
                const double dfLonSouthernPole =
                    oValMap["Longitude of the southern pole (GRIB convention)"];
                const double dfAxisRotation =
                    oValMap["Axis rotation (GRIB convention)"];

                const double dfLon0 = dfLonSouthernPole;
                const double dfLonp = dfAxisRotation == 0 ? 0 : -dfAxisRotation;
                const double dfLatp =
                    dfLatSouthernPole == 0 ? 0 : -dfLatSouthernPole;

                osCFProjection = ROTATED_POLE_VAR_NAME;
                addParamString(CF_GRD_MAPPING_NAME,
                               CF_PT_ROTATED_LATITUDE_LONGITUDE);
                addParamDouble(CF_PP_GRID_NORTH_POLE_LONGITUDE, dfLon0 - 180);
                addParamDouble(CF_PP_GRID_NORTH_POLE_LATITUDE, dfLatp);
                addParamDouble(CF_PP_NORTH_POLE_GRID_LONGITUDE, dfLonp);
            }
            else
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Unsupported method for DerivedGeographicCRS: %s",
                         pszMethod);
                return OGRERR_FAILURE;
            }
        }
        else
        {
            // Write CF-1.5 compliant Geographics attributes.
            // Note: WKT information will not be preserved (e.g. WGS84).
            osCFProjection = "crs";
            addParamString(CF_GRD_MAPPING_NAME, CF_PT_LATITUDE_LONGITUDE);
        }

        constexpr const char *CF_LNG_NAME = "long_name";
        addParamString(CF_LNG_NAME, "CRS definition");

        // Write CF-1.5 compliant common attributes.

        // DATUM information.
        addParamDouble(CF_PP_LONG_PRIME_MERIDIAN, GetPrimeMeridian());
        addParamDouble(CF_PP_SEMI_MAJOR_AXIS, GetSemiMajor());
        addParamDouble(CF_PP_INVERSE_FLATTENING, GetInvFlattening());

        if (bWriteWkt)
        {
            char *pszSpatialRef = nullptr;
            exportToWkt(&pszSpatialRef);
            if (pszSpatialRef && pszSpatialRef[0])
            {
                addParamString(NCDF_CRS_WKT, pszSpatialRef);
            }
            CPLFree(pszSpatialRef);
        }

        if (ppszGridMappingName)
            *ppszGridMappingName = CPLStrdup(osCFProjection.c_str());

        if (ppapszKeyValues)
        {
            CPLStringList aosKeyValues;
            for (const auto &param : oParams)
            {
                if (!param.valueStr.empty())
                {
                    aosKeyValues.AddNameValue(param.key.c_str(),
                                              param.valueStr.c_str());
                }
                else
                {
                    std::string osVal;
                    for (const double dfVal : param.doubles)
                    {
                        if (!osVal.empty())
                            osVal += ',';
                        osVal += CPLSPrintf("%.18g", dfVal);
                    }
                    aosKeyValues.AddNameValue(param.key.c_str(), osVal.c_str());
                }
            }
            *ppapszKeyValues = aosKeyValues.StealList();
        }
    }

    if (ppszUnits)
    {
        const char *pszUnits = nullptr;
        const char *pszUnitsToWrite = "";

        const double dfUnits = GetLinearUnits(&pszUnits);
        if (fabs(dfUnits - 1.0) < 1e-15 || pszUnits == nullptr ||
            EQUAL(pszUnits, "m") || EQUAL(pszUnits, "metre"))
        {
            pszUnitsToWrite = "m";
        }
        else if (fabs(dfUnits - 1000.0) < 1e-15)
        {
            pszUnitsToWrite = "km";
        }
        else if (fabs(dfUnits - CPLAtof(SRS_UL_US_FOOT_CONV)) < 1e-15 ||
                 EQUAL(pszUnits, SRS_UL_US_FOOT) ||
                 EQUAL(pszUnits, "US survey foot"))
        {
            pszUnitsToWrite = "US_survey_foot";
        }
        if (pszUnitsToWrite)
            *ppszUnits = CPLStrdup(pszUnitsToWrite);
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                          OSRExportToCF1()                            */
/************************************************************************/
/**
 * \brief Export a CRS to netCDF CF-1 definitions.
 *
 * This function is the same as OGRSpatialReference::exportToCF1().
 */
OGRErr OSRExportToCF1(OGRSpatialReferenceH hSRS, char **ppszGridMappingName,
                      char ***ppapszKeyValues, char **ppszUnits,
                      CSLConstList papszOptions)

{
    VALIDATE_POINTER1(hSRS, "OSRExportToCF1", OGRERR_FAILURE);

    return OGRSpatialReference::FromHandle(hSRS)->exportToCF1(
        ppszGridMappingName, ppapszKeyValues, ppszUnits, papszOptions);
}

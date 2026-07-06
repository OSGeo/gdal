/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGRSpatialReference translation from an ISIS3 (PVL) Mapping group.
 * Author:   Oleg Alexandrov
 *
 ******************************************************************************
 * Copyright (c) 2026, Oleg Alexandrov
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "ogr_srs_api.h"

#include <cmath>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_json.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"

/************************************************************************/
/*                        GetISISMappingString()                        */
/************************************************************************/

// Read a value from the ISIS PVL Mapping group as a string. A value that
// carries a unit is stored as a { "value", "unit" } object, so unwrap that.

static CPLString GetISISMappingString(const CPLJSONObject &oMapping,
                                      const char *pszKey,
                                      const char *pszDefault = "")
{
    CPLJSONObject oChild = oMapping.GetObj(pszKey);
    if (oChild.GetType() == CPLJSONObject::Type::Object)
        oChild = oChild.GetObj("value");
    if (oChild.GetType() == CPLJSONObject::Type::String)
        return oChild.ToString(pszDefault);
    return pszDefault;
}

/************************************************************************/
/*                        GetISISMappingDouble()                        */
/************************************************************************/

// Read a value from the ISIS PVL Mapping group as a double.

static double GetISISMappingDouble(const CPLJSONObject &oMapping,
                                   const char *pszKey, double dfDefault = 0.0)
{
    CPLJSONObject oChild = oMapping.GetObj(pszKey);
    if (oChild.GetType() == CPLJSONObject::Type::Object)
        oChild = oChild.GetObj("value");
    switch (oChild.GetType())
    {
        case CPLJSONObject::Type::Integer:
        case CPLJSONObject::Type::Long:
            return static_cast<double>(oChild.ToLong());
        case CPLJSONObject::Type::Double:
            return oChild.ToDouble();
        case CPLJSONObject::Type::String:
            return CPLAtof(oChild.ToString().c_str());
        default:
            return dfDefault;
    }
}

/************************************************************************/
/*                      ParseISISPVLMappingGroup()                      */
/************************************************************************/

// Parse an ISIS PVL Mapping group as a flat CPLJSONObject of keys and string
// values. Any surrounding "Group = Mapping" / "End_Group" wrapper is
// ignored, and a quoted value (such as ProjStr) may span lines.

static CPLJSONObject ParseISISPVLMappingGroup(const char *pszText)
{
    CPLJSONObject oMapping;
    const char *pszIter = pszText;
    while (*pszIter)
    {
        while (*pszIter && isspace(static_cast<unsigned char>(*pszIter)))
            ++pszIter;
        if (!*pszIter)
            break;
        // A comment line.
        if (*pszIter == '#')
        {
            while (*pszIter && *pszIter != '\n')
                ++pszIter;
            continue;
        }

        // Read the keyword name.
        std::string osKey;
        while (*pszIter && !isspace(static_cast<unsigned char>(*pszIter)) &&
               *pszIter != '=')
        {
            osKey += *pszIter;
            ++pszIter;
        }
        while (*pszIter && isspace(static_cast<unsigned char>(*pszIter)))
            ++pszIter;

        // Group/Object terminators carry no value.
        if (EQUAL(osKey.c_str(), "End") || EQUAL(osKey.c_str(), "End_Group") ||
            EQUAL(osKey.c_str(), "End_Object"))
            continue;

        if (*pszIter != '=')
        {
            // A keyword with no value; skip to the end of the line.
            while (*pszIter && *pszIter != '\n')
                ++pszIter;
            continue;
        }
        ++pszIter;  // skip '='
        while (*pszIter && (*pszIter == ' ' || *pszIter == '\t'))
            ++pszIter;

        // Read the value.
        std::string osValue;
        if (*pszIter == '"')
        {
            ++pszIter;  // opening quote
            while (*pszIter && *pszIter != '"')
            {
                osValue += *pszIter;
                ++pszIter;
            }
            if (*pszIter == '"')
                ++pszIter;  // closing quote
        }
        else
        {
            while (*pszIter && *pszIter != '\n' && *pszIter != '\r')
            {
                osValue += *pszIter;
                ++pszIter;
            }
            while (!osValue.empty() &&
                   (osValue.back() == ' ' || osValue.back() == '\t'))
                osValue.pop_back();
        }

        // The "Group = Mapping" / "Object = ..." wrapper is not a real key.
        if (EQUAL(osKey.c_str(), "Group") || EQUAL(osKey.c_str(), "Object"))
            continue;

        if (!osKey.empty())
            oMapping.Add(osKey, osValue);
    }
    return oMapping;
}

/************************************************************************/
/*                         importFromISISPVL()                          */
/************************************************************************/

/**
 * \brief Import a coordinate system from an ISIS3 PVL Mapping group.
 *
 * This method reads the projection of an ISIS3 cube from the text of its
 * Mapping group (the "Group = Mapping ... End_Group" block, with or without
 * that wrapper). It supports the named ISIS projections as well as the newer
 * generic form where ProjectionName is IProj and the definition is carried by
 * a ProjStr PROJ string.
 *
 * This is the same logic used by the GDAL ISIS3 driver to georeference a cube.
 * Exposing it lets applications that produce ISIS mapping groups (such as ISIS
 * itself when writing GeoTIFFs) build the coordinate system without a copy of
 * this code.
 *
 * This method is the equivalent of the C function OSRImportFromISISPVL().
 *
 * @param pszPVLMappingGroup text of the ISIS PVL Mapping group.
 *
 * @return OGRERR_NONE on success, or an error code on failure.
 *
 * @since GDAL 3.14
 */

OGRErr OGRSpatialReference::importFromISISPVL(const char *pszPVLMappingGroup)
{
    return importFromISISPVL(ParseISISPVLMappingGroup(pszPVLMappingGroup));
}

/************************************************************************/
/*                         importFromISISPVL()                          */
/************************************************************************/

/**
 * \brief Import a coordinate system from an already parsed ISIS3 Mapping group.
 *
 * This is the overload of importFromISISPVL(const char*) that takes the
 * Mapping group as a CPLJSONObject of key/value pairs, as parsed by the
 * ISIS3 driver. See importFromISISPVL(const char*) for details.
 *
 * @param oMapping the ISIS Mapping group as a CPLJSONObject.
 *
 * @return OGRERR_NONE on success, or an error code on failure.
 *
 * @since GDAL 3.14
 */

OGRErr OGRSpatialReference::importFromISISPVL(const CPLJSONObject &oMapping)
{
    Clear();

    /***********   Grab TARGET_NAME  ************/
    /**** This is the planets name i.e. Mars ***/
    const CPLString target_name = GetISISMappingString(oMapping, "TargetName");

#ifdef notdef
    const double dfLongitudeMulFactor =
        EQUAL(GetISISMappingString(oMapping, "LongitudeDirection",
                                   "PositiveEast"),
              "PositiveEast")
            ? 1
            : -1;
#else
    const double dfLongitudeMulFactor = 1;
#endif

    /***********   Grab MAP_PROJECTION_TYPE ************/
    const CPLString map_proj_name =
        GetISISMappingString(oMapping, "ProjectionName");

    /***********   Grab SEMI-MAJOR ************/
    const double semi_major =
        GetISISMappingDouble(oMapping, "EquatorialRadius");

    /***********   Grab semi-minor ************/
    const double semi_minor = GetISISMappingDouble(oMapping, "PolarRadius");

    /***********   Grab CENTER_LAT ************/
    const double center_lat = GetISISMappingDouble(oMapping, "CenterLatitude");

    /***********   Grab CENTER_LON ************/
    const double center_lon =
        GetISISMappingDouble(oMapping, "CenterLongitude") *
        dfLongitudeMulFactor;

    /***********   Grab 1st std parallel ************/
    const double first_std_parallel =
        GetISISMappingDouble(oMapping, "FirstStandardParallel");

    /***********   Grab 2nd std parallel ************/
    const double second_std_parallel =
        GetISISMappingDouble(oMapping, "SecondStandardParallel");

    /***********   Grab scaleFactor ************/
    const double scaleFactor =
        GetISISMappingDouble(oMapping, "scaleFactor", 1.0);

    /*** grab      LatitudeType = Planetographic ****/
    // Need to further study how ocentric/ographic will effect the gdal library
    // So far we will use this fact to define a sphere or ellipse for some
    // projections

    // Frank - may need to talk this over
    bool bIsGeographic = true;
    if (EQUAL(GetISISMappingString(oMapping, "LatitudeType"), "Planetocentric"))
        bIsGeographic = false;

    // Set oSRS projection and parameters
    // ############################################################
    // ISIS3 Projection types
    //   Equirectangular
    //   LambertConformal
    //   Mercator
    //   ObliqueCylindrical
    //   Orthographic
    //   PolarStereographic
    //   SimpleCylindrical
    //   Sinusoidal
    //   TransverseMercator

#ifdef DEBUG
    CPLDebug("ISIS3", "using projection %s", map_proj_name.c_str());
#endif

    bool bProjectionSet = true;

    // The ISIS IProj projection (ProjectionName = IProj) carries its full
    // definition in a ProjStr PROJ string. When present, use it directly and
    // skip the per-parameter reconstruction below.
    const CPLString osProjStrRaw = GetISISMappingString(oMapping, "ProjStr");
    if (!osProjStrRaw.empty())
    {
        // A ProjStr that spans several label lines comes back with the line
        // breaks embedded as literal "\n" (plus indentation), or as real
        // control characters. Turn those into spaces before handing it to PROJ.
        std::string osProjStr;
        for (const char *pszIter = osProjStrRaw.c_str(); *pszIter; ++pszIter)
        {
            if (pszIter[0] == '\\' &&
                (pszIter[1] == 'n' || pszIter[1] == 'r' || pszIter[1] == 't'))
            {
                osProjStr += ' ';
                ++pszIter;
            }
            else if (*pszIter == '\n' || *pszIter == '\r' || *pszIter == '\t')
                osProjStr += ' ';
            else
                osProjStr += *pszIter;
        }

        OGRErr eErr;
        {
            // A ProjStr that PROJ rejects should leave a warning and an error
            // code, not a hard error that callers see as an exception.
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            eErr = importFromProj4(osProjStr.c_str());
        }
        if (eErr == OGRERR_NONE)
        {
            SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            return OGRERR_NONE;
        }

        CPLError(CE_Warning, CPLE_AppDefined,
                 "Cannot parse ProjStr '%s' from ISIS3 Mapping group.",
                 osProjStr.c_str());
        return OGRERR_CORRUPT_DATA;
    }
    else if ((EQUAL(map_proj_name, "Equirectangular")) ||
             (EQUAL(map_proj_name, "SimpleCylindrical")))
    {
        SetEquirectangular2(0.0, center_lon, center_lat, 0, 0);
    }
    else if (EQUAL(map_proj_name, "Orthographic"))
    {
        SetOrthographic(center_lat, center_lon, 0, 0);
    }
    else if (EQUAL(map_proj_name, "Sinusoidal"))
    {
        SetSinusoidal(center_lon, 0, 0);
    }
    else if (EQUAL(map_proj_name, "Mercator"))
    {
        SetMercator(center_lat, center_lon, scaleFactor, 0, 0);
    }
    else if (EQUAL(map_proj_name, "PolarStereographic"))
    {
        SetPS(center_lat, center_lon, scaleFactor, 0, 0);
    }
    else if (EQUAL(map_proj_name, "TransverseMercator"))
    {
        SetTM(center_lat, center_lon, scaleFactor, 0, 0);
    }
    else if (EQUAL(map_proj_name, "LambertConformal"))
    {
        SetLCC(first_std_parallel, second_std_parallel, center_lat, center_lon,
               0, 0);
    }
    else if (EQUAL(map_proj_name, "PointPerspective"))
    {
        // Distance parameter is the distance to the center of the body, and is
        // given in km
        const double distance =
            GetISISMappingDouble(oMapping, "Distance") * 1000.0;
        const double height_above_ground = distance - semi_major;
        SetVerticalPerspective(center_lat, center_lon, 0, height_above_ground,
                               0, 0);
    }
    else if (EQUAL(map_proj_name, "ObliqueCylindrical"))
    {
        const double poleLatitude =
            GetISISMappingDouble(oMapping, "PoleLatitude");
        const double poleLongitude =
            GetISISMappingDouble(oMapping, "PoleLongitude") *
            dfLongitudeMulFactor;
        const double poleRotation =
            GetISISMappingDouble(oMapping, "PoleRotation");
        CPLString oProj4String;
        // ISIS3 rotated pole doesn't use the same conventions than PROJ ob_tran
        // Compare the sign difference in
        // https://github.com/USGS-Astrogeology/ISIS3/blob/3.8.0/isis/src/base/objs/ObliqueCylindrical/ObliqueCylindrical.cpp#L244
        // and
        // https://github.com/OSGeo/PROJ/blob/6.2/src/projections/ob_tran.cpp#L34
        // They can be compensated by modifying the poleLatitude to
        // 180-poleLatitude There's also a sign difference for the poleRotation
        // parameter The existence of those different conventions is
        // acknowledged in
        // https://pds-imaging.jpl.nasa.gov/documentation/Cassini_BIDRSIS.PDF in
        // the middle of page 10
        oProj4String.Printf("+proj=ob_tran +o_proj=eqc +o_lon_p=%.17g "
                            "+o_lat_p=%.17g +lon_0=%.17g",
                            -poleRotation, 180 - poleLatitude, poleLongitude);
        SetFromUserInput(oProj4String);
    }
    else
    {
        CPLDebug("ISIS3",
                 "Dataset projection %s is not supported. Continuing...",
                 map_proj_name.c_str());
        bProjectionSet = false;
    }

    if (!bProjectionSet)
        return OGRERR_UNSUPPORTED_SRS;

    // Create projection name, i.e. MERCATOR MARS and set as ProjCS keyword
    CPLString osProjTargetName(map_proj_name);
    osProjTargetName += " ";
    osProjTargetName += target_name;
    SetProjCS(osProjTargetName);  // set ProjCS keyword

    // The geographic/geocentric name will be the same basic name as the
    // body name 'GCS' = Geographic/Geocentric Coordinate System
    CPLString osGeogName("GCS_");
    osGeogName += target_name;

    // The datum name will be the same basic name as the planet
    CPLString osDatumName("D_");
    osDatumName += target_name;

    CPLString osSphereName(target_name);
    // strcat(osSphereName, "_IAU_IAG");  //Might not be IAU defined so
    // don't add

    // calculate inverse flattening from major and minor axis: 1/f = a/(a-b)
    double iflattening = 0.0;
    if ((semi_major - semi_minor) < 0.0000001)
        iflattening = 0;
    else
        iflattening = semi_major / (semi_major - semi_minor);

    // Set the body size but take into consideration which proj is being
    // used to help w/ proj4 compatibility The use of a Sphere, polar radius
    // or ellipse here is based on how ISIS does it internally
    if (((EQUAL(map_proj_name, "Stereographic") && (fabs(center_lat) == 90))) ||
        (EQUAL(map_proj_name, "PolarStereographic")))
    {
        if (bIsGeographic)
        {
            // Geograpraphic, so set an ellipse
            SetGeogCS(osGeogName, osDatumName, osSphereName, semi_major,
                      iflattening, "Reference_Meridian", 0.0);
        }
        else
        {
            // Geocentric, so force a sphere using the semi-minor axis. I
            // hope...
            osSphereName += "_polarRadius";
            SetGeogCS(osGeogName, osDatumName, osSphereName, semi_minor, 0.0,
                      "Reference_Meridian", 0.0);
        }
    }
    else if ((EQUAL(map_proj_name, "SimpleCylindrical")) ||
             (EQUAL(map_proj_name, "Orthographic")) ||
             (EQUAL(map_proj_name, "Stereographic")) ||
             (EQUAL(map_proj_name, "Sinusoidal")) ||
             (EQUAL(map_proj_name, "PointPerspective")))
    {
        // ISIS uses the spherical equation for these projections
        // so force a sphere.
        SetGeogCS(osGeogName, osDatumName, osSphereName, semi_major, 0.0,
                  "Reference_Meridian", 0.0);
    }
    else if (EQUAL(map_proj_name, "Equirectangular"))
    {
        // Calculate localRadius using ISIS3 simple elliptical method
        //   not the more standard Radius of Curvature method
        // PI = 4 * atan(1);
        const double radLat = center_lat * M_PI / 180;  // in radians
        const double meanRadius = sqrt(pow(semi_minor * cos(radLat), 2) +
                                       pow(semi_major * sin(radLat), 2));
        const double localRadius =
            (meanRadius == 0.0) ? 0.0 : semi_major * semi_minor / meanRadius;
        osSphereName += "_localRadius";
        SetGeogCS(osGeogName, osDatumName, osSphereName, localRadius, 0.0,
                  "Reference_Meridian", 0.0);
    }
    else
    {
        // All other projections: Mercator, Transverse Mercator, Lambert
        // Conformal, etc. Geographic, so set an ellipse
        if (bIsGeographic)
        {
            SetGeogCS(osGeogName, osDatumName, osSphereName, semi_major,
                      iflattening, "Reference_Meridian", 0.0);
        }
        else
        {
            // Geocentric, so force a sphere. I hope...
            SetGeogCS(osGeogName, osDatumName, osSphereName, semi_major, 0.0,
                      "Reference_Meridian", 0.0);
        }
    }

    SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    return OGRERR_NONE;
}

/************************************************************************/
/*                        OSRImportFromISISPVL()                        */
/************************************************************************/

/**
 * \brief Import a coordinate system from an ISIS3 PVL Mapping group.
 *
 * This function is the same as OGRSpatialReference::importFromISISPVL().
 *
 * @since GDAL 3.14
 */

OGRErr OSRImportFromISISPVL(OGRSpatialReferenceH hSRS,
                            const char *pszPVLMappingGroup)
{
    VALIDATE_POINTER1(hSRS, "OSRImportFromISISPVL", OGRERR_FAILURE);

    return OGRSpatialReference::FromHandle(hSRS)->importFromISISPVL(
        pszPVLMappingGroup);
}

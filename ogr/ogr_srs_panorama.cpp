/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGRSpatialReference translation to/from "Panorama" GIS
 *           georeferencing information (also know as GIS "Integration").
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2005, Andrey Kiselev <dron@ak4719.spb.edu>
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2020-2022, Dmitry Baryshnikov <polimax@mail.ru>
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

#include "ogr_spatialref.h"
#include "cpl_conv.h"
#include "cpl_csv.h"
#include "ogr_p.h"

#include <cmath>

constexpr double TO_DEGREES = 180.0 / M_PI;
constexpr double TO_RADIANS = M_PI / 180.0;
constexpr int NONE_VAL = -1L;

// This function computes zone number from the central meridian parameter.
static int GetZoneNumberGK(double dfCenterLong)
{
    return static_cast<int>((dfCenterLong + 363.0) / 6.0 + 0.5) % 60;
}

static int GetZoneNumberUTM(double dfCenterLong)
{
    return static_cast<int>((dfCenterLong + 186.0) / 6.0);
}

static bool IsNone(long val)
{
    return val == -1L || val == 0L || val == 255L;
}

/************************************************************************/
/*  "Panorama" projection codes.                                        */
/************************************************************************/

constexpr long PAN_PROJ_TM = 1L;       // Gauss-Kruger (Transverse Mercator)
constexpr long PAN_PROJ_LCC = 2L;      // Lambert Conformal Conic 2SP
constexpr long PAN_PROJ_STEREO = 5L;   // Stereographic
constexpr long PAN_PROJ_AE = 6L;       // Azimuthal Equidistant (Postel)
constexpr long PAN_PROJ_MERCAT = 8L;   // Mercator
constexpr long PAN_PROJ_POLYC = 10L;   // Polyconic
constexpr long PAN_PROJ_PS = 13L;      // Polar Stereographic
constexpr long PAN_PROJ_GNOMON = 15L;  // Gnomonic
constexpr long PAN_PROJ_UTM = 17L;     // Universal Transverse Mercator (UTM)
constexpr long PAN_PROJ_WAG1 = 18L;    // Wagner I (Kavraisky VI)
constexpr long PAN_PROJ_MOLL = 19L;    // Mollweide
constexpr long PAN_PROJ_EC = 20L;      // Equidistant Conic
constexpr long PAN_PROJ_LAEA = 24L;    // Lambert Azimuthal Equal Area
constexpr long PAN_PROJ_EQC = 27L;     // Equirectangular
constexpr long PAN_PROJ_CEA = 28L;     // Cylindrical Equal Area (Lambert)
constexpr long PAN_PROJ_IMWP = 29L;  // International Map of the World Polyconic
constexpr long PAN_PROJ_SPHERE = 33L;  // Sphere
constexpr long PAN_PROJ_MILLER = 34L;  // Miller
constexpr long PAN_PROJ_PSEUDO_MERCATOR =
    35L;  // Popular Visualisation Pseudo Mercator
/************************************************************************/
/*  "Panorama" datum codes.                                             */
/************************************************************************/

constexpr long PAN_DATUM_PULKOVO42 = 1L;    // Pulkovo 1942
constexpr long PAN_DATUM_UTM = 2L;          // Universal Transverse Mercator
constexpr long PAN_DATUM_RECTANGULAR = 6L;  // WGS84
// constexpr long PAN_DATUM_WGS84 = 8L;        // WGS84
constexpr long PAN_DATUM_PULKOVO95 = 9L;  // Pulokovo 1995
constexpr long PAN_DATUM_GSK2011 = 10L;   // GSK 2011

/************************************************************************/
/*  "Panorama" ellipsoid codes.                                         */
/************************************************************************/

constexpr long PAN_ELLIPSOID_KRASSOVSKY = 1L;  // Krassovsky, 1940
// constexpr long PAN_ELLIPSOID_WGS72       = 2L;  // WGS, 1972
// constexpr long PAN_ELLIPSOID_INT1924     = 3L;  // International, 1924
// (Hayford, 1909) constexpr long PAN_ELLIPSOID_CLARCKE1880 = 4L;  // Clarke,
// 1880 constexpr long PAN_ELLIPSOID_CLARCKE1866 = 5L;  // Clarke, 1866
// (NAD1927) constexpr long PAN_ELLIPSOID_EVEREST1830 = 6L;  // Everest, 1830
// constexpr long PAN_ELLIPSOID_BESSEL1841  = 7L;  // Bessel, 1841
// constexpr long PAN_ELLIPSOID_AIRY1830    = 8L;  // Airy, 1830
constexpr long PAN_ELLIPSOID_WGS84 = 9L;          // WGS, 1984 (GPS)
constexpr long PAN_ELLIPSOID_WGS84_SPHERE = 45L;  // WGS, 1984 (Sphere)
constexpr long PAN_ELLIPSOID_GSK2011 = 46L;       // GSK 2011
constexpr long PAN_ELLIPSOID_PZ90 = 47L;          // PZ-90

/************************************************************************/
/*  Correspondence between "Panorama" datum and EPSG GeogCS codes.             */
/************************************************************************/

constexpr int aoDatums[] = {
    0,     // 0.  Undefined (also may be 255 or -1)
    4284,  // 1.  Pulkovo, 1942
    4326,  // 2.  WGS, 1984,
    4277,  // 3.  OSGB 1936 (British National Grid)
    0,     // 4.  Local spatial reference
    0,     // 5.  SK 63
    0,     // 6.  Rectangular conditional spatial reference
    0,     // 7.  Geodesic coordinates in radians
    0,     // 8.  Geodesic coordinates in degrees
    4200,  // 9.  Pulkovo, 1995
    7683   // 10. GSK 2011
};

constexpr int NUMBER_OF_DATUMS = static_cast<int>(CPL_ARRAYSIZE(aoDatums));

/************************************************************************/
/*  Correspondence between "Panorama" and EPSG ellipsoid codes.         */
/************************************************************************/

constexpr int aoPanoramaEllips[] = {
    0,     // 0. Undefined
    7024,  // 1. Krassovsky, 1940
    7043,  // 2. WGS, 1972
    7022,  // 3. International, 1924 (Hayford, 1909)
    7034,  // 4. Clarke, 1880
    7008,  // 5. Clarke, 1866 (NAD1927)
    7015,  // 6. Everest, 1830
    7004,  // 7. Bessel, 1841
    7001,  // 8. Airy, 1830
    7030,  // 9. WGS, 1984 (GPS)
    7054,  // 10. PZ-90.02 // http://epsg.io/7054-ellipsoid
    7019,  // 11. GRS, 1980 (NAD1983)
    0,     // 12. IERS 1996 (6378136.49 298.25645)
    7022,  // 13. International, 1924 (Hayford, 1909) XXX?
    7036,  // 14. South American, 1969
    7021,  // 15. Indonesian, 1974
    7020,  // 16. Helmert 1906
    0,     // 17. FIXME: Fisher 1960 - https://epsg.io/37002
    0,     // 18. FIXME: Fisher 1968 - https://epsg.io/37003
    0,     // 19. FIXME: Haff 1960 - (6378270.0 297.0)
    7042,  // 20. Everest, 1830
    7003,  // 21. Australian National, 1965
    1024,  // 22. CGCS2000 http://epsg.io/1024-ellipsoid
    7002,  // 23. Airy Modified 1849 http://epsg.io/7002-ellipsoid
    7005,  // 24. Bessel Modified
    7046,  // 25. Bessel Namibia
    7046,  // 26. Bessel Namibia (GLM)
    7013,  // 27. Clarke 1880 (Arc)
    7014,  // 28. Clarke 1880 (SGA 1922)
    7042,  // 29. Everest (1830 Definition)
    7018,  // 30. Everest 1830 Modified
    7056,  // 31. Everest 1830 (RSO 1969)
    7045,  // 32. Everest 1830 (1975 Definition)
    7025,  // 33. NWL 9D
    7027,  // 34. Plessis 1817
    7028,  // 35. Struve 1860
    7029,  // 36. War Office
    7031,  // 37. GEM 10C
    7032,  // 38. OSU86F
    7033,  // 39. OSU91A
    7036,  // 40. GRS 1967
    7041,  // 41. Average Terrestrial System 1977
    7049,  // 42. IAG 1975
    7050,  // 43. GRS 1967 Modified
    7051,  // 44. Danish 1876
    7048,  // 45. GRS 1980 Authalic Sphere
    1025,  // 46. GSK 2011
    7054   // 47. PZ-90
};

constexpr int NUMBER_OF_PANORAM_ELLIPSOIDS =
    static_cast<int>(CPL_ARRAYSIZE(aoPanoramaEllips));

/************************************************************************/
/*  Correspondence between "Panorama" and EPSG vertical CS.             */
/************************************************************************/

constexpr int aoVCS[] = {
    0,     //0, 255, -1 - Undefined
    8357,  //1 Baltic 1957 height
    5711,  //2 AHD height
    5195,  //3 Trieste height
    5710,  //4 Ostend height - zero normal
    5710,  //5 Ostend height - null point de shosse
    0,     //6 Channel height (GB)
    5732,  //7 Belfast height
    5731,  //8 Malin Head height
    0,     //9 Dublib bay height
    5716,  //10 Piraeus height
    5733,  //11 DNN height
    8089,  //12 ISH2004 height
    5782,  //13 Alicante height
    0,     //14 Canary islands
    5214,  //15 Genoa height
    5709,  //16 NAP height
    5776,  //17 NN54 height
    0,     //18 North Norway
    5780,  //19 Cascais height
    5717,  //20 N60 height
    5613,  //21 RH2000 height
    0,     //22 France, Marseilles height
    5775,  //23 Antalya height
    5702,  //24 NGVD29 height (ftUS)
    5705,  //25 Baltic 1977 height
    0,     //26 Pacific Ocean (Ohotsk sea level)
    5714   //27 MSL height
};

constexpr int NUMBER_OF_VERTICALCS = static_cast<int>(CPL_ARRAYSIZE(aoVCS));

/************************************************************************/
/*                        OSRImportFromPanorama()                       */
/************************************************************************/

/** Import coordinate system from "Panorama" GIS projection definition.
 *
 * See OGRSpatialReference::importFromPanorama()
 */

OGRErr OSRImportFromPanorama(OGRSpatialReferenceH hSRS, long iProjSys,
                             long iDatum, long iEllips, double *padfPrjParams)

{
    VALIDATE_POINTER1(hSRS, "OSRImportFromPanorama", OGRERR_FAILURE);

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->importFromPanorama(
        iProjSys, iDatum, iEllips, padfPrjParams);
}

/************************************************************************/
/*                          importFromPanorama()                        */
/************************************************************************/

/**
 * Import coordinate system from "Panorama" GIS projection definition.
 *
 * This method will import projection definition in style, used by
 * "Panorama" GIS.
 *
 * This function is the equivalent of the C function OSRImportFromPanorama().
 *
 * @param iProjSys Input projection system code, used in GIS "Panorama".
 *
 * Supported Projections are:
 * <ul>
 * <li>1:  Gauss-Kruger (Transverse Mercator)</li>
 * <li>2:  Lambert Conformal Conic 2SP</li>
 * <li>5:  Stereographic</li>
 * <li>6:  Azimuthal Equidistant (Postel)</li>
 * <li>8:  Mercator</li>
 * <li>10: Polyconic</li>
 * <li>13: Polar Stereographic</li>
 * <li>15: Gnomonic</li>
 * <li>17: Universal Transverse Mercator (UTM)</li>
 * <li>18: Wagner I (Kavraisky VI)</li>
 * <li>19: Mollweide</li>
 * <li>20: Equidistant Conic</li>
 * <li>24: Lambert Azimuthal Equal Area</li>
 * <li>27: Equirectangular</li>
 * <li>28: Cylindrical Equal Area (Lambert)</li>
 * <li>29: International Map of the World Polyconic</li>
 * </ul>
 *
 * @param iDatum Input coordinate system.
 *
 * Supported Datums are:
 * <ul>
 * <li>1:  Pulkovo, 1942</li>
 * <li>2:  WGS, 1984</li>
 * <li>3:  OSGB 1936 (British National Grid)</li>
 * <li>9:  Pulkovo, 1995</li>
 * <li>10: GSK 2011</li>
 * </ul>
 *
 * @param iEllips Input spheroid.
 *
 * Supported Spheroids are:
 * <ul>
 * <li>1:  Krassovsky, 1940</li>
 * <li>2:  WGS, 1972</li>
 * <li>3:  International, 1924 (Hayford, 1909)</li>
 * <li>4:  Clarke, 1880</li>
 * <li>5:  Clarke, 1866 (NAD1927)</li>
 * <li>6:  Everest, 1830</li>
 * <li>7:  Bessel, 1841</li>
 * <li>8:  Airy, 1830</li>
 * <li>9:  WGS, 1984 (GPS)</li>
 * <li>10: PZ-90.02</li>
 * <li>11: GRS, 1980 (NAD1983)</li>
 * <li>12: IERS 1996 (6378136.49 298.25645)</li>
 * <li>13: International, 1924 (Hayford, 1909)</li>
 * <li>14: South American, 1969</li>
 * <li>15: Indonesian, 1974</li>
 * <li>16: Helmert 1906</li>
 * <li>17: Fisher 1960</li>
 * <li>18: Fisher 1968</li>
 * <li>19. Haff 1960 - (6378270.0 297.0)</li>
 * <li>20: Everest, 1830</li>
 * <li>21: Australian National, 1965</li>
 * <li>22: CGCS2000</li>
 * <li>23: Airy Modified 1849</li>
 * <li>24: Bessel Modified</li>
 * <li>25: Bessel Namibia</li>
 * <li>26: Bessel Namibia (GLM)</li>
 * <li>27: Clarke 1880 (Arc)</li>
 * <li>28: Clarke 1880 (SGA 1922)</li>
 * <li>29: Everest (1830 Definition)</li>
 * <li>30: Everest 1830 Modified</li>
 * <li>31: Everest 1830 (RSO 1969)</li>
 * <li>32: Everest 1830 (1975 Definition)</li>
 * <li>33: NWL 9D</li>
 * <li>34: Plessis 1817</li>
 * <li>35: Struve 1860</li>
 * <li>36: War Office</li>
 * <li>37: GEM 10C</li>
 * <li>38: OSU86F</li>
 * <li>39: OSU91A</li>
 * <li>40: GRS 1967</li>
 * <li>41: Average Terrestrial System 1977</li>
 * <li>42: IAG 1975</li>
 * <li>43: GRS 1967 Modified</li>
 * <li>44: Danish 1876</li>
 * <li>45: GRS 1980 Authalic Sphere</li>
 * <li>46: GSK 2011</li>
 * <li>47: PZ-90</li>
 * </ul>
 *
 * @param padfPrjParams Array of 8 coordinate system parameters:
 *
 * <ul>
 * <li>[0]  Latitude of the first standard parallel (radians)</li>
 * <li>[1]  Latitude of the second standard parallel (radians)</li>
 * <li>[2]  Latitude of center of projection (radians)</li>
 * <li>[3]  Longitude of center of projection (radians)</li>
 * <li>[4]  Scaling factor</li>
 * <li>[5]  False Easting</li>
 * <li>[6]  False Northing</li>
 * <li>[7]  Zone number</li>
 * </ul>
 *
 * @param bNorth If northern hemisphere true, else false. Defaults to true.
 *
 * Particular projection uses different parameters, unused ones may be set to
 * zero. If NULL supplied instead of array pointer default values will be used
 * (i.e., zeroes).
 *
 * @return OGRERR_NONE on success or an error code in case of failure.
 */

OGRErr OGRSpatialReference::importFromPanorama(long iProjSys, long iDatum,
                                               long iEllips,
                                               double *padfPrjParams,
                                               bool bNorth)

{
    Clear();

    /* -------------------------------------------------------------------- */
    /*      Use safe defaults if projection parameters are not supplied.    */
    /* -------------------------------------------------------------------- */
    double adfPrjParams[8] = {0.0};
    if (padfPrjParams != nullptr)
    {
        std::copy(padfPrjParams, padfPrjParams + 8, adfPrjParams);
    }

    CPLDebug("OSR_Panorama",
             "importFromPanorama: proj %ld, datum %ld, ellips %ld, params [%f, "
             "%f, %f, %f, %f, %f, %f, %f], north %d",
             iProjSys, iDatum, iEllips, adfPrjParams[0], adfPrjParams[1],
             adfPrjParams[2], adfPrjParams[3], adfPrjParams[4], adfPrjParams[5],
             adfPrjParams[6], adfPrjParams[7], bNorth);

    // Check some zonal projections
    if ((IsNone(iEllips) || iEllips == PAN_ELLIPSOID_KRASSOVSKY) &&
        (IsNone(iDatum) || iDatum == PAN_DATUM_PULKOVO42) &&
        iProjSys == PAN_PROJ_TM)  // Pulkovo 1942 / Gauss-Kruger
    {
        int nZone = adfPrjParams[7] == 0.0
                        ? GetZoneNumberGK(TO_DEGREES * adfPrjParams[3])
                        : static_cast<int>(adfPrjParams[7]);

        if (nZone > 1 && nZone < 33)
        {
            return importFromEPSG(28400 + nZone);
        }
    }
    if ((IsNone(iEllips) || iEllips == PAN_ELLIPSOID_KRASSOVSKY) &&
        iDatum == PAN_DATUM_PULKOVO95 &&
        iProjSys == PAN_PROJ_TM)  // Pulkovo 1995 / Gauss-Kruger
    {
        int nZone = adfPrjParams[7] == 0.0
                        ? GetZoneNumberGK(TO_DEGREES * adfPrjParams[3])
                        : static_cast<int>(adfPrjParams[7]);

        if (nZone > 3 && nZone < 33)
        {
            return importFromEPSG(20000 + nZone);
        }
    }
    if (iEllips == PAN_ELLIPSOID_WGS84 && iDatum == PAN_DATUM_UTM &&
        iProjSys == PAN_PROJ_UTM)  // WGS84 / UTM
    {
        const int nZone = adfPrjParams[7] == 0.0
                              ? GetZoneNumberUTM(TO_DEGREES * adfPrjParams[3])
                              : static_cast<int>(adfPrjParams[7]);
        int nEPSG;
        if (bNorth)
        {
            nEPSG = 32600 + nZone;
        }
        else
        {
            nEPSG = 32700 + nZone;
        }
        return importFromEPSG(nEPSG);
    }

    /* -------------------------------------------------------------------- */
    /*      Operate on the basis of the projection code.                    */
    /* -------------------------------------------------------------------- */
    switch (iProjSys)
    {
        case -1L:
        case 255L:
            break;

        case PAN_PROJ_SPHERE:
            if (iEllips == PAN_ELLIPSOID_WGS84)
            {
                return SetWellKnownGeogCS("EPSG:4326");
            }
            break;

        case PAN_PROJ_UTM:
        {
            const int nZone =
                adfPrjParams[7] == 0.0
                    ? GetZoneNumberUTM(TO_DEGREES * adfPrjParams[3])
                    : static_cast<int>(adfPrjParams[7]);

            SetUTM(nZone, bNorth);
        }
        break;

        case PAN_PROJ_WAG1:
            SetWagner(1, 0.0, adfPrjParams[5], adfPrjParams[6]);
            break;

        case PAN_PROJ_MERCAT:
            SetMercator(TO_DEGREES * adfPrjParams[0],
                        TO_DEGREES * adfPrjParams[3], adfPrjParams[4],
                        adfPrjParams[5], adfPrjParams[6]);
            break;

        case PAN_PROJ_PS:
            SetPS(TO_DEGREES * adfPrjParams[2], TO_DEGREES * adfPrjParams[3],
                  adfPrjParams[4], adfPrjParams[5], adfPrjParams[6]);
            break;

        case PAN_PROJ_POLYC:
            SetPolyconic(TO_DEGREES * adfPrjParams[2],
                         TO_DEGREES * adfPrjParams[3], adfPrjParams[5],
                         adfPrjParams[6]);
            break;

        case PAN_PROJ_EC:
            SetEC(TO_DEGREES * adfPrjParams[0], TO_DEGREES * adfPrjParams[1],
                  TO_DEGREES * adfPrjParams[2], TO_DEGREES * adfPrjParams[3],
                  adfPrjParams[5], adfPrjParams[6]);
            break;

        case PAN_PROJ_LCC:
            SetLCC(TO_DEGREES * adfPrjParams[0], TO_DEGREES * adfPrjParams[1],
                   TO_DEGREES * adfPrjParams[2], TO_DEGREES * adfPrjParams[3],
                   adfPrjParams[5], adfPrjParams[6]);
            break;

        case PAN_PROJ_TM:
        {
            // XXX: we need zone number to compute false easting
            // parameter, because usually it is not contained in the
            // "Panorama" projection definition.
            // FIXME: what to do with negative values?
            int nZone = 0;
            double dfCenterLong = 0.0;

            if (adfPrjParams[7] == 0.0)
            {
                dfCenterLong = TO_DEGREES * adfPrjParams[3];
                nZone = GetZoneNumberGK(dfCenterLong);
            }
            else
            {
                nZone = static_cast<int>(adfPrjParams[7]);
                dfCenterLong = 6.0 * nZone - 3.0;
            }

            adfPrjParams[5] = nZone * 1000000.0 + 500000.0;
            adfPrjParams[4] = 1.0;
            SetTM(TO_DEGREES * adfPrjParams[2], dfCenterLong, adfPrjParams[4],
                  adfPrjParams[5], adfPrjParams[6]);
        }
        break;

        case PAN_PROJ_STEREO:
            SetStereographic(TO_DEGREES * adfPrjParams[2],
                             TO_DEGREES * adfPrjParams[3], adfPrjParams[4],
                             adfPrjParams[5], adfPrjParams[6]);
            break;

        case PAN_PROJ_AE:
            SetAE(TO_DEGREES * adfPrjParams[0], TO_DEGREES * adfPrjParams[3],
                  adfPrjParams[5], adfPrjParams[6]);
            break;

        case PAN_PROJ_GNOMON:
            SetGnomonic(TO_DEGREES * adfPrjParams[2],
                        TO_DEGREES * adfPrjParams[3], adfPrjParams[5],
                        adfPrjParams[6]);
            break;

        case PAN_PROJ_MOLL:
            SetMollweide(TO_DEGREES * adfPrjParams[3], adfPrjParams[5],
                         adfPrjParams[6]);
            break;

        case PAN_PROJ_LAEA:
            SetLAEA(TO_DEGREES * adfPrjParams[0], TO_DEGREES * adfPrjParams[3],
                    adfPrjParams[5], adfPrjParams[6]);
            break;

        case PAN_PROJ_EQC:
            SetEquirectangular(TO_DEGREES * adfPrjParams[0],
                               TO_DEGREES * adfPrjParams[3], adfPrjParams[5],
                               adfPrjParams[6]);
            break;

        case PAN_PROJ_CEA:
            SetCEA(TO_DEGREES * adfPrjParams[0], TO_DEGREES * adfPrjParams[3],
                   adfPrjParams[5], adfPrjParams[6]);
            break;

        case PAN_PROJ_IMWP:
            SetIWMPolyconic(
                TO_DEGREES * adfPrjParams[0], TO_DEGREES * adfPrjParams[1],
                TO_DEGREES * adfPrjParams[3], adfPrjParams[5], adfPrjParams[6]);
            break;

        case PAN_PROJ_MILLER:
            SetMC(TO_DEGREES * adfPrjParams[5], TO_DEGREES * adfPrjParams[4],
                  adfPrjParams[6], adfPrjParams[7]);
            break;

        case PAN_PROJ_PSEUDO_MERCATOR:
        {
            int nEPSG = 0;
            if (iEllips == PAN_ELLIPSOID_WGS84_SPHERE)
            {
                nEPSG = 3857;
            }
            else if (iEllips == PAN_ELLIPSOID_WGS84)
            {
                nEPSG = 3395;
            }
            if (nEPSG > 0)
            {
                return importFromEPSG(nEPSG);
            }
        }
        break;

        default:
            CPLDebug("OSR_Panorama", "Unsupported projection: %ld", iProjSys);
            SetLocalCS(CPLString().Printf("\"Panorama\" projection number %ld",
                                          iProjSys));
            break;
    }

    /* -------------------------------------------------------------------- */
    /*      Try to translate the datum/spheroid.                            */
    /* -------------------------------------------------------------------- */

    if (!IsLocal())
    {
        if (iEllips == PAN_ELLIPSOID_GSK2011 || iDatum == PAN_DATUM_GSK2011)
        {
            OGRSpatialReference oGCS;
            oGCS.importFromEPSG(7683);
            CopyGeogCSFrom(&oGCS);
        }
        else if (iEllips == PAN_ELLIPSOID_PZ90)
        {
            OGRSpatialReference oGCS;
            oGCS.importFromEPSG(7679);
            CopyGeogCSFrom(&oGCS);
        }
        else if (iDatum == PAN_DATUM_PULKOVO95)
        {
            OGRSpatialReference oGCS;
            oGCS.importFromEPSG(4200);
            CopyGeogCSFrom(&oGCS);
        }
        else if (iDatum > 0 && iDatum < NUMBER_OF_DATUMS && aoDatums[iDatum])
        {
            OGRSpatialReference oGCS;
            oGCS.importFromEPSG(aoDatums[iDatum]);
            CopyGeogCSFrom(&oGCS);
        }
        else if (iEllips > 0 && iEllips < NUMBER_OF_PANORAM_ELLIPSOIDS &&
                 aoPanoramaEllips[iEllips])
        {
            char *pszName = nullptr;
            double dfSemiMajor = 0.0;
            double dfInvFlattening = 0.0;

            if (OSRGetEllipsoidInfo(aoPanoramaEllips[iEllips], &pszName,
                                    &dfSemiMajor,
                                    &dfInvFlattening) == OGRERR_NONE)
            {
                SetGeogCS(
                    CPLString().Printf(
                        "Unknown datum based upon the %s ellipsoid", pszName),
                    CPLString().Printf("Not specified (based on %s spheroid)",
                                       pszName),
                    pszName, dfSemiMajor, dfInvFlattening, nullptr, 0.0,
                    nullptr, 0.0);
                SetAuthority("SPHEROID", "EPSG", aoPanoramaEllips[iEllips]);
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Failed to lookup ellipsoid code %ld. "
                         "Falling back to use Pulkovo 42.",
                         iEllips);
                SetWellKnownGeogCS("EPSG:4284");
            }

            CPLFree(pszName);
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Wrong datum code %ld. Supported datums are 1 - %d "
                     "only.  Falling back to use Pulkovo 42.",
                     iDatum, NUMBER_OF_DATUMS - 1);
            SetWellKnownGeogCS("EPSG:4284");
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Grid units translation                                          */
    /* -------------------------------------------------------------------- */
    if (IsLocal() || IsProjected())
    {
        SetLinearUnits(SRS_UL_METER, 1.0);
    }

    return OGRERR_NONE;
}

/**
 * Import vertical coordinate system from "Panorama" GIS projection definition.
 *
 * @param iVCS Input vertical coordinate system ID.
 *
 * Supported VCS are:
 * <ul>
 * <li>1:  Baltic 1977 height (EPSG:5705)</li>
 * <li>2:  AHD height (EPSG:5711)</li>
 * <li>4:  Ostend height (EPSG:5710)</li>
 * <li>5:  Ostend height (EPSG:5710)</li>
 * <li>7:  Belfast height (EPSG: 5732)</li>
 * <li>8:  Malin Head height (EPSG: 5731)</li>
 * <li>10: Piraeus height (EPSG:5716)</li>
 * <li>11: DNN height (EPSG:5733)</li>
 * <li>12: ISH2004 height (EPSG:8089)</li>
 * <li>13: Alicante height (EPSG:5782)</li>
 * <li>15: Genoa height (EPSG:5214)</li>
 * <li>16: NAP height (EPSG:5709)</li>
 * <li>17: NN54 height (EPSG:5776)</li>
 * <li>19: Cascais height (EPSG:5780)</li>
 * <li>20: N60 height (EPSG:5717)</li>
 * <li>21: RH2000 height (EPSG:5613)</li>
 * <li>23: Antalya height (EPSG:5775)</li>
 * <li>24: NGVD29 height (ftUS) (EPSG:5702)</li>
 * <li>25: Baltic 1977 height (EPSG:5705)</li>
 * <li>27: MSL height (EPSG:5714)</li>
 * </ul>
 */
OGRErr OGRSpatialReference::importVertCSFromPanorama(int iVCS)
{
    if (iVCS < 0 || iVCS >= NUMBER_OF_VERTICALCS)
    {
        return OGRERR_CORRUPT_DATA;
    }

    const int nEPSG = aoVCS[iVCS];

    if (nEPSG == 0)
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Vertical coordinate system (Panorama index %d) not supported",
                 iVCS);
        return OGRERR_UNSUPPORTED_SRS;
    }

    OGRSpatialReference sr;
    sr.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    OGRErr eImportFromEPSGErr = sr.importFromEPSG(nEPSG);
    if (eImportFromEPSGErr != OGRERR_NONE)
    {
        CPLError(CE_Warning, CPLE_None,
                 "Vertical coordinate system (Panorama index %d, EPSG %d) "
                 "import from EPSG error",
                 iVCS, nEPSG);
        return OGRERR_UNSUPPORTED_SRS;
    }

    if (sr.IsVertical() != 1)
    {
        CPLError(CE_Warning, CPLE_None,
                 "Coordinate system (Panorama index %d, EPSG %d) "
                 "is not Vertical",
                 iVCS, nEPSG);
        return OGRERR_UNSUPPORTED_SRS;
    }

    OGRErr eSetVertCSErr =
        SetVertCS(sr.GetAttrValue("VERT_CS"), sr.GetAttrValue("VERT_DATUM"));
    if (eSetVertCSErr != OGRERR_NONE)
    {
        CPLError(CE_Warning, CPLE_None,
                 "Vertical coordinate system (Panorama index %d, EPSG %d) "
                 "set error",
                 iVCS, nEPSG);
        return eSetVertCSErr;
    }
    return OGRERR_NONE;
}

/**
 * Export vertical coordinate system to "Panorama" GIS projection definition.
 */
OGRErr OGRSpatialReference::exportVertCSToPanorama(int *piVert) const
{
    auto pszVertCSName = GetAttrValue("COMPD_CS|VERT_CS");
    if (pszVertCSName != nullptr)
    {
        auto pszValue = GetAuthorityCode("COMPD_CS|VERT_CS");
        if (pszValue != nullptr)
        {
            auto nEPSG = atoi(pszValue);
            if (nEPSG > 0)
            {
                for (int i = 0; i < NUMBER_OF_VERTICALCS; i++)
                {
                    if (aoVCS[i] == nEPSG)
                    {
                        *piVert = i;
                        return OGRERR_NONE;
                    }
                }
            }
        }
        else  // Try to get Panorama ID from pszVertCSName
        {
            for (int i = 0; i < NUMBER_OF_VERTICALCS; i++)
            {
                if (aoVCS[i] > 0)
                {
                    OGRSpatialReference oTmpSRS;
                    oTmpSRS.importFromEPSG(aoVCS[i]);
                    if (EQUAL(pszVertCSName, oTmpSRS.GetAttrValue("VERT_CS")))
                    {

                        *piVert = i;
                        return OGRERR_NONE;
                    }
                }
            }
        }
    }
    CPLDebug("OSR_Panorama",
             "Vertical coordinate system not supported by Panorama");
    return OGRERR_UNSUPPORTED_SRS;
}

/************************************************************************/
/*                      OSRExportToPanorama()                           */
/************************************************************************/

/** Export coordinate system in "Panorama" GIS projection definition.
 *
 * See OGRSpatialReference::exportToPanorama()
 */

OGRErr OSRExportToPanorama(OGRSpatialReferenceH hSRS, long *piProjSys,
                           long *piDatum, long *piEllips, long *piZone,
                           double *padfPrjParams)

{
    VALIDATE_POINTER1(hSRS, "OSRExportToPanorama", OGRERR_FAILURE);
    VALIDATE_POINTER1(piProjSys, "OSRExportToPanorama", OGRERR_FAILURE);
    VALIDATE_POINTER1(piDatum, "OSRExportToPanorama", OGRERR_FAILURE);
    VALIDATE_POINTER1(piEllips, "OSRExportToPanorama", OGRERR_FAILURE);
    VALIDATE_POINTER1(padfPrjParams, "OSRExportToPanorama", OGRERR_FAILURE);

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->exportToPanorama(
        piProjSys, piDatum, piEllips, piZone, padfPrjParams);
}

/************************************************************************/
/*                           exportToPanorama()                         */
/************************************************************************/

/**
 * Export coordinate system in "Panorama" GIS projection definition.
 *
 * This method is the equivalent of the C function OSRExportToPanorama().
 *
 * @param piProjSys Pointer to variable, where the projection system code will
 * be returned.
 *
 * @param piDatum Pointer to variable, where the coordinate system code will
 * be returned.
 *
 * @param piEllips Pointer to variable, where the spheroid code will be
 * returned.
 *
 * @param piZone Pointer to variable, where the zone for UTM projection
 * system will be returned.
 *
 * @param padfPrjParams an existing 7 double buffer into which the
 * projection parameters will be placed. See importFromPanorama()
 * for the list of parameters.
 *
 * @return OGRERR_NONE on success or an error code on failure.
 */

OGRErr OGRSpatialReference::exportToPanorama(long *piProjSys, long *piDatum,
                                             long *piEllips, long *piZone,
                                             double *padfPrjParams) const

{
    CPLAssert(padfPrjParams);

    const char *pszProjection = GetAttrValue("PROJECTION");
    int nEPSG = 0;
    auto pszEPSG = GetAuthorityCode("PROJCS");
    if (pszEPSG == nullptr)
    {
        pszEPSG = GetAuthorityCode("GEOGCS");
    }
    if (pszEPSG != nullptr)
    {
        nEPSG = atoi(pszEPSG);
    }

    /* -------------------------------------------------------------------- */
    /*      Fill all projection parameters with zero.                       */
    /* -------------------------------------------------------------------- */
    *piDatum = 0L;
    *piEllips = 0L;
    *piZone = 0L;
    for (int i = 0; i < 7; i++)
        padfPrjParams[i] = 0.0;

    /* ==================================================================== */
    /*      Handle the projection definition.                               */
    /* ==================================================================== */
    if (IsLocal())
    {
        *piProjSys = NONE_VAL;
    }
    else if (IsGeographic() || IsGeocentric())
    {
        *piProjSys = PAN_PROJ_SPHERE;
    }
    // Check well known EPSG codes
    else if (nEPSG == 3857)
    {
        *piProjSys = PAN_PROJ_PSEUDO_MERCATOR;
        *piDatum = PAN_DATUM_RECTANGULAR;
        *piEllips = PAN_ELLIPSOID_WGS84_SPHERE;
        return OGRERR_NONE;
    }
    else if (pszProjection == nullptr)
    {
#ifdef DEBUG
        CPLDebug("OSR_Panorama",
                 "Empty projection definition, considered as Geographic");
#endif
        *piProjSys = NONE_VAL;
    }
    else if (EQUAL(pszProjection, SRS_PT_MERCATOR_1SP))
    {
        *piProjSys = PAN_PROJ_MERCAT;
        padfPrjParams[3] =
            TO_RADIANS * GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);
        padfPrjParams[0] =
            TO_RADIANS * GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0);
        padfPrjParams[4] = GetNormProjParm(SRS_PP_SCALE_FACTOR, 1.0);
        padfPrjParams[5] = GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0);
        padfPrjParams[6] = GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0);
    }
    else if (EQUAL(pszProjection, SRS_PT_POLAR_STEREOGRAPHIC))
    {
        *piProjSys = PAN_PROJ_PS;
        padfPrjParams[3] =
            TO_RADIANS * GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);
        padfPrjParams[2] =
            TO_RADIANS * GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0);
        padfPrjParams[4] = GetNormProjParm(SRS_PP_SCALE_FACTOR, 1.0);
        padfPrjParams[5] = GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0);
        padfPrjParams[6] = GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0);
    }
    else if (EQUAL(pszProjection, SRS_PT_POLYCONIC))
    {
        *piProjSys = PAN_PROJ_POLYC;
        padfPrjParams[3] =
            TO_RADIANS * GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);
        padfPrjParams[2] =
            TO_RADIANS * GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0);
        padfPrjParams[5] = GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0);
        padfPrjParams[6] = GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0);
    }
    else if (EQUAL(pszProjection, SRS_PT_EQUIDISTANT_CONIC))
    {
        *piProjSys = PAN_PROJ_EC;
        padfPrjParams[0] =
            TO_RADIANS * GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1, 0.0);
        padfPrjParams[1] =
            TO_RADIANS * GetNormProjParm(SRS_PP_STANDARD_PARALLEL_2, 0.0);
        padfPrjParams[3] =
            TO_RADIANS * GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);
        padfPrjParams[2] =
            TO_RADIANS * GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0);
        padfPrjParams[5] = GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0);
        padfPrjParams[6] = GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0);
    }
    else if (EQUAL(pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP))
    {
        *piProjSys = PAN_PROJ_LCC;
        padfPrjParams[0] =
            TO_RADIANS * GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1, 0.0);
        padfPrjParams[1] =
            TO_RADIANS * GetNormProjParm(SRS_PP_STANDARD_PARALLEL_2, 0.0);
        padfPrjParams[3] =
            TO_RADIANS * GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);
        padfPrjParams[2] =
            TO_RADIANS * GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0);
        padfPrjParams[5] = GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0);
        padfPrjParams[6] = GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0);
    }
    else if (EQUAL(pszProjection, SRS_PT_TRANSVERSE_MERCATOR))
    {
        int bNorth = FALSE;

        *piZone = GetUTMZone(&bNorth);

        auto dfCenterLong = GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);
        padfPrjParams[3] = TO_RADIANS * dfCenterLong;
        padfPrjParams[2] =
            TO_RADIANS * GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0);
        padfPrjParams[4] = GetNormProjParm(SRS_PP_SCALE_FACTOR, 1.0);
        padfPrjParams[5] = GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0);
        padfPrjParams[6] = GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0);

        if (*piZone != 0)
        {
            *piProjSys = PAN_PROJ_UTM;
            if (!bNorth)
                *piZone = -*piZone;
        }
        else
        {
            *piProjSys = PAN_PROJ_TM;
            auto nZone = GetZoneNumberGK(dfCenterLong);
            *piZone = nZone;
        }
    }
    else if (EQUAL(pszProjection, SRS_PT_WAGNER_I))
    {
        *piProjSys = PAN_PROJ_WAG1;
        padfPrjParams[5] = GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0);
        padfPrjParams[6] = GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0);
    }
    else if (EQUAL(pszProjection, SRS_PT_STEREOGRAPHIC))
    {
        *piProjSys = PAN_PROJ_STEREO;
        padfPrjParams[3] =
            TO_RADIANS * GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);
        padfPrjParams[2] =
            TO_RADIANS * GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0);
        padfPrjParams[4] = GetNormProjParm(SRS_PP_SCALE_FACTOR, 1.0);
        padfPrjParams[5] = GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0);
        padfPrjParams[6] = GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0);
    }
    else if (EQUAL(pszProjection, SRS_PT_AZIMUTHAL_EQUIDISTANT))
    {
        *piProjSys = PAN_PROJ_AE;
        padfPrjParams[3] =
            TO_RADIANS * GetNormProjParm(SRS_PP_LONGITUDE_OF_CENTER, 0.0);
        padfPrjParams[0] =
            TO_RADIANS * GetNormProjParm(SRS_PP_LATITUDE_OF_CENTER, 0.0);
        padfPrjParams[5] = GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0);
        padfPrjParams[6] = GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0);
    }
    else if (EQUAL(pszProjection, SRS_PT_GNOMONIC))
    {
        *piProjSys = PAN_PROJ_GNOMON;
        padfPrjParams[3] =
            TO_RADIANS * GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);
        padfPrjParams[2] =
            TO_RADIANS * GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0);
        padfPrjParams[5] = GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0);
        padfPrjParams[6] = GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0);
    }
    else if (EQUAL(pszProjection, SRS_PT_MOLLWEIDE))
    {
        *piProjSys = PAN_PROJ_MOLL;
        padfPrjParams[3] =
            TO_RADIANS * GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);
        padfPrjParams[5] = GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0);
        padfPrjParams[6] = GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0);
    }
    else if (EQUAL(pszProjection, SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA))
    {
        *piProjSys = PAN_PROJ_LAEA;
        padfPrjParams[3] =
            TO_RADIANS * GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);
        padfPrjParams[0] =
            TO_RADIANS * GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0);
        padfPrjParams[5] = GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0);
        padfPrjParams[6] = GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0);
    }
    else if (EQUAL(pszProjection, SRS_PT_EQUIRECTANGULAR))
    {
        *piProjSys = PAN_PROJ_EQC;
        padfPrjParams[3] =
            TO_RADIANS * GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);
        padfPrjParams[0] =
            TO_RADIANS * GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0);
        padfPrjParams[5] = GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0);
        padfPrjParams[6] = GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0);
    }
    else if (EQUAL(pszProjection, SRS_PT_CYLINDRICAL_EQUAL_AREA))
    {
        *piProjSys = PAN_PROJ_CEA;
        padfPrjParams[3] =
            TO_RADIANS * GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);
        padfPrjParams[2] =
            TO_RADIANS * GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1, 0.0);
        padfPrjParams[5] = GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0);
        padfPrjParams[6] = GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0);
    }
    else if (EQUAL(pszProjection, SRS_PT_IMW_POLYCONIC))
    {
        *piProjSys = PAN_PROJ_IMWP;
        padfPrjParams[3] =
            TO_RADIANS * GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);
        padfPrjParams[0] =
            TO_RADIANS * GetNormProjParm(SRS_PP_LATITUDE_OF_1ST_POINT, 0.0);
        padfPrjParams[1] =
            TO_RADIANS * GetNormProjParm(SRS_PP_LATITUDE_OF_2ND_POINT, 0.0);
        padfPrjParams[5] = GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0);
        padfPrjParams[6] = GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0);
    }
    // Projection unsupported by "Panorama" GIS
    else
    {
        CPLDebug("OSR_Panorama",
                 "Projection \"%s\" unsupported by \"Panorama\" GIS. "
                 "Geographic system will be used.",
                 pszProjection);
        *piProjSys = NONE_VAL;
    }

    /* -------------------------------------------------------------------- */
    /*      Translate the datum.                                            */
    /* -------------------------------------------------------------------- */
    const char *pszDatum = GetAttrValue("DATUM");

    if (pszDatum == nullptr)
    {
        *piDatum = NONE_VAL;
        *piEllips = NONE_VAL;
    }
    else if (EQUAL(pszDatum, "Pulkovo_1942"))
    {
        *piDatum = PAN_DATUM_PULKOVO42;
        *piEllips = PAN_ELLIPSOID_KRASSOVSKY;
    }
    else if (EQUAL(pszDatum, "Pulkovo_1995"))
    {
        *piDatum = PAN_DATUM_PULKOVO95;
        *piEllips = PAN_ELLIPSOID_KRASSOVSKY;
    }
    else if (EQUAL(pszDatum, SRS_DN_WGS84))
    {
        *piDatum = PAN_DATUM_RECTANGULAR;  // PAN_DATUM_WGS84;
        *piEllips = PAN_ELLIPSOID_WGS84;
    }

    // If not found well known datum, translate ellipsoid.
    else
    {
        const double dfSemiMajor = GetSemiMajor();
        const double dfInvFlattening = GetInvFlattening();

#ifdef DEBUG
        CPLDebug("OSR_Panorama",
                 "Datum \"%s\" unsupported by \"Panorama\" GIS. "
                 "Trying to translate an ellipsoid definition.",
                 pszDatum);
#endif

        int i = 0;  // Used after for.
        for (; i < NUMBER_OF_PANORAM_ELLIPSOIDS; i++)
        {
            if (aoPanoramaEllips[i])
            {
                double dfSM = 0.0;
                double dfIF = 1.0;

                if (OSRGetEllipsoidInfo(aoPanoramaEllips[i], nullptr, &dfSM,
                                        &dfIF) == OGRERR_NONE &&
                    std::abs(dfSemiMajor - dfSM) < 1e-10 * dfSemiMajor &&
                    std::abs(dfInvFlattening - dfIF) < 1e-10 * dfInvFlattening)
                {
                    *piEllips = i;
                    break;
                }
            }
        }

        if (i == NUMBER_OF_PANORAM_ELLIPSOIDS)  // Didn't found matches.
        {
#ifdef DEBUG
            CPLDebug("OSR_Panorama",
                     R"(Ellipsoid "%s" unsupported by "Panorama" GIS.)",
                     pszDatum);
#endif
            *piDatum = NONE_VAL;
            *piEllips = NONE_VAL;
        }
    }

    return OGRERR_NONE;
}

/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGRSpatialReference translation to/from USGS georeferencing
 *           information (used in GCTP package).
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2004, Andrey Kiselev <dron@ak4719.spb.edu>
 * Copyright (c) 2008-2009, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_port.h"
#include "ogr_srs_api.h"

#include <cmath>
#include <cstddef>

#include "cpl_conv.h"
#include "cpl_csv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "ogr_core.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*  GCTP projection codes.                                              */
/************************************************************************/

static const long GEO    = 0L;   // Geographic
static const long UTM    = 1L;   // Universal Transverse Mercator (UTM)
static const long SPCS   = 2L;   // State Plane Coordinates
static const long ALBERS = 3L;   // Albers Conical Equal Area
static const long LAMCC  = 4L;   // Lambert Conformal Conic
static const long MERCAT = 5L;   // Mercator
static const long PS     = 6L;   // Polar Stereographic
static const long POLYC  = 7L;   // Polyconic
static const long EQUIDC = 8L;   // Equidistant Conic
static const long TM     = 9L;   // Transverse Mercator
static const long STEREO = 10L;  // Stereographic
static const long LAMAZ  = 11L;  // Lambert Azimuthal Equal Area
static const long AZMEQD = 12L;  // Azimuthal Equidistant
static const long GNOMON = 13L;  // Gnomonic
static const long ORTHO  = 14L;  // Orthographic
// static const long GVNSP  = 15L;  // General Vertical Near-Side Perspective
static const long SNSOID = 16L;  // Sinusiodal
static const long EQRECT = 17L;  // Equirectangular
static const long MILLER = 18L;  // Miller Cylindrical
static const long VGRINT = 19L;  // Van der Grinten
static const long HOM    = 20L;  // (Hotine) Oblique Mercator
static const long ROBIN  = 21L;  // Robinson
// static const long SOM    = 22L;  // Space Oblique Mercator (SOM)
// static const long ALASKA = 23L;  // Alaska Conformal
// static const long GOODE  = 24L;  // Interrupted Goode Homolosine
static const long MOLL   = 25L;  // Mollweide
// static const long IMOLL  = 26L;  // Interrupted Mollweide
// static const long HAMMER = 27L;  // Hammer
static const long WAGIV  = 28L;  // Wagner IV
static const long WAGVII = 29L;  // Wagner VII
// static const long OBEQA  = 30L;  // Oblated Equal Area
// static const long ISINUS1 = 31L; // Integerized Sinusoidal Grid (the same as 99)
// static const long CEA    = 97L;  // Cylindrical Equal Area (Grid corners set
                                 // in meters for EASE grid)
// static const long BCEA   = 98L;  // Cylindrical Equal Area (Grid corners set
                                 // in DMS degs for EASE grid)
// static const long ISINUS = 99L;  // Integerized Sinusoidal Grid
                                 // (added by Raj Gejjagaraguppe ARC for MODIS)

/************************************************************************/
/*  GCTP ellipsoid codes.                                               */
/************************************************************************/

static const long CLARKE1866         = 0L;
// static const long CLARKE1880         = 1L;
// static const long BESSEL             = 2L;
// static const long INTERNATIONAL1967  = 3L;
// static const long INTERNATIONAL1909  = 4L;
// static const long WGS72              = 5L;
// static const long EVEREST            = 6L;
// static const long WGS66              = 7L;
static const long GRS1980            = 8L;
// static const long AIRY               = 9L;
// static const long MODIFIED_EVEREST   = 10L;
// static const long MODIFIED_AIRY      = 11L;
static const long WGS84              = 12L;
// static const long SOUTHEAST_ASIA     = 13L;
// static const long AUSTRALIAN_NATIONAL= 14L;
// static const long KRASSOVSKY         = 15L;
// static const long HOUGH              = 16L;
// static const long MERCURY1960        = 17L;
// static const long MODIFIED_MERCURY   = 18L;
// static const long SPHERE             = 19L;

/************************************************************************/
/*  Correspondence between GCTP and EPSG ellipsoid codes.               */
/************************************************************************/

static const int aoEllips[] =
{
    7008,   // Clarke, 1866 (NAD1927)
    7034,   // Clarke, 1880
    7004,   // Bessel, 1841
    0,      // FIXME: New International, 1967 --- skipped
    7022,   // International, 1924 (Hayford, 1909) XXX?
    7043,   // WGS, 1972
    7042,   // Everest, 1830
    7025,   // FIXME: WGS, 1966
    7019,   // GRS, 1980 (NAD1983)
    7001,   // Airy, 1830
    7018,   // Modified Everest
    7002,   // Modified Airy
    7030,   // WGS, 1984 (GPS)
    0,      // FIXME: Southeast Asia --- skipped
    7003,   // Australian National, 1965
    7024,   // Krassovsky, 1940
    7053,   // Hough
    0,      // FIXME: Mercury, 1960 --- skipped
    0,      // FIXME: Modified Mercury, 1968 --- skipped
    7047,   // Sphere, rad 6370997 m (normal sphere)
    7006,   // Bessel, 1841 (Namibia)
    7016,   // Everest (Sabah & Sarawak)
    7044,   // Everest, 1956
    7056,   // Everest, Malaysia 1969
    7018,   // Everest, Malay & Singapr 1948
    0,      // FIXME: Everest, Pakistan --- skipped
    7022,   // Hayford (International 1924) XXX?
    7020,   // Helmert 1906
    7021,   // Indonesian, 1974
    7036,   // South American, 1969
    0       // FIXME: WGS 60 --- skipped
};

#define NUMBER_OF_ELLIPSOIDS    (int)(sizeof(aoEllips)/sizeof(aoEllips[0]))

/************************************************************************/
/*                         OSRImportFromUSGS()                          */
/************************************************************************/

/**
 * \brief Import coordinate system from USGS projection definition.
 *
 * This function is the same as OGRSpatialReference::importFromUSGS().
 */
OGRErr OSRImportFromUSGS( OGRSpatialReferenceH hSRS, long iProjsys,
                          long iZone, double *padfPrjParams, long iDatum )

{
    VALIDATE_POINTER1( hSRS, "OSRImportFromUSGS", OGRERR_FAILURE );

    return ((OGRSpatialReference *) hSRS)->importFromUSGS( iProjsys, iZone,
                                                           padfPrjParams,
                                                           iDatum );
}

static double OGRSpatialReferenceUSGSUnpackNoOp( double dfVal )
{
    return dfVal;
}

static double OGRSpatialReferenceUSGSUnpackRadian( double dfVal )
{
    return dfVal * 180.0 / M_PI;
}

/************************************************************************/
/*                          importFromUSGS()                            */
/************************************************************************/

/**
 * \brief Import coordinate system from USGS projection definition.
 *
 * This method will import projection definition in style, used by USGS GCTP
 * software. GCTP operates on angles in packed DMS format (see
 * CPLDecToPackedDMS() function for details), so all angle values (latitudes,
 * longitudes, azimuths, etc.) specified in the padfPrjParams array should
 * be in the packed DMS format, unless bAnglesInPackedDMSFormat is set to FALSE.
 *
 * This function is the equivalent of the C function OSRImportFromUSGS().
 * Note that the bAnglesInPackedDMSFormat parameter is only present in the C++
 * method. The C function assumes bAnglesInPackedFormat = TRUE.
 *
 * @param iProjSys Input projection system code, used in GCTP.
 *
 * @param iZone Input zone for UTM and State Plane projection systems. For
 * Southern Hemisphere UTM use a negative zone code. iZone ignored for all
 * other projections.
 *
 * @param padfPrjParams Array of 15 coordinate system parameters. These
 * parameters differs for different projections.
 *
 *        <h4>Projection Transformation Package Projection Parameters</h4>
 * <pre>
 * ----------------------------------------------------------------------------
 *                         |                    Array Element
 *  Code & Projection Id   |---------------------------------------------------
 *                         |   0  |   1  |  2   |  3   |   4   |    5    |6 | 7
 * ----------------------------------------------------------------------------
 *  0 Geographic           |      |      |      |      |       |         |  |
 *  1 U T M                |Lon/Z |Lat/Z |      |      |       |         |  |
 *  2 State Plane          |      |      |      |      |       |         |  |
 *  3 Albers Equal Area    |SMajor|SMinor|STDPR1|STDPR2|CentMer|OriginLat|FE|FN
 *  4 Lambert Conformal C  |SMajor|SMinor|STDPR1|STDPR2|CentMer|OriginLat|FE|FN
 *  5 Mercator             |SMajor|SMinor|      |      |CentMer|TrueScale|FE|FN
 *  6 Polar Stereographic  |SMajor|SMinor|      |      |LongPol|TrueScale|FE|FN
 *  7 Polyconic            |SMajor|SMinor|      |      |CentMer|OriginLat|FE|FN
 *  8 Equid. Conic A       |SMajor|SMinor|STDPAR|      |CentMer|OriginLat|FE|FN
 *    Equid. Conic B       |SMajor|SMinor|STDPR1|STDPR2|CentMer|OriginLat|FE|FN
 *  9 Transverse Mercator  |SMajor|SMinor|Factor|      |CentMer|OriginLat|FE|FN
 * 10 Stereographic        |Sphere|      |      |      |CentLon|CenterLat|FE|FN
 * 11 Lambert Azimuthal    |Sphere|      |      |      |CentLon|CenterLat|FE|FN
 * 12 Azimuthal            |Sphere|      |      |      |CentLon|CenterLat|FE|FN
 * 13 Gnomonic             |Sphere|      |      |      |CentLon|CenterLat|FE|FN
 * 14 Orthographic         |Sphere|      |      |      |CentLon|CenterLat|FE|FN
 * 15 Gen. Vert. Near Per  |Sphere|      |Height|      |CentLon|CenterLat|FE|FN
 * 16 Sinusoidal           |Sphere|      |      |      |CentMer|         |FE|FN
 * 17 Equirectangular      |Sphere|      |      |      |CentMer|TrueScale|FE|FN
 * 18 Miller Cylindrical   |Sphere|      |      |      |CentMer|         |FE|FN
 * 19 Van der Grinten      |Sphere|      |      |      |CentMer|OriginLat|FE|FN
 * 20 Hotin Oblique Merc A |SMajor|SMinor|Factor|      |       |OriginLat|FE|FN
 *    Hotin Oblique Merc B |SMajor|SMinor|Factor|AziAng|AzmthPt|OriginLat|FE|FN
 * 21 Robinson             |Sphere|      |      |      |CentMer|         |FE|FN
 * 22 Space Oblique Merc A |SMajor|SMinor|      |IncAng|AscLong|         |FE|FN
 *    Space Oblique Merc B |SMajor|SMinor|Satnum|Path  |       |         |FE|FN
 * 23 Alaska Conformal     |SMajor|SMinor|      |      |       |         |FE|FN
 * 24 Interrupted Goode    |Sphere|      |      |      |       |         |  |
 * 25 Mollweide            |Sphere|      |      |      |CentMer|         |FE|FN
 * 26 Interrupt Mollweide  |Sphere|      |      |      |       |         |  |
 * 27 Hammer               |Sphere|      |      |      |CentMer|         |FE|FN
 * 28 Wagner IV            |Sphere|      |      |      |CentMer|         |FE|FN
 * 29 Wagner VII           |Sphere|      |      |      |CentMer|         |FE|FN
 * 30 Oblated Equal Area   |Sphere|      |Shapem|Shapen|CentLon|CenterLat|FE|FN
 * ----------------------------------------------------------------------------
 *
 *       ----------------------------------------------------
 *                               |      Array Element       |
 *         Code & Projection Id  |---------------------------
 *                               |  8  |  9 |  10 | 11 | 12 |
 *       ----------------------------------------------------
 *        0 Geographic           |     |    |     |    |    |
 *        1 U T M                |     |    |     |    |    |
 *        2 State Plane          |     |    |     |    |    |
 *        3 Albers Equal Area    |     |    |     |    |    |
 *        4 Lambert Conformal C  |     |    |     |    |    |
 *        5 Mercator             |     |    |     |    |    |
 *        6 Polar Stereographic  |     |    |     |    |    |
 *        7 Polyconic            |     |    |     |    |    |
 *        8 Equid. Conic A       |zero |    |     |    |    |
 *          Equid. Conic B       |one  |    |     |    |    |
 *        9 Transverse Mercator  |     |    |     |    |    |
 *       10 Stereographic        |     |    |     |    |    |
 *       11 Lambert Azimuthal    |     |    |     |    |    |
 *       12 Azimuthal            |     |    |     |    |    |
 *       13 Gnomonic             |     |    |     |    |    |
 *       14 Orthographic         |     |    |     |    |    |
 *       15 Gen. Vert. Near Per  |     |    |     |    |    |
 *       16 Sinusoidal           |     |    |     |    |    |
 *       17 Equirectangular      |     |    |     |    |    |
 *       18 Miller Cylindrical   |     |    |     |    |    |
 *       19 Van der Grinten      |     |    |     |    |    |
 *       20 Hotin Oblique Merc A |Long1|Lat1|Long2|Lat2|zero|
 *          Hotin Oblique Merc B |     |    |     |    |one |
 *       21 Robinson             |     |    |     |    |    |
 *       22 Space Oblique Merc A |PSRev|LRat|PFlag|    |zero|
 *          Space Oblique Merc B |     |    |     |    |one |
 *       23 Alaska Conformal     |     |    |     |    |    |
 *       24 Interrupted Goode    |     |    |     |    |    |
 *       25 Mollweide            |     |    |     |    |    |
 *       26 Interrupt Mollweide  |     |    |     |    |    |
 *       27 Hammer               |     |    |     |    |    |
 *       28 Wagner IV            |     |    |     |    |    |
 *       29 Wagner VII           |     |    |     |    |    |
 *       30 Oblated Equal Area   |Angle|    |     |    |    |
 *       ----------------------------------------------------
 *
 *   where
 *
 *    Lon/Z     Longitude of any point in the UTM zone or zero.  If zero,
 *              a zone code must be specified.
 *    Lat/Z     Latitude of any point in the UTM zone or zero.  If zero, a
 *              zone code must be specified.
 *    SMajor    Semi-major axis of ellipsoid.  If zero, Clarke 1866 in meters
 *              is assumed.
 *    SMinor    Eccentricity squared of the ellipsoid if less than zero,
 *              if zero, a spherical form is assumed, or if greater than
 *              zero, the semi-minor axis of ellipsoid.
 *    Sphere    Radius of reference sphere.  If zero, 6370997 meters is used.
 *    STDPAR    Latitude of the standard parallel
 *    STDPR1    Latitude of the first standard parallel
 *    STDPR2    Latitude of the second standard parallel
 *    CentMer   Longitude of the central meridian
 *    OriginLat Latitude of the projection origin
 *    FE        False easting in the same units as the semi-major axis
 *    FN        False northing in the same units as the semi-major axis
 *    TrueScale Latitude of true scale
 *    LongPol   Longitude down below pole of map
 *    Factor    Scale factor at central meridian (Transverse Mercator) or
 *              center of projection (Hotine Oblique Mercator)
 *    CentLon   Longitude of center of projection
 *    CenterLat Latitude of center of projection
 *    Height    Height of perspective point
 *    Long1     Longitude of first point on center line (Hotine Oblique
 *              Mercator, format A)
 *    Long2     Longitude of second point on center line (Hotine Oblique
 *              Mercator, format A)
 *    Lat1      Latitude of first point on center line (Hotine Oblique
 *              Mercator, format A)
 *    Lat2      Latitude of second point on center line (Hotine Oblique
 *              Mercator, format A)
 *    AziAng    Azimuth angle east of north of center line (Hotine Oblique
 *              Mercator, format B)
 *    AzmthPt   Longitude of point on central meridian where azimuth occurs
 *              (Hotine Oblique Mercator, format B)
 *    IncAng    Inclination of orbit at ascending node, counter-clockwise
 *              from equator (SOM, format A)
 *    AscLong   Longitude of ascending orbit at equator (SOM, format A)
 *    PSRev     Period of satellite revolution in minutes (SOM, format A)
 *    LRat      Landsat ratio to compensate for confusion at northern end
 *              of orbit (SOM, format A -- use 0.5201613)
 *    PFlag     End of path flag for Landsat:  0 = start of path,
 *              1 = end of path (SOM, format A)
 *    Satnum    Landsat Satellite Number (SOM, format B)
 *    Path      Landsat Path Number (Use WRS-1 for Landsat 1, 2 and 3 and
 *              WRS-2 for Landsat 4, 5 and 6.)  (SOM, format B)
 *    Shapem    Oblated Equal Area oval shape parameter m
 *    Shapen    Oblated Equal Area oval shape parameter n
 *    Angle     Oblated Equal Area oval rotation angle
 *
 * Array elements 13 and 14 are set to zero. All array elements with blank
 * fields are set to zero too.
 * </pre>
 *
 * @param iDatum Input spheroid.<p>
 *
 * If the datum code is negative, the first two values in the parameter array
 * (parm) are used to define the values as follows:
 *
 * <ul>
 *
 * <li> If padfPrjParams[0] is a non-zero value and padfPrjParams[1] is
 * greater than one, the semimajor axis is set to padfPrjParams[0] and
 * the semiminor axis is set to padfPrjParams[1].
 *
 * <li> If padfPrjParams[0] is nonzero and padfPrjParams[1] is greater than
 * zero but less than or equal to one, the semimajor axis is set to
 * padfPrjParams[0] and the semiminor axis is computed from the eccentricity
 * squared value padfPrjParams[1]:<p>
 *
 * semiminor = sqrt(1.0 - ES) * semimajor<p>
 *
 * where<p>
 *
 * ES = eccentricity squared
 *
 * <li> If padfPrjParams[0] is nonzero and padfPrjParams[1] is equal to zero,
 * the semimajor axis and semiminor axis are set to padfPrjParams[0].
 *
 * <li> If padfPrjParams[0] equals zero and padfPrjParams[1] is greater than
 * zero, the default Clarke 1866 is used to assign values to the semimajor
 * axis and semiminor axis.
 *
 * <li> If padfPrjParams[0] and padfPrjParams[1] equals zero, the semimajor
 * axis is set to 6370997.0 and the semiminor axis is set to zero.
 *
 * </ul>
 *
 * If a datum code is zero or greater, the semimajor and semiminor axis are
 * defined by the datum code as found in the following table:
 *
 *      <h4>Supported Datums</h4>
 * <pre>
 *       0: Clarke 1866 (default)
 *       1: Clarke 1880
 *       2: Bessel
 *       3: International 1967
 *       4: International 1909
 *       5: WGS 72
 *       6: Everest
 *       7: WGS 66
 *       8: GRS 1980/WGS 84
 *       9: Airy
 *      10: Modified Everest
 *      11: Modified Airy
 *      12: Walbeck
 *      13: Southeast Asia
 *      14: Australian National
 *      15: Krassovsky
 *      16: Hough
 *      17: Mercury 1960
 *      18: Modified Mercury 1968
 *      19: Sphere of Radius 6370997 meters
 * </pre>
 *
 * @param nUSGSAngleFormat one of USGS_ANGLE_DECIMALDEGREES,
 *    USGS_ANGLE_PACKEDDMS, or USGS_ANGLE_RADIANS (default is
 *    USGS_ANGLE_PACKEDDMS).
 *
 * @return OGRERR_NONE on success or an error code in case of failure.
 */

OGRErr OGRSpatialReference::importFromUSGS( long iProjSys, long iZone,
                                            double *padfPrjParams,
                                            long iDatum,
                                            int nUSGSAngleFormat )

{
    if( !padfPrjParams )
        return OGRERR_CORRUPT_DATA;

    double (*pfnUnpackAnglesFn)(double) = NULL;

    if( nUSGSAngleFormat == USGS_ANGLE_DECIMALDEGREES )
        pfnUnpackAnglesFn = OGRSpatialReferenceUSGSUnpackNoOp;
    else if( nUSGSAngleFormat == USGS_ANGLE_RADIANS )
        pfnUnpackAnglesFn = OGRSpatialReferenceUSGSUnpackRadian;
    else
        pfnUnpackAnglesFn = CPLPackedDMSToDec;

/* -------------------------------------------------------------------- */
/*      Operate on the basis of the projection code.                    */
/* -------------------------------------------------------------------- */
    switch( iProjSys )
    {
        case GEO:
            break;

        case UTM:
            {
                int bNorth = TRUE;

                if( !iZone )
                {
                    if( padfPrjParams[2] != 0.0 )
                    {
                        iZone = static_cast<long>(padfPrjParams[2]);
                    }
                    else if( padfPrjParams[0] != 0.0 &&
                             padfPrjParams[1] != 0.0 )
                    {
                        const double dfUnpackedAngle =
                            pfnUnpackAnglesFn(padfPrjParams[0]);
                        iZone = static_cast<long>(
                            ((dfUnpackedAngle + 180.0) / 6.0) + 1.0);
                        if( dfUnpackedAngle < 0 )
                            bNorth = FALSE;
                    }
                }

                if( iZone < 0 )
                {
                    iZone = -iZone;
                    bNorth = FALSE;
                }
                SetUTM( static_cast<int>(iZone), bNorth );
            }
            break;

        case SPCS:
            {
                int bNAD83 = TRUE;

                if( iDatum == 0 )
                    bNAD83 = FALSE;
                else if( iDatum != 8 )
                    CPLError( CE_Warning, CPLE_AppDefined,
                              "Wrong datum for State Plane projection %d. "
                              "Should be 0 or 8.", static_cast<int>(iDatum) );

                SetStatePlane( static_cast<int>(iZone), bNAD83 );
            }
            break;

        case ALBERS:
            SetACEA( pfnUnpackAnglesFn(padfPrjParams[2]),
                     pfnUnpackAnglesFn(padfPrjParams[3]),
                     pfnUnpackAnglesFn(padfPrjParams[5]),
                     pfnUnpackAnglesFn(padfPrjParams[4]),
                     padfPrjParams[6], padfPrjParams[7] );
            break;

        case LAMCC:
            SetLCC( pfnUnpackAnglesFn(padfPrjParams[2]),
                    pfnUnpackAnglesFn(padfPrjParams[3]),
                    pfnUnpackAnglesFn(padfPrjParams[5]),
                    pfnUnpackAnglesFn(padfPrjParams[4]),
                    padfPrjParams[6], padfPrjParams[7] );
            break;

        case MERCAT:
            SetMercator( pfnUnpackAnglesFn(padfPrjParams[5]),
                         pfnUnpackAnglesFn(padfPrjParams[4]),
                         1.0,
                         padfPrjParams[6], padfPrjParams[7] );
            break;

        case PS:
            SetPS( pfnUnpackAnglesFn(padfPrjParams[5]),
                   pfnUnpackAnglesFn(padfPrjParams[4]),
                   1.0,
                   padfPrjParams[6], padfPrjParams[7] );

            break;

        case POLYC:
            SetPolyconic( pfnUnpackAnglesFn(padfPrjParams[5]),
                          pfnUnpackAnglesFn(padfPrjParams[4]),
                          padfPrjParams[6], padfPrjParams[7] );
            break;

        case EQUIDC:
            if( padfPrjParams[8] != 0.0 )
            {
                SetEC( pfnUnpackAnglesFn(padfPrjParams[2]),
                       pfnUnpackAnglesFn(padfPrjParams[3]),
                       pfnUnpackAnglesFn(padfPrjParams[5]),
                       pfnUnpackAnglesFn(padfPrjParams[4]),
                       padfPrjParams[6], padfPrjParams[7] );
            }
            else
            {
                SetEC( pfnUnpackAnglesFn(padfPrjParams[2]),
                       pfnUnpackAnglesFn(padfPrjParams[2]),
                       pfnUnpackAnglesFn(padfPrjParams[5]),
                       pfnUnpackAnglesFn(padfPrjParams[4]),
                       padfPrjParams[6], padfPrjParams[7] );
            }
            break;

        case TM:
            SetTM( pfnUnpackAnglesFn(padfPrjParams[5]),
                   pfnUnpackAnglesFn(padfPrjParams[4]),
                   padfPrjParams[2],
                   padfPrjParams[6], padfPrjParams[7] );
            break;

        case STEREO:
            SetStereographic( pfnUnpackAnglesFn(padfPrjParams[5]),
                              pfnUnpackAnglesFn(padfPrjParams[4]),
                              1.0,
                              padfPrjParams[6], padfPrjParams[7] );
            break;

        case LAMAZ:
            SetLAEA( pfnUnpackAnglesFn(padfPrjParams[5]),
                     pfnUnpackAnglesFn(padfPrjParams[4]),
                     padfPrjParams[6], padfPrjParams[7] );
            break;

        case AZMEQD:
            SetAE( pfnUnpackAnglesFn(padfPrjParams[5]),
                   pfnUnpackAnglesFn(padfPrjParams[4]),
                   padfPrjParams[6], padfPrjParams[7] );
            break;

        case GNOMON:
            SetGnomonic( pfnUnpackAnglesFn(padfPrjParams[5]),
                         pfnUnpackAnglesFn(padfPrjParams[4]),
                         padfPrjParams[6], padfPrjParams[7] );
            break;

        case ORTHO:
            SetOrthographic( pfnUnpackAnglesFn(padfPrjParams[5]),
                             pfnUnpackAnglesFn(padfPrjParams[4]),
                             padfPrjParams[6], padfPrjParams[7] );
            break;

        // FIXME: GVNSP --- General Vertical Near-Side Perspective skipped.

        case SNSOID:
            SetSinusoidal( pfnUnpackAnglesFn(padfPrjParams[4]),
                           padfPrjParams[6], padfPrjParams[7] );
            break;

        case EQRECT:
            SetEquirectangular2( 0.0,
                                 pfnUnpackAnglesFn(padfPrjParams[4]),
                                 pfnUnpackAnglesFn(padfPrjParams[5]),
                                 padfPrjParams[6], padfPrjParams[7] );
            break;

        case MILLER:
            SetMC( pfnUnpackAnglesFn(padfPrjParams[5]),
                   pfnUnpackAnglesFn(padfPrjParams[4]),
                   padfPrjParams[6], padfPrjParams[7] );
            break;

        case VGRINT:
            SetVDG( pfnUnpackAnglesFn(padfPrjParams[4]),
                    padfPrjParams[6], padfPrjParams[7] );
            break;

        case HOM:
            if( padfPrjParams[12] != 0.0 )
            {
                SetHOM( pfnUnpackAnglesFn(padfPrjParams[5]),
                        pfnUnpackAnglesFn(padfPrjParams[4]),
                        pfnUnpackAnglesFn(padfPrjParams[3]),
                        0.0, padfPrjParams[2],
                        padfPrjParams[6], padfPrjParams[7] );
            }
            else
            {
                SetHOM2PNO( pfnUnpackAnglesFn(padfPrjParams[5]),
                            pfnUnpackAnglesFn(padfPrjParams[9]),
                            pfnUnpackAnglesFn(padfPrjParams[8]),
                            pfnUnpackAnglesFn(padfPrjParams[11]),
                            pfnUnpackAnglesFn(padfPrjParams[10]),
                            padfPrjParams[2],
                            padfPrjParams[6], padfPrjParams[7] );
            }
            break;

        case ROBIN:
            SetRobinson( pfnUnpackAnglesFn(padfPrjParams[4]),
                         padfPrjParams[6], padfPrjParams[7] );
            break;

        // FIXME: SOM --- Space Oblique Mercator skipped.
        // FIXME: ALASKA --- Alaska Conformal skipped.
        // FIXME: GOODE --- Interrupted Goode skipped.

        case MOLL:
            SetMollweide( pfnUnpackAnglesFn(padfPrjParams[4]),
                          padfPrjParams[6], padfPrjParams[7] );
            break;

        // FIXME: IMOLL --- Interrupted Mollweide skipped.
        // FIXME: HAMMER --- Hammer skipped.

        case WAGIV:
            SetWagner( 4, 0.0, padfPrjParams[6], padfPrjParams[7] );
            break;

        case WAGVII:
            SetWagner( 7, 0.0, padfPrjParams[6], padfPrjParams[7] );
            break;

        // FIXME: OBEQA --- Oblated Equal Area skipped.
        // FIXME: ISINUS1 --- Integerized Sinusoidal Grid (the same as 99).
        // FIXME: CEA --- Cylindrical Equal Area skipped (Grid corners set in
        // meters for EASE grid).
        // FIXME: BCEA --- Cylindrical Equal Area skipped (Grid corners set in
        // DMS degs for EASE grid).
        // FIXME: ISINUS --- Integrized Sinusoidal skipped.

        default:
            CPLDebug( "OSR_USGS", "Unsupported projection: %ld", iProjSys );
            SetLocalCS( CPLString().Printf("GCTP projection number %ld",
                                           iProjSys) );
            break;
    }

/* -------------------------------------------------------------------- */
/*      Try to translate the datum/spheroid.                            */
/* -------------------------------------------------------------------- */

    if( !IsLocal() )
    {
        char *pszName = NULL;
        double dfSemiMajor = 0.0;
        double dfInvFlattening = 0.0;

        if( iDatum < 0 ) // Use specified ellipsoid parameters.
        {
            if( padfPrjParams[0] > 0.0 )
            {
                if( padfPrjParams[1] > 1.0 )
                {
                    dfInvFlattening = OSRCalcInvFlattening(padfPrjParams[0],
                                                           padfPrjParams[1]);
                }
                else if( padfPrjParams[1] > 0.0 )
                {
                    dfInvFlattening =
                        1.0 / ( 1.0 - sqrt(1.0 - padfPrjParams[1]) );
                }
                else
                {
                    dfInvFlattening = 0.0;
                }

                SetGeogCS( "Unknown datum based upon the custom spheroid",
                           "Not specified (based on custom spheroid)",
                           "Custom spheroid", padfPrjParams[0], dfInvFlattening,
                           NULL, 0, NULL, 0 );
            }
            else if( padfPrjParams[1] > 0.0 )  // Clarke 1866.
            {
                if( OSRGetEllipsoidInfo( 7008, &pszName, &dfSemiMajor,
                                          &dfInvFlattening ) == OGRERR_NONE )
                {
                    SetGeogCS( CPLString().Printf(
                                    "Unknown datum based upon the %s ellipsoid",
                                    pszName ),
                               CPLString().Printf(
                                    "Not specified (based on %s spheroid)",
                                    pszName ),
                               pszName, dfSemiMajor, dfInvFlattening,
                               NULL, 0.0, NULL, 0.0 );
                    SetAuthority( "SPHEROID", "EPSG", 7008 );
                }
            }
            else                              // Sphere, rad 6370997 m
            {
                if( OSRGetEllipsoidInfo( 7047, &pszName, &dfSemiMajor,
                                         &dfInvFlattening ) == OGRERR_NONE )
                {
                    SetGeogCS( CPLString().Printf(
                                    "Unknown datum based upon the %s ellipsoid",
                                    pszName ),
                               CPLString().Printf(
                                    "Not specified (based on %s spheroid)",
                                    pszName ),
                               pszName, dfSemiMajor, dfInvFlattening,
                               NULL, 0.0, NULL, 0.0 );
                    SetAuthority( "SPHEROID", "EPSG", 7047 );
                }
            }
        }
        else if( iDatum < NUMBER_OF_ELLIPSOIDS && aoEllips[iDatum] )
        {
            if( OSRGetEllipsoidInfo( aoEllips[iDatum], &pszName,
                                     &dfSemiMajor,
                                     &dfInvFlattening ) == OGRERR_NONE )
            {
                SetGeogCS( CPLString().Printf(
                               "Unknown datum based upon the %s ellipsoid",
                               pszName),
                           CPLString().Printf(
                               "Not specified (based on %s spheroid)",
                               pszName),
                           pszName, dfSemiMajor, dfInvFlattening,
                           NULL, 0.0, NULL, 0.0 );
                SetAuthority( "SPHEROID", "EPSG", aoEllips[iDatum] );
            }
            else
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Failed to lookup datum code %d, likely due to "
                          "missing GDAL gcs.csv file.  "
                          "Falling back to use WGS84.",
                          static_cast<int>(iDatum) );
                SetWellKnownGeogCS("WGS84");
            }
        }
        else
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Wrong datum code %d. Supported datums 0--%d only.  "
                      "Setting WGS84 as a fallback.",
                      static_cast<int>(iDatum), NUMBER_OF_ELLIPSOIDS );
            SetWellKnownGeogCS( "WGS84" );
        }

        CPLFree( pszName );
    }

/* -------------------------------------------------------------------- */
/*      Grid units translation                                          */
/* -------------------------------------------------------------------- */
    if( IsLocal() || IsProjected() )
        SetLinearUnits( SRS_UL_METER, 1.0 );

    FixupOrdering();

    return OGRERR_NONE;
}

/************************************************************************/
/*                          OSRExportToUSGS()                           */
/************************************************************************/
/**
 * \brief Export coordinate system in USGS GCTP projection definition.
 *
 * This function is the same as OGRSpatialReference::exportToUSGS().
 */

OGRErr OSRExportToUSGS( OGRSpatialReferenceH hSRS,
                        long *piProjSys, long *piZone,
                        double **ppadfPrjParams, long *piDatum )

{
    VALIDATE_POINTER1( hSRS, "OSRExportToUSGS", OGRERR_FAILURE );

    *ppadfPrjParams = NULL;

    return ((OGRSpatialReference *) hSRS)->exportToUSGS( piProjSys, piZone,
                                                         ppadfPrjParams,
                                                         piDatum );
}

/************************************************************************/
/*                           exportToUSGS()                             */
/************************************************************************/

/**
 * \brief Export coordinate system in USGS GCTP projection definition.
 *
 * This method is the equivalent of the C function OSRExportToUSGS().
 *
 * @param piProjSys Pointer to variable, where the projection system code will
 * be returned.
 *
 * @param piZone Pointer to variable, where the zone for UTM and State Plane
 * projection systems will be returned.
 *
 * @param ppadfPrjParams Pointer to which dynamically allocated array of
 * 15 projection parameters will be assigned. See importFromUSGS() for
 * the list of parameters. Caller responsible to free this array.
 *
 * @param piDatum Pointer to variable, where the datum code will
 * be returned.
 *
 * @return OGRERR_NONE on success or an error code on failure.
 */

OGRErr OGRSpatialReference::exportToUSGS( long *piProjSys, long *piZone,
                                          double **ppadfPrjParams,
                                          long *piDatum ) const

{
    const char *pszProjection = GetAttrValue("PROJECTION");

/* -------------------------------------------------------------------- */
/*      Fill all projection parameters with zero.                       */
/* -------------------------------------------------------------------- */
    *ppadfPrjParams = static_cast<double *>(CPLMalloc(15 * sizeof(double)));
    for( int i = 0; i < 15; i++ )
        (*ppadfPrjParams)[i] = 0.0;

    *piZone = 0L;

/* ==================================================================== */
/*      Handle the projection definition.                               */
/* ==================================================================== */
    if( IsLocal() )
        *piProjSys = GEO;

    else if( pszProjection == NULL )
    {
#ifdef DEBUG
        CPLDebug( "OSR_USGS",
                  "Empty projection definition, considered as Geographic" );
#endif
        *piProjSys = GEO;
    }

    else if( EQUAL(pszProjection, SRS_PT_ALBERS_CONIC_EQUAL_AREA) )
    {
        *piProjSys = ALBERS;
        (*ppadfPrjParams)[2] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, 0.0 ) );
        (*ppadfPrjParams)[3] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_STANDARD_PARALLEL_2, 0.0 ) );
        (*ppadfPrjParams)[4] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        (*ppadfPrjParams)[5] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP) )
    {
        *piProjSys = LAMCC;
        (*ppadfPrjParams)[2] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, 0.0 ) );
        (*ppadfPrjParams)[3] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_STANDARD_PARALLEL_2, 0.0 ) );
        (*ppadfPrjParams)[4] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        (*ppadfPrjParams)[5] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_MERCATOR_1SP) )
    {
        *piProjSys = MERCAT;
        (*ppadfPrjParams)[4] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        (*ppadfPrjParams)[5] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_POLAR_STEREOGRAPHIC) )
    {
        *piProjSys = PS;
        (*ppadfPrjParams)[4] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        (*ppadfPrjParams)[5] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_POLYCONIC) )
    {
        *piProjSys = POLYC;
        (*ppadfPrjParams)[4] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        (*ppadfPrjParams)[5] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_EQUIDISTANT_CONIC) )
    {
        *piProjSys = EQUIDC;
        (*ppadfPrjParams)[2] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, 0.0 ) );
        (*ppadfPrjParams)[3] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_STANDARD_PARALLEL_2, 0.0 ) );
        (*ppadfPrjParams)[4] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        (*ppadfPrjParams)[5] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
        (*ppadfPrjParams)[8] = 1.0;
    }

    else if( EQUAL(pszProjection, SRS_PT_TRANSVERSE_MERCATOR) )
    {
        int bNorth;

        *piZone = GetUTMZone( &bNorth );

        if( *piZone != 0 )
        {
            *piProjSys = UTM;
            if( !bNorth )
                *piZone = - *piZone;
        }
        else
        {
            *piProjSys = TM;
            (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_SCALE_FACTOR, 1.0 );
            (*ppadfPrjParams)[4] = CPLDecToPackedDMS(
                GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
            (*ppadfPrjParams)[5] = CPLDecToPackedDMS(
                GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );
            (*ppadfPrjParams)[6] =
                GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
            (*ppadfPrjParams)[7] =
                GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
        }
    }

    else if( EQUAL(pszProjection, SRS_PT_STEREOGRAPHIC) )
    {
        *piProjSys = STEREO;
        (*ppadfPrjParams)[4] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        (*ppadfPrjParams)[5] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA) )
    {
        *piProjSys = LAMAZ;
        (*ppadfPrjParams)[4] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        (*ppadfPrjParams)[5] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_AZIMUTHAL_EQUIDISTANT) )
    {
        *piProjSys = AZMEQD;
        (*ppadfPrjParams)[4] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0 ) );
        (*ppadfPrjParams)[5] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_LATITUDE_OF_CENTER, 0.0 ) );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_GNOMONIC) )
    {
        *piProjSys = GNOMON;
        (*ppadfPrjParams)[4] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        (*ppadfPrjParams)[5] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_ORTHOGRAPHIC) )
    {
        *piProjSys = ORTHO;
        (*ppadfPrjParams)[4] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        (*ppadfPrjParams)[5] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_SINUSOIDAL) )
    {
        *piProjSys = SNSOID;
        (*ppadfPrjParams)[4] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0 ) );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_EQUIRECTANGULAR) )
    {
        *piProjSys = EQRECT;
        (*ppadfPrjParams)[4] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        (*ppadfPrjParams)[5] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, 0.0 ) );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_MILLER_CYLINDRICAL) )
    {
        *piProjSys = MILLER;
        (*ppadfPrjParams)[4] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0 ) );
        (*ppadfPrjParams)[5] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_LATITUDE_OF_CENTER, 0.0 ) );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_VANDERGRINTEN) )
    {
        *piProjSys = VGRINT;
        (*ppadfPrjParams)[4] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0 ) );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_HOTINE_OBLIQUE_MERCATOR) )
    {
        *piProjSys = HOM;
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_SCALE_FACTOR, 1.0 );
        (*ppadfPrjParams)[3] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_AZIMUTH, 0.0 ) );
        (*ppadfPrjParams)[4] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0 ) );
        (*ppadfPrjParams)[5] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_LATITUDE_OF_CENTER, 0.0 ) );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
        (*ppadfPrjParams)[12] = 1.0;
    }

    else if( EQUAL(pszProjection,
                   SRS_PT_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN) )
    {
        *piProjSys = HOM;
        (*ppadfPrjParams)[2] = GetNormProjParm( SRS_PP_SCALE_FACTOR, 1.0 );
        (*ppadfPrjParams)[5] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_LATITUDE_OF_CENTER, 0.0 ) );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
        (*ppadfPrjParams)[8] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_LONGITUDE_OF_POINT_1, 0.0 ) );
        (*ppadfPrjParams)[9] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_LATITUDE_OF_POINT_1, 0.0 ) );
        (*ppadfPrjParams)[10] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_LONGITUDE_OF_POINT_2, 0.0 ) );
        (*ppadfPrjParams)[11] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_LATITUDE_OF_POINT_2, 0.0 ) );
        (*ppadfPrjParams)[12] = 0.0;
    }

    else if( EQUAL(pszProjection, SRS_PT_ROBINSON) )
    {
        *piProjSys = ROBIN;
        (*ppadfPrjParams)[4] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0 ) );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_MOLLWEIDE) )
    {
        *piProjSys = MOLL;
        (*ppadfPrjParams)[4] = CPLDecToPackedDMS(
            GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_WAGNER_IV) )
    {
        *piProjSys = WAGIV;
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }

    else if( EQUAL(pszProjection, SRS_PT_WAGNER_VII) )
    {
        *piProjSys = WAGVII;
        (*ppadfPrjParams)[6] = GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
        (*ppadfPrjParams)[7] = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
    }
    // Projection unsupported by GCTP.
    else
    {
        CPLDebug( "OSR_USGS",
                  "Projection \"%s\" unsupported by USGS GCTP. "
                  "Geographic system will be used.", pszProjection );
        *piProjSys = GEO;
    }

/* -------------------------------------------------------------------- */
/*      Translate the datum.                                            */
/* -------------------------------------------------------------------- */
    const char *pszDatum = GetAttrValue( "DATUM" );

    if( pszDatum )
    {
        if( EQUAL( pszDatum, SRS_DN_NAD27 ) )
        {
            *piDatum = CLARKE1866;
        }
        else if( EQUAL( pszDatum, SRS_DN_NAD83 ) )
        {
            *piDatum = GRS1980;
        }
        else if( EQUAL( pszDatum, SRS_DN_WGS84 ) )
        {
            *piDatum = WGS84;
        }
        // If not found well known datum, translate ellipsoid.
        else
        {
            const double dfSemiMajor = GetSemiMajor();
            const double dfInvFlattening = GetInvFlattening();

#ifdef DEBUG
            CPLDebug( "OSR_USGS",
                      "Datum \"%s\" unsupported by USGS GCTP. "
                      "Try to translate ellipsoid definition.", pszDatum );
#endif

            int i = 0;  // Used after for.
            for( ; i < NUMBER_OF_ELLIPSOIDS; i++ )
            {
                double dfSM = 0.0;
                double dfIF = 0.0;

                if( OSRGetEllipsoidInfo( aoEllips[i], NULL,
                                         &dfSM, &dfIF ) == OGRERR_NONE
                    && CPLIsEqual( dfSemiMajor, dfSM )
                    && CPLIsEqual( dfInvFlattening, dfIF ) )
                {
                    *piDatum = i;
                    break;
                }
            }

            if( i == NUMBER_OF_ELLIPSOIDS )  // Didn't found matches; set
            {                                // custom ellipsoid parameters.
#ifdef DEBUG
                CPLDebug( "OSR_USGS",
                          "Ellipsoid \"%s\" unsupported by USGS GCTP. "
                          "Custom ellipsoid definition will be used.",
                          pszDatum );
#endif
                *piDatum = -1;
                (*ppadfPrjParams)[0] = dfSemiMajor;
                if( std::abs( dfInvFlattening ) < 0.000000000001 )
                {
                    (*ppadfPrjParams)[1] = dfSemiMajor;
                }
                else
                {
                    (*ppadfPrjParams)[1] =
                        dfSemiMajor * (1.0 - 1.0/dfInvFlattening);
                }
            }
        }
    }
    else
    {
        *piDatum = -1;
    }

    return OGRERR_NONE;
}

/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGRSpatialReference translation to/from USGS georeferencing
 *           information (used in GCTP package).
 * Author:   Andrey Kiselev, dron@remotesensing.org
 *
 ******************************************************************************
 * Copyright (c) 2004, Andrey Kiselev <dron@remotesensing.org>
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
 ****************************************************************************/

#include "ogr_spatialref.h"
#include "cpl_conv.h"
#include "cpl_csv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*  GCTP projection codes.                                              */
/************************************************************************/

#define GEO     0L      // Geographic
#define UTM     1L      // Universal Transverse Mercator (UTM)
#define SPCS    2L      // State Plane Coordinates
#define ALBERS  3L      // Albers Conical Equal Area
#define LAMCC   4L      // Lambert Conformal Conic
#define MERCAT  5L      // Mercator
#define PS      6L      // Polar Stereographic
#define POLYC   7L      // Polyconic
#define EQUIDC  8L      // Equidistant Conic
#define TM      9L      // Transverse Mercator
#define STEREO  10L     // Stereographic
#define LAMAZ   11L     // Lambert Azimuthal Equal Area
#define AZMEQD  12L     // Azimuthal Equidistant
#define GNOMON  13L     // Gnomonic
#define ORTHO   14L     // Orthographic
#define GVNSP   15L     // General Vertical Near-Side Perspective
#define SNSOID  16L     // Sinusiodal
#define EQRECT  17L     // Equirectangular
#define MILLER  18L     // Miller Cylindrical
#define VGRINT  19L     // Van der Grinten
#define HOM     20L     // (Hotine) Oblique Mercator 
#define ROBIN   21L     // Robinson
#define SOM     22L     // Space Oblique Mercator (SOM)
#define ALASKA  23L     // Alaska Conformal
#define GOODE   24L     // Interrupted Goode Homolosine 
#define MOLL    25L     // Mollweide
#define IMOLL   26L     // Interrupted Mollweide
#define HAMMER  27L     // Hammer
#define WAGIV   28L     // Wagner IV
#define WAGVII  29L     // Wagner VII
#define OBEQA   30L     // Oblated Equal Area
#define ISINUS1 31L     // Integerized Sinusoidal Grid (the same as 99)
#define CEA     97L     // Cylindrical Equal Area (Grid corners set
                        // in meters for EASE grid) 
#define BCEA    98L     // Cylindrical Equal Area (Grid corners set
                        // in DMS degs for EASE grid) 
#define ISINUS  99L     // Integerized Sinusoidal Grid
                        // (added by Raj Gejjagaraguppe ARC for MODIS) 

/************************************************************************/
/*  Correspondence between GCTP and EPSG ellipsoid codes.               */
/************************************************************************/

static long aoEllips[] =
{
    7008,   // Clarke, 1866 (NAD1927)
    7034,   // Clarke, 1880
    7004,   // Bessel, 1841
    0,// FIXME: New International, 1967 --- skipped
    7022,   // International, 1924 (Hayford, 1909) XXX?
    7043,   // WGS, 1972
    7042,   // Everest, 1830
    7025,   // FIXME: WGS, 1966
    7019,   // GRS, 1980 (NAD1983)
    7001,   // Airy, 1830
    7018,   // Modified Everest
    7002,   // Modified Airy
    7030,   // WGS, 1984 (GPS)
    0,// FIXME: Southeast Asia --- skipped
    7003,   // Australian National, 1965
    7024,   // Krassovsky, 1940
    0,// FIXME: Hough --- skipped
    0,// FIXME: Mercury, 1960 --- skipped
    0,// FIXME: Modified Mercury, 1968 --- skipped
    7047,   // Sphere, rad 6370997 m (normal sphere)
    7006,   // Bessel, 1841 (Namibia)
    7016,   // Everest (Sabah & Sarawak)
    7044,   // Everest, 1956
    0,// FIXME: Everest, Malaysia 1969 --- skipped
    7018,   // Everest, Malay & Singapr 1948
    0,// FIXME: Everest, Pakistan --- skipped
    7022,   // Hayford (International 1924) XXX?
    7020,   // Helmert 1906
    7021,   // Indonesian, 1974
    7036,   // South American, 1969
    0// FIXME: WGS 60 --- skipped
};

#define NUMBER_OF_ELLIPSOIDS    (sizeof(aoEllips)/sizeof(aoEllips[0]))

/************************************************************************/
/*                        USGSGetUOMLengthInfo()                        */
/*                                                                      */
/*      Note: This function should eventually also know how to          */
/*      lookup length aliases in the UOM_LE_ALIAS table.                */
/************************************************************************/

static int 
USGSGetUOMLengthInfo( int nUOMLengthCode, char **ppszUOMName,
                      double * pdfInMeters )

{
    char        **papszUnitsRecord;
    char        szSearchKey[24];
    int         iNameField;

#define UOM_FILENAME CSVFilename( "unit_of_measure.csv" )

/* -------------------------------------------------------------------- */
/*      We short cut meter to save work in the most common case.        */
/* -------------------------------------------------------------------- */
    if( nUOMLengthCode == 9001 )
    {
        if( ppszUOMName != NULL )
            *ppszUOMName = CPLStrdup( "metre" );
        if( pdfInMeters != NULL )
            *pdfInMeters = 1.0;

        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Search the units database for this unit.  If we don't find      */
/*      it return failure.                                              */
/* -------------------------------------------------------------------- */
    sprintf( szSearchKey, "%d", nUOMLengthCode );
    papszUnitsRecord =
        CSVScanFileByName( UOM_FILENAME, "UOM_CODE", szSearchKey, CC_Integer );

    if( papszUnitsRecord == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Get the name, if requested.                                     */
/* -------------------------------------------------------------------- */
    if( ppszUOMName != NULL )
    {
        iNameField = CSVGetFileFieldId( UOM_FILENAME, "UNIT_OF_MEAS_NAME" );
        *ppszUOMName = CPLStrdup( CSLGetField(papszUnitsRecord, iNameField) );
    }
    
/* -------------------------------------------------------------------- */
/*      Get the A and B factor fields, and create the multiplicative    */
/*      factor.                                                         */
/* -------------------------------------------------------------------- */
    if( pdfInMeters != NULL )
    {
        int     iBFactorField, iCFactorField;
        
        iBFactorField = CSVGetFileFieldId( UOM_FILENAME, "FACTOR_B" );
        iCFactorField = CSVGetFileFieldId( UOM_FILENAME, "FACTOR_C" );

        if( atof(CSLGetField(papszUnitsRecord, iCFactorField)) > 0.0 )
            *pdfInMeters = atof(CSLGetField(papszUnitsRecord, iBFactorField))
                / atof(CSLGetField(papszUnitsRecord, iCFactorField));
        else
            *pdfInMeters = 0.0;
    }
    
    return( TRUE );
}

/************************************************************************/
/*                        USGSGetEllipsoidInfo()                        */
/*                                                                      */
/*      Fetch info about an ellipsoid.  Axes are always returned in     */
/*      meters.  SemiMajor computed based on inverse flattening         */
/*      where that is provided.                                         */
/************************************************************************/

static int 
USGSGetEllipsoidInfo( int nCode, char ** ppszName,
                      double * pdfSemiMajor, double * pdfInvFlattening )

{
    char        szSearchKey[24];
    double      dfSemiMajor, dfToMeters = 1.0;
    int         nUOMLength;
    
/* -------------------------------------------------------------------- */
/*      Get the semi major axis.                                        */
/* -------------------------------------------------------------------- */
    sprintf( szSearchKey, "%d", nCode );

    dfSemiMajor =
        atof(CSVGetField( CSVFilename("ellipsoid.csv" ),
                          "ELLIPSOID_CODE", szSearchKey, CC_Integer,
                          "SEMI_MAJOR_AXIS" ) );
    if( dfSemiMajor == 0.0 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Get the translation factor into meters.                         */
/* -------------------------------------------------------------------- */
    nUOMLength = atoi(CSVGetField( CSVFilename("ellipsoid.csv" ),
                                   "ELLIPSOID_CODE", szSearchKey, CC_Integer,
                                   "UOM_CODE" ));
    USGSGetUOMLengthInfo( nUOMLength, NULL, &dfToMeters );

    dfSemiMajor *= dfToMeters;
    
    if( pdfSemiMajor != NULL )
        *pdfSemiMajor = dfSemiMajor;
    
/* -------------------------------------------------------------------- */
/*      Get the semi-minor if requested.  If the Semi-minor axis        */
/*      isn't available, compute it based on the inverse flattening.    */
/* -------------------------------------------------------------------- */
    if( pdfInvFlattening != NULL )
    {
        *pdfInvFlattening = 
            atof(CSVGetField( CSVFilename("ellipsoid.csv" ),
                              "ELLIPSOID_CODE", szSearchKey, CC_Integer,
                              "INV_FLATTENING" ));

        if( *pdfInvFlattening == 0.0 )
        {
            double dfSemiMinor;

            dfSemiMinor =
                atof(CSVGetField( CSVFilename("ellipsoid.csv" ),
                                  "ELLIPSOID_CODE", szSearchKey, CC_Integer,
                                  "SEMI_MINOR_AXIS" )) * dfToMeters;

            if( dfSemiMajor != 0.0 && dfSemiMajor != dfSemiMinor )
                *pdfInvFlattening = 
                    -1.0 / (dfSemiMinor/dfSemiMajor - 1.0);
            else
                *pdfInvFlattening = 0.0;
        }
    }

/* -------------------------------------------------------------------- */
/*      Get the name, if requested.                                     */
/* -------------------------------------------------------------------- */
    if( ppszName != NULL )
        *ppszName =
            CPLStrdup(CSVGetField( CSVFilename("ellipsoid.csv" ),
                                   "ELLIPSOID_CODE", szSearchKey, CC_Integer,
                                   "ELLIPSOID_NAME" ));
    
    return( TRUE );
}

/************************************************************************/
/*                         OSRImportFromUSGS()                          */
/************************************************************************/

OGRErr OSRImportFromUSGS( OGRSpatialReferenceH hSRS, long iProjsys,
                          long iZone, double *padfPrjParams, long iDatum )

{
    return ((OGRSpatialReference *) hSRS)->importFromUSGS( iProjsys, iZone,
                                                           padfPrjParams,
                                                           iDatum );
}

/************************************************************************/
/*                          importFromUSGS()                            */
/************************************************************************/

/**
 * Import coordinate system from USGS projection definition.
 *
 * This method will import projection definition in style, used by USGS GCTP
 * software. GCTP operates on angles in packed DMS format (see
 * CPLDecToPackedDMS() function for details), so all angle values (latitudes,
 * longitudes, azimuths, etc.) specified in the padfPrjParams array should
 * be in the packed DMS format.
 *
 * This function is the equivalent of the C function OSRImportFromUSGS().
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
 * @param iDatum Output spheroid.<p>
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
 * @return OGRERR_NONE on success or an error code in case of failure. 
 */

OGRErr OGRSpatialReference::importFromUSGS( long iProjSys, long iZone,
                                            double *padfPrjParams,
                                            long iDatum )

{
    if( !padfPrjParams )
        return OGRERR_CORRUPT_DATA;

/* -------------------------------------------------------------------- */
/*      Operate on the basis of the projection code.                    */
/* -------------------------------------------------------------------- */
    switch ( iProjSys )
    {
        case GEO:
            break;

        case UTM:
            {
                int bNorth = TRUE;

                if ( !iZone )
                {
                    if ( padfPrjParams[2] != 0.0 )
                        iZone = (long) padfPrjParams[2];
                    else if (padfPrjParams[0] != 0.0 && padfPrjParams[1] != 0.0)
                    {
                        iZone = (long)(((CPLPackedDMSToDec(padfPrjParams[0])
                                         + 180.0) / 6.0) + 1.0);
                        if ( CPLPackedDMSToDec(padfPrjParams[0]) < 0 )
                            bNorth = FALSE;
                    }
                }

                if ( iZone < 0 )
                {
                    iZone = -iZone;
                    bNorth = FALSE;
                }
                SetUTM( iZone, bNorth );
            }
            break;

        case SPCS:
            {
                int bNAD83 = TRUE;

                if ( iDatum == 0 )
                    bNAD83 = FALSE;
                else if ( iDatum != 8 )
                    CPLError( CE_Warning, CPLE_AppDefined,
                              "Wrong datum for State Plane projection %d. "
                              "Should be 0 or 8.", iDatum );
                
                SetStatePlane( iZone, bNAD83 );
            }
            break;

        case ALBERS:
            SetACEA( CPLPackedDMSToDec(padfPrjParams[2]),
                     CPLPackedDMSToDec(padfPrjParams[3]),
                     CPLPackedDMSToDec(padfPrjParams[5]),
                     CPLPackedDMSToDec(padfPrjParams[4]),
                     padfPrjParams[6], padfPrjParams[7] );
            break;

        case LAMCC:
            SetLCC( CPLPackedDMSToDec(padfPrjParams[2]),
                    CPLPackedDMSToDec(padfPrjParams[3]),
                    CPLPackedDMSToDec(padfPrjParams[5]),
                    CPLPackedDMSToDec(padfPrjParams[4]),
                    padfPrjParams[6], padfPrjParams[7] );
            break;

        case MERCAT:
            SetMercator( CPLPackedDMSToDec(padfPrjParams[5]),
                         CPLPackedDMSToDec(padfPrjParams[4]),
                         1.0,
                         padfPrjParams[6], padfPrjParams[7] );
            break;

        case PS:
            SetPS( CPLPackedDMSToDec(padfPrjParams[5]),
                   CPLPackedDMSToDec(padfPrjParams[4]),
                   1.0,
                   padfPrjParams[6], padfPrjParams[7] );

            break;

        case POLYC:
            SetPolyconic( CPLPackedDMSToDec(padfPrjParams[5]),
                          CPLPackedDMSToDec(padfPrjParams[4]),
                          padfPrjParams[6], padfPrjParams[7] );
            break;

        case EQUIDC:
            if ( padfPrjParams[8] )
            {
                SetEC( CPLPackedDMSToDec(padfPrjParams[2]),
                       CPLPackedDMSToDec(padfPrjParams[3]),
                       CPLPackedDMSToDec(padfPrjParams[5]),
                       CPLPackedDMSToDec(padfPrjParams[4]),
                       padfPrjParams[6], padfPrjParams[7] );
            }
            else
            {
                SetEC( CPLPackedDMSToDec(padfPrjParams[2]),
                       CPLPackedDMSToDec(padfPrjParams[2]),
                       CPLPackedDMSToDec(padfPrjParams[5]),
                       CPLPackedDMSToDec(padfPrjParams[4]),
                       padfPrjParams[6], padfPrjParams[7] );
            }
            break;

        case TM:
            SetTM( CPLPackedDMSToDec(padfPrjParams[5]),
                   CPLPackedDMSToDec(padfPrjParams[4]),
                   padfPrjParams[2],
                   padfPrjParams[6], padfPrjParams[7] );
            break;

        case STEREO:
            SetStereographic( CPLPackedDMSToDec(padfPrjParams[5]),
                              CPLPackedDMSToDec(padfPrjParams[4]),
                              1.0,
                              padfPrjParams[6], padfPrjParams[7] );
            break;

        case LAMAZ:
            SetLAEA( CPLPackedDMSToDec(padfPrjParams[5]),
                     CPLPackedDMSToDec(padfPrjParams[4]),
                     padfPrjParams[6], padfPrjParams[7] );
            break;

        case AZMEQD:
            SetAE( CPLPackedDMSToDec(padfPrjParams[5]),
                   CPLPackedDMSToDec(padfPrjParams[4]),
                   padfPrjParams[6], padfPrjParams[7] );
            break;

        case GNOMON:
            SetGnomonic( CPLPackedDMSToDec(padfPrjParams[5]),
                         CPLPackedDMSToDec(padfPrjParams[4]),
                         padfPrjParams[6], padfPrjParams[7] );
            break;

        case ORTHO:
            SetOrthographic( CPLPackedDMSToDec(padfPrjParams[5]),
                             CPLPackedDMSToDec(padfPrjParams[4]),
                             padfPrjParams[6], padfPrjParams[7] );
            break;

        // FIXME: GVNSP --- General Vertical Near-Side Perspective skipped

        case SNSOID:
            SetSinusoidal( CPLPackedDMSToDec(padfPrjParams[4]),
                           padfPrjParams[6], padfPrjParams[7] );
            break;

        case EQRECT:
            SetEquirectangular( CPLPackedDMSToDec(padfPrjParams[5]),
                                CPLPackedDMSToDec(padfPrjParams[4]),
                                padfPrjParams[6], padfPrjParams[7] );
            break;

        case MILLER:
            SetMC( CPLPackedDMSToDec(padfPrjParams[5]),
                   CPLPackedDMSToDec(padfPrjParams[4]),
                   padfPrjParams[6], padfPrjParams[7] );
            break;

        case VGRINT:
            SetVDG( CPLPackedDMSToDec(padfPrjParams[4]),
                    padfPrjParams[6], padfPrjParams[7] );
            break;

        case HOM:
            if ( padfPrjParams[12] )
            {
                SetHOM( CPLPackedDMSToDec(padfPrjParams[5]),
                        CPLPackedDMSToDec(padfPrjParams[4]),
                        CPLPackedDMSToDec(padfPrjParams[3]),
                        0.0, padfPrjParams[2],
                        padfPrjParams[6],  padfPrjParams[7] );
            }
            else
            {
                SetHOM2PNO( CPLPackedDMSToDec(padfPrjParams[5]),
                            CPLPackedDMSToDec(padfPrjParams[9]),
                            CPLPackedDMSToDec(padfPrjParams[8]),
                            CPLPackedDMSToDec(padfPrjParams[11]),
                            CPLPackedDMSToDec(padfPrjParams[10]),
                            padfPrjParams[2],
                            padfPrjParams[6],  padfPrjParams[7] );
            }
            break;

        case ROBIN:
            SetRobinson( CPLPackedDMSToDec(padfPrjParams[4]),
                         padfPrjParams[6], padfPrjParams[7] );
            break;

        // FIXME: SOM --- Space Oblique Mercator skipped

        // FIXME: ALASKA --- Alaska Conformal skipped

        // FIXME: GOODE --- Interrupted Goode skipped

        case MOLL:
            SetMollweide( CPLPackedDMSToDec(padfPrjParams[4]),
                          padfPrjParams[6], padfPrjParams[7] );
            break;

        // FIXME: IMOLL --- Interrupted Mollweide skipped

        // FIXME: HAMMER --- Hammer skipped

        // FIXME: WAGIV --- Wagner IV skipped

        // FIXME: WAGVII --- Wagner VII skipped

        // FIXME: OBEQA --- Oblated Equal Area skipped

        // FIXME: ISINUS1 --- Integerized Sinusoidal Grid (the same as 99) skipped
        
        // FIXME: CEA --- Cylindrical Equal Area skipped (Grid corners set in meters for EASE grid)

        // FIXME: BCEA --- Cylindrical Equal Area skipped (Grid corners set in DMS degs for EASE grid)

        // FIXME: ISINUS --- Integrized Sinusoidal skipped

        default:
            CPLDebug( "OSR_USGS", "Unsupported projection: %d", iProjSys );
            SetLocalCS( CPLSPrintf("GCTP projection number %d", iProjSys) );
            break;
            
    }

/* -------------------------------------------------------------------- */
/*      Try to translate the datum/spheroid.                            */
/* -------------------------------------------------------------------- */

    if ( !IsLocal() )
    {
        char    *pszName = NULL;
        double  dfSemiMajor, dfInvFlattening;

        if ( iDatum < 0  ) // Use specified ellipsoid parameters
        {
            if ( padfPrjParams[0] > 0.0 )
            {
                if ( padfPrjParams[1] > 1.0 )
                {
                    if( ABS(padfPrjParams[0] - padfPrjParams[1]) < 0.01 )
                        dfInvFlattening = 0.0;
                    else
                    {
                        dfInvFlattening = padfPrjParams[0]
                            / ( padfPrjParams[0] - padfPrjParams[1] );
                    }
                }
                else if ( padfPrjParams[1] > 0.0 )
                {
                    dfInvFlattening =
                        1.0 / ( 1.0 - sqrt(1.0 - padfPrjParams[1]) );
                }
                else
                    dfInvFlattening = 0.0;

                SetGeogCS( "Unknown datum based upon the custom spheroid",
                           "Not specified (based on custom spheroid)",
                           "Custom spheroid", padfPrjParams[0], dfInvFlattening,
                           NULL, 0, NULL, 0 );
            }
            else if ( padfPrjParams[1] > 0.0 )  // Clarke 1866
            {
                USGSGetEllipsoidInfo( 7008, &pszName, &dfSemiMajor,
                                      &dfInvFlattening );
                SetGeogCS( CPLSPrintf(
                               "Unknown datum based upon the %s ellipsoid",
                               pszName ),
                           CPLSPrintf( "Not specified (based on %s spheroid)",
                                       pszName ),
                           pszName, dfSemiMajor, dfInvFlattening,
                           NULL, 0.0, NULL, 0.0 );
                SetAuthority( "SPHEROID", "EPSG", 7008 );
            }
            else                              // Sphere, rad 6370997 m
            {
                USGSGetEllipsoidInfo( 7047, &pszName, &dfSemiMajor,
                                      &dfInvFlattening );
                SetGeogCS( CPLSPrintf(
                               "Unknown datum based upon the %s ellipsoid",
                               pszName ),
                           CPLSPrintf( "Not specified (based on %s spheroid)",
                                       pszName ),
                           pszName, dfSemiMajor, dfInvFlattening,
                           NULL, 0.0, NULL, 0.0 );
                SetAuthority( "SPHEROID", "EPSG", 7047 );
            }

        }
        else if ( iDatum < (int) NUMBER_OF_ELLIPSOIDS && aoEllips[iDatum] )
        {
            if( USGSGetEllipsoidInfo( aoEllips[iDatum], &pszName,
                                       &dfSemiMajor, &dfInvFlattening ) )
            {
                SetGeogCS( CPLSPrintf("Unknown datum based upon the %s ellipsoid",
                                      pszName ),
                           CPLSPrintf( "Not specified (based on %s spheroid)",
                                       pszName ),
                           pszName, dfSemiMajor, dfInvFlattening,
                           NULL, 0.0, NULL, 0.0 );
                SetAuthority( "SPHEROID", "EPSG", aoEllips[iDatum] );
            }
            else
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Failed to lookup datum code %d, likely due to missing GDAL gcs.csv\n"
                          " file.  Falling back to use WGS84.", 
                          iDatum );
                SetWellKnownGeogCS("WGS84" );
            }
        }
        else
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Wrong datum code %d. Supported datums 0--%d only.\n"
                      "Setting WGS84 as a fallback.",
                      iDatum, NUMBER_OF_ELLIPSOIDS );
            SetWellKnownGeogCS( "WGS84" );
        }

        if ( pszName )
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

OGRErr OSRExportToUSGS( OGRSpatialReferenceH hSRS,
                        long *piProjSys, long *piZone,
                        double **ppadfPrjParams, long *piDatum )

{
    *ppadfPrjParams = NULL;

    return ((OGRSpatialReference *) hSRS)->exportToUSGS( piProjSys, piZone,
                                                         ppadfPrjParams,
                                                         piDatum );
}

/************************************************************************/
/*                           exportToUSGS()                             */
/************************************************************************/

/**
 * Export coordinate system in USGS GCTP projection definition.
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
 * @return OGRERR_NONE on success or an error code on failure. 
 */

OGRErr OGRSpatialReference::exportToUSGS( long *piProjSys, long *piZone,
                                          double **ppadfPrjParams,
                                          long *piDatum ) const

{
    const char  *pszProjection = GetAttrValue("PROJECTION");

/* -------------------------------------------------------------------- */
/*      Fill all projection parameters with zero.                       */
/* -------------------------------------------------------------------- */
    int         i;

    *ppadfPrjParams = (double *)CPLMalloc( 15 * sizeof(double) );
    for ( i = 0; i < 15; i++ )
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
            GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );
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

    // Projection unsupported by GCTP
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
    const char  *pszDatum = GetAttrValue( "DATUM" );

    if( EQUAL( pszDatum, SRS_DN_NAD27 ) )
        *piDatum = 0L;

    else if( EQUAL( pszDatum, SRS_DN_NAD83 ) )
        *piDatum = 8L;

    else if( EQUAL( pszDatum, SRS_DN_WGS84 ) )
        *piDatum = 12L;

    // If not found well known datum, translate ellipsoid
    else
    {
        double      dfSemiMajor = GetSemiMajor();
        double      dfInvFlattening = GetInvFlattening();

#ifdef DEBUG
        CPLDebug( "OSR_USGS",
                  "Datum \"%s\" unsupported by USGS GCTP. "
                  "Try to translate ellipsoid definition.", pszDatum );
#endif
        
        for ( i = 0; i < (int) NUMBER_OF_ELLIPSOIDS; i++ )
        {
            double  dfSM;
            double  dfIF;

            USGSGetEllipsoidInfo( aoEllips[i], NULL, &dfSM, &dfIF );
            if( ABS( dfSemiMajor - dfSM ) < 0.01
                && ABS( dfInvFlattening - dfIF ) < 0.0001 )
            {
                *piDatum = i;
                break;
            }
        }

        if ( i == NUMBER_OF_ELLIPSOIDS )    // Didn't found matches; set
        {                                   // custom ellipsoid parameters
#ifdef DEBUG
            CPLDebug( "OSR_USGS",
                      "Ellipsoid \"%s\" unsupported by USGS GCTP. "
                      "Custom ellipsoid definition will be used.", pszDatum );
#endif
            *piDatum = -1;
            (*ppadfPrjParams)[0] = dfSemiMajor;
            if ( ABS( dfInvFlattening ) < 0.000000000001 )
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

    return OGRERR_NONE;
}


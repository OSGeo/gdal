#ifndef GEOCENT_H
#define GEOCENT_H

/***************************************************************************/
/* RSC IDENTIFIER:  GEOCENTRIC
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates (latitude,
 *    longitude in radians and height in meters) and Geocentric coordinates
 *    (X, Y, Z) in meters.
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using 
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *      GEOCENT_NO_ERROR        : No errors occurred in function
 *      GEOCENT_LAT_ERROR       : Latitude out of valid range
 *                                 (-90 to 90 degrees)
 *      GEOCENT_LON_ERROR       : Longitude out of valid range
 *                                 (-180 to 360 degrees)
 *      GEOCENT_A_ERROR         : Semi-major axis less than or equal to zero
 *      GEOCENT_B_ERROR         : Semi-minor axis less than or equal to zero
 *      GEOCENT_A_LESS_B_ERROR  : Semi-major axis less than semi-minor axis
 *
 *
 * REUSE NOTES
 *
 *    GEOCENTRIC is intended for reuse by any application that performs
 *    coordinate conversions between geodetic coordinates and geocentric
 *    coordinates.
 *    
 *
 * REFERENCES
 *    
 *    An Improved Algorithm for Geocentric to Geodetic Coordinate Conversion,
 *    Ralph Toms, February 1996  UCRL-JC-123138.
 *    
 *    Further information on GEOCENTRIC can be found in the Reuse Manual.
 *
 *    GEOCENTRIC originated from : U.S. Army Topographic Engineering Center
 *                                 Geospatial Information Division
 *                                 7701 Telegraph Road
 *                                 Alexandria, VA  22310-3864
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    GEOCENTRIC has no restrictions.
 *
 * ENVIRONMENT
 *
 *    GEOCENTRIC was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC version 2.8.1
 *    2. Windows 95 with MS Visual C++ version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *
 *
 */


/***************************************************************************/
/*
 *                              DEFINES
 */
#define GEOCENT_NO_ERROR        0x0000
#define GEOCENT_LAT_ERROR       0x0001
#define GEOCENT_LON_ERROR       0x0002
#define GEOCENT_A_ERROR         0x0004
#define GEOCENT_B_ERROR         0x0008
#define GEOCENT_A_LESS_B_ERROR  0x0010


/***************************************************************************/
/*
 *                              FUNCTION PROTOTYPES
 */

/* ensure proper linkage to c++ programs */
#ifdef __cplusplus
extern "C" {
#endif

typedef struct 
{
    double Geocent_a;        /* Semi-major axis of ellipsoid in meters */
    double Geocent_b;        /* Semi-minor axis of ellipsoid           */
    double Geocent_a2;       /* Square of semi-major axis */
    double Geocent_b2;       /* Square of semi-minor axis */
    double Geocent_e2;       /* Eccentricity squared  */
    double Geocent_ep2;      /* 2nd eccentricity squared */
} GeocentricInfo;

void pj_Init_Geocentric( GeocentricInfo *gi );
long pj_Set_Geocentric_Parameters( GeocentricInfo *gi, 
                                   double a, 
                                   double b);

/*
 * The function Set_Geocentric_Parameters receives the ellipsoid parameters
 * as inputs and sets the corresponding state variables.
 *
 *    a  : Semi-major axis, in meters.          (input)
 *    b  : Semi-minor axis, in meters.          (input)
 */


void pj_Get_Geocentric_Parameters ( GeocentricInfo *gi,
                                    double *a, 
                                    double *b);

/*
 * The function Get_Geocentric_Parameters returns the ellipsoid parameters
 * to be used in geocentric coordinate conversions.
 *
 *    a  : Semi-major axis, in meters.          (output)
 *    b  : Semi-minor axis, in meters.          (output)
 */


long pj_Convert_Geodetic_To_Geocentric ( GeocentricInfo *gi,
                                         double Latitude,
                                         double Longitude,
                                         double Height,
                                         double *X,
                                         double *Y,
                                         double *Z);
/*
 * The function Convert_Geodetic_To_Geocentric converts geodetic coordinates
 * (latitude, longitude, and height) to geocentric coordinates (X, Y, Z),
 * according to the current ellipsoid parameters.
 *
 *    Latitude  : Geodetic latitude in radians                     (input)
 *    Longitude : Geodetic longitude in radians                    (input)
 *    Height    : Geodetic height, in meters                       (input)
 *    X         : Calculated Geocentric X coordinate, in meters.   (output)
 *    Y         : Calculated Geocentric Y coordinate, in meters.   (output)
 *    Z         : Calculated Geocentric Z coordinate, in meters.   (output)
 *
 */


void pj_Convert_Geocentric_To_Geodetic (GeocentricInfo *gi,
                                        double X,
                                        double Y, 
                                        double Z,
                                        double *Latitude,
                                        double *Longitude,
                                        double *Height);
/*
 * The function Convert_Geocentric_To_Geodetic converts geocentric
 * coordinates (X, Y, Z) to geodetic coordinates (latitude, longitude, 
 * and height), according to the current ellipsoid parameters.
 *
 *    X         : Geocentric X coordinate, in meters.         (input)
 *    Y         : Geocentric Y coordinate, in meters.         (input)
 *    Z         : Geocentric Z coordinate, in meters.         (input)
 *    Latitude  : Calculated latitude value in radians.       (output)
 *    Longitude : Calculated longitude value in radians.      (output)
 *    Height    : Calculated height value, in meters.         (output)
 */


#ifdef __cplusplus
}
#endif

#endif /* GEOCENT_H */

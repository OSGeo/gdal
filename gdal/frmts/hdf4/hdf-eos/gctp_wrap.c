/******************************************************************************
 * $Id$
 *
 * Project:  Hierarchical Data Format Release 4 (HDF4)
 * Purpose:  This is the wrapper code to use OGR Coordinate Transformation
 *           services instead of GCTP library
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2004, Andrey Kiselev <dron@ak4719.spb.edu>
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

#include "ogr_srs_api.h"
#include <stdlib.h>

#include "mfhdf.h"

#include <math.h>

#ifndef PI
#ifndef M_PI
#define PI (3.141592653589793238)
#else
#define PI (M_PI)
#endif
#endif

#define DEG (180.0 / PI)
#define RAD (PI / 180.0)

void for_init(
int32 outsys,       /* output system code                               */
int32 outzone,      /* output zone number                               */
float64 *outparm,   /* output array of projection parameters    */
int32 outdatum,     /* output datum                                     */
char *fn27,         /* NAD 1927 parameter file                  */
char *fn83,         /* NAD 1983 parameter file                  */
int32 *iflg,        /* status flag                                      */
int32 (*for_trans[])(double, double, double *, double *));

void inv_init(
int32 insys,            /* input system code                            */
int32 inzone,           /* input zone number                            */
float64 *inparm,        /* input array of projection parameters         */
int32 indatum,      /* input datum code                         */
char *fn27,                 /* NAD 1927 parameter file                  */
char *fn83,                 /* NAD 1983 parameter file                  */
int32 *iflg,            /* status flag                                  */
int32 (*inv_trans[])(double, double, double*, double*));

/***** static vars to store the transformers in *****/
/***** this is not thread safe *****/

static OGRCoordinateTransformationH hForCT, hInvCT;

/******************************************************************************
 function for forward gctp transformation

 gctp expects Longitude and Latitude values to be in radians
******************************************************************************/

static int32 osr_for(
double lon,			/* (I) Longitude 		*/
double lat,			/* (I) Latitude 		*/
double *x,			/* (O) X projection coordinate 	*/
double *y)			/* (O) Y projection coordinate 	*/
{

    double dfX, dfY, dfZ = 0.0;

    dfX = lon * DEG;
    dfY = lat * DEG;
    
    OCTTransform( hForCT, 1, &dfX, &dfY, &dfZ);

    *x = dfX;
    *y = dfY;
    
    return 0;
}

/******************************************************************************
 function to init a forward gctp transformer
******************************************************************************/

void for_init(
int32 outsys,       /* output system code				*/
int32 outzone,      /* output zone number				*/
float64 *outparm,   /* output array of projection parameters	*/
int32 outdatum,     /* output datum					*/
CPL_UNUSED char *fn27,         /* NAD 1927 parameter file			*/
CPL_UNUSED char *fn83,         /* NAD 1983 parameter file			*/
int32 *iflg,        /* status flag					*/
int32 (*for_trans[])(double, double, double *, double *))
                        /* forward function pointer			*/
{
    OGRSpatialReferenceH hOutSourceSRS, hLatLong = NULL;
    
    *iflg = 0;
    
    hOutSourceSRS = OSRNewSpatialReference( NULL );
    OSRSetAxisMappingStrategy(hOutSourceSRS, OAMS_TRADITIONAL_GIS_ORDER);

    OSRImportFromUSGS( hOutSourceSRS, outsys, outzone, outparm, outdatum     );
    hLatLong = OSRNewSpatialReference ( SRS_WKT_WGS84_LAT_LONG );
    OSRSetAxisMappingStrategy(hLatLong, OAMS_TRADITIONAL_GIS_ORDER);

    hForCT = OCTNewCoordinateTransformation( hLatLong, hOutSourceSRS );

    OSRDestroySpatialReference( hOutSourceSRS );
    OSRDestroySpatialReference( hLatLong );
    
    for_trans[outsys] = osr_for;
}

/******************************************************************************
 function for inverse gctp transformation

 gctp returns Longitude and Latitude values in radians
******************************************************************************/

static int32 osr_inv(
double x,           /* (I) X projection coordinate 	*/
double y,           /* (I) Y projection coordinate 	*/
double *lon,        /* (O) Longitude 		*/
double *lat)        /* (O) Latitude 		*/
{

    double dfX, dfY, dfZ = 0.0;
    
    dfX = x;
    dfY = y;

    OCTTransform( hInvCT, 1, &dfX, &dfY, &dfZ );

    *lon = dfX * RAD;
    *lat = dfY * RAD;

    return 0;
}

/******************************************************************************
 function to init a inverse gctp transformer
******************************************************************************/

void inv_init(
int32 insys,		/* input system code				*/
int32 inzone,		/* input zone number				*/
float64 *inparm,	/* input array of projection parameters         */
int32 indatum,	    /* input datum code			        */
CPL_UNUSED char *fn27,		    /* NAD 1927 parameter file			*/
CPL_UNUSED char *fn83,		    /* NAD 1983 parameter file			*/
int32 *iflg,		/* status flag					*/
int32 (*inv_trans[])(double, double, double*, double*))	
                        /* inverse function pointer			*/
{
    
    OGRSpatialReferenceH hInSourceSRS, hLatLong = NULL;
    *iflg = 0;
    
    hInSourceSRS = OSRNewSpatialReference( NULL );
    OSRSetAxisMappingStrategy(hInSourceSRS, OAMS_TRADITIONAL_GIS_ORDER);
    OSRImportFromUSGS( hInSourceSRS, insys, inzone, inparm, indatum );

    hLatLong = OSRNewSpatialReference ( SRS_WKT_WGS84_LAT_LONG );
    OSRSetAxisMappingStrategy(hLatLong, OAMS_TRADITIONAL_GIS_ORDER);

    hInvCT = OCTNewCoordinateTransformation( hInSourceSRS, hLatLong );

    OSRDestroySpatialReference( hInSourceSRS );
    OSRDestroySpatialReference( hLatLong );
    
    inv_trans[insys] = osr_inv;
}

/******************************************************************************
 function to cleanup the transformers

 note: gctp does not have a function that does this
******************************************************************************/
#ifndef GDAL_COMPILATION
void gctp_destroy(void) {
    OCTDestroyCoordinateTransformation ( hForCT );
    OCTDestroyCoordinateTransformation ( hInvCT );
}
#endif

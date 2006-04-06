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
 ******************************************************************************
 * 
 * $Log$
 * Revision 1.2  2006/04/06 14:22:02  dron
 * Added explicit copyright statement.
 *
 */

#include <ogr_srs_api.h>

static int iOutsys, iOutzone, iOutdatum, iInsys, iInzone, iIndatum;
static double *pdfOutparm, *pdfInparm;

int osr_for(
double lon,			/* (I) Longitude 		*/
double lat,			/* (I) Latitude 		*/
double *x,			/* (O) X projection coordinate 	*/
double *y)			/* (O) Y projection coordinate 	*/
{
    OGRSpatialReferenceH hSourceSRS, hLatLong;
    OGRCoordinateTransformationH hCT;
    double dfX, dfY, dfZ = 0.0;

    hSourceSRS = OSRNewSpatialReference( NULL );
    OSRImportFromUSGS( hSourceSRS, iOutsys, iOutzone, pdfOutparm, iOutdatum );
    hLatLong = OSRCloneGeogCS( hSourceSRS );
    hCT = OCTNewCoordinateTransformation( hLatLong, hSourceSRS );
    if( hCT == NULL )
    {
        ;
    }

    dfY = lon, dfX = lat;
    if( !OCTTransform( hCT, 1, &dfX, &dfY, &dfZ ) )
        ;

    *x = dfX, *y = dfY;

    OSRDestroySpatialReference( hSourceSRS );
    OSRDestroySpatialReference( hLatLong );

    return 0;
}

int for_init(
int outsys,		/* output system code				*/
int outzone,		/* output zone number				*/
double *outparm,	/* output array of projection parameters	*/
int outdatum,		/* output datum					*/
char *fn27,		/* NAD 1927 parameter file			*/
char *fn83,		/* NAD 1983 parameter file			*/
int *iflg,		/* status flag					*/
int (*for_trans[])(double, double, double *, double *))
                        /* forward function pointer			*/
{
    iflg = 0;
    iOutsys = outsys;
    iOutzone = outzone;
    pdfOutparm = outparm;
    iOutdatum = outdatum;
    for_trans[iOutsys] = osr_for;

    return 0;
}

int osr_inv(
double x,			/* (O) X projection coordinate 	*/
double y,			/* (O) Y projection coordinate 	*/
double *lon,			/* (I) Longitude 		*/
double *lat)			/* (I) Latitude 		*/
{
    OGRSpatialReferenceH hSourceSRS, hLatLong;
    OGRCoordinateTransformationH hCT;
    double dfX, dfY, dfZ = 0.0;

    hSourceSRS = OSRNewSpatialReference( NULL );
    OSRImportFromUSGS( hSourceSRS, iInsys, iInzone, pdfInparm, iIndatum );
    hLatLong = OSRCloneGeogCS( hSourceSRS );
    hCT = OCTNewCoordinateTransformation( hSourceSRS, hLatLong );
    if( hCT == NULL )
    {
        ;
    }

    //OSRDestroySpatialReference();

    dfX = x, dfY = x;
    if( !OCTTransform( hCT, 1, &dfX, &dfY, &dfZ ) )
        ;

    *lon = dfX, *lat = dfY;

    return 0;
}

int inv_init(
int insys,		/* input system code				*/
int inzone,		/* input zone number				*/
double *inparm,	        /* input array of projection parameters         */
int indatum,	        /* input datum code			        */
char *fn27,		/* NAD 1927 parameter file			*/
char *fn83,		/* NAD 1983 parameter file			*/
int *iflg,		/* status flag					*/
int (*inv_trans[])(double, double, double*, double*))	
                        /* inverse function pointer			*/
{
    iflg = 0;
    iInsys = insys;
    iInzone = inzone;
    pdfInparm = inparm; 
    iIndatum = indatum;
    inv_trans[insys] = osr_inv;

    return 0;
}


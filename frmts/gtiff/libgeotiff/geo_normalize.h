/******************************************************************************
 * $Id: geo_normalize.c,v 1.1 1999/03/09 15:57:04 geotiff Exp $
 *
 * Project:  libgeotiff
 * Purpose:  Include file related to geo_normalize.c containing Code to
 *           normalize PCS and other composite codes in a GeoTIFF file.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 * $Log: geo_normalize.c,v $
 */

#ifndef GEO_NORMALIZE_H_INCLUDED
#define GEO_NORMALIZE_H_INCLUDED

#include "geotiff.h"

#ifdef __cplusplus
extern "C" {
#endif
    
#define MAX_GTIF_PROJPARMS 	10

typedef struct {
    short	Model;		/* GTModelTypeGeoKey:
                                   ModelTypeGeographic or ModelTypeProjected */
    short	PCS;		/* ProjectedCSTypeGeoKey */
    short	GCS;		/* GeographicTypeGeoKey (Datum+PM) */

    short	UOMLength;	/* ProjLinearUnitsGeoKey (eg. Linear_Meter) */
    double	UOMLengthInMeters;  /* one UOM = UOMLengthInMeters meters*/

    short	Datum;		/* GeogGeodeticDatumGeoKey */

    short	PM;		/* GeogPrimeMeridianGeoKey */
    double	PMLongToGreenwich; /* dec. deg (Long of PM rel.to Green)*/

    short	Ellipsoid;	/* GeogEllipsoidGeoKey */
    double	SemiMajor;	/* in meters */
    double	SemiMinor;	/* in meters */

    short	ProjCode;	/* ProjectionGeoKey ... eg. Proj_UTM_11S */

    short	Projection;	/* EPSG code from TRF_METHOD */
    short	CTProjection;   /* ProjCoordTransGeoKey:
                                   GeoTIFF CT_* code from geo_ctrans.inc
                                   eg. CT_TransverseMercator */

    int		nParms;
    double	ProjParm[MAX_GTIF_PROJPARMS];
    int		ProjParmId[MAX_GTIF_PROJPARMS]; /* geokey identifier,
                                                   eg. ProjFalseEastingGeoKey*/

} GTIFDefn;

int GTIFGetPCSInfo( int nPCSCode, char **ppszEPSGName,
                    short *pnUOMLengthCode, short *pnUOMAngleCode,
                    short *pnGeogCS, short *pnProjectionCSCode );
int GTIFGetProjTRFInfo( int nProjTRFCode,
                        char ** ppszProjTRFName,
                        short * pnProjMethod,
                        double * padfProjParms );
int GTIFGetGCSInfo( int nGCSCode, char **ppszName,
                    short *pnDatum, short *pnPM );
int GTIFGetDatumInfo( int nDatumCode, char **ppszName, short * pnEllipsoid );
int GTIFGetEllipsoidInfo( int nEllipsoid, char ** ppszName,
                          double * pdfSemiMajor, double * pdfSemiMinor );
int GTIFGetPMInfo( int nPM, char **ppszName, double * pdfLongToGreenwich );

double GTIFAngleStringToDD( const char *pszAngle, int nUOMAngle );
int GTIFGetUOMLengthInfo( int nUOMLengthCode,
                          char **ppszUOMName,
                          double * pdfInMeters );

int GTIFGetDefn( GTIF *, GTIFDefn * );
void GTIFPrintDefn( GTIFDefn *, FILE * );
void GTIFFreeDefn( GTIF * );

char * GTIFGetProj4Defn( GTIFDefn * );

void SetCSVFilenameHook( const char *(*)(const char *) );

#ifdef __cplusplus
}
#endif
    
#endif /* ndef GEO_NORMALIZE_H_INCLUDED */

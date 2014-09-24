/******************************************************************************
 * $Id: geo_normalize.h 1983 2011-03-10 02:10:00Z warmerdam $
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
 *****************************************************************************/

#ifndef GEO_NORMALIZE_H_INCLUDED
#define GEO_NORMALIZE_H_INCLUDED

#include <stdio.h>
#include "geotiff.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \file geo_normalize.h
 *
 * Include file for extended projection definition normalization api.
 */
    
#define MAX_GTIF_PROJPARMS 	10

/**
 * Holds a definition of a coordinate system in normalized form.
 */

typedef struct {
    /** From GTModelTypeGeoKey tag.  Can have the values ModelTypeGeographic
        or ModelTypeProjected. */
    short	Model;

    /** From ProjectedCSTypeGeoKey tag.  For example PCS_NAD27_UTM_zone_3N.*/
    short	PCS;

    /** From GeographicTypeGeoKey tag.  For example GCS_WGS_84 or
        GCS_Voirol_1875_Paris.  Includes datum and prime meridian value. */
    short	GCS;	      

    /** From ProjLinearUnitsGeoKey.  For example Linear_Meter. */
    short	UOMLength;

    /** One UOMLength = UOMLengthInMeters meters. */
    double	UOMLengthInMeters;

    /** The angular units of the GCS. */
    short       UOMAngle;

    /** One UOMAngle = UOMLengthInDegrees degrees. */
    double      UOMAngleInDegrees;
    
    /** Datum from GeogGeodeticDatumGeoKey tag. For example Datum_WGS84 */
    short	Datum;

    /** Prime meridian from GeogPrimeMeridianGeoKey.  For example PM_Greenwich
        or PM_Paris. */
    short	PM;

    /** Decimal degrees of longitude between this prime meridian and
        Greenwich.  Prime meridians to the west of Greenwich are negative. */
    double	PMLongToGreenwich;

    /** Ellipsoid identifier from GeogELlipsoidGeoKey.  For example
        Ellipse_Clarke_1866. */
    short	Ellipsoid;

    /** The length of the semi major ellipse axis in meters. */
    double	SemiMajor;

    /** The length of the semi minor ellipse axis in meters. */
    double	SemiMinor;

    /** TOWGS84 transformation values (0/3/7) */
    short       TOWGS84Count;

    /** TOWGS84 transformation values */
    double      TOWGS84[7];

    /** Projection id from ProjectionGeoKey.  For example Proj_UTM_11S. */
    short	ProjCode;

    /** EPSG identifier for underlying projection method.  From the EPSG
        TRF_METHOD table.  */
    short	Projection;

    /** GeoTIFF identifier for underlying projection method.  While some of
      these values have corresponding vlaues in EPSG (Projection field),
      others do not.  For example CT_TransverseMercator. */
    short	CTProjection;   

    /** Number of projection parameters in ProjParm and ProjParmId. */
    int		nParms;

    /** Projection parameter value.  The identify of this parameter
        is established from the corresponding entry in ProjParmId.  The
        value will be measured in meters, or decimal degrees if it is a
        linear or angular measure. */
    double	ProjParm[MAX_GTIF_PROJPARMS];

    /** Projection parameter identifier.  For example ProjFalseEastingGeoKey.
        The value will be 0 for unused table entries. */
    int		ProjParmId[MAX_GTIF_PROJPARMS]; /* geokey identifier,
                                                   eg. ProjFalseEastingGeoKey*/

    /** Special zone map system code (MapSys_UTM_South, MapSys_UTM_North,
        MapSys_State_Plane or KvUserDefined if none apply. */
    int		MapSys;

    /** UTM, or State Plane Zone number, zero if not known. */
    int		Zone;

    /** Do we have any definition at all?  0 if no geokeys found */
    int         DefnSet;

} GTIFDefn;

int CPL_DLL GTIFGetPCSInfo( int nPCSCode, char **ppszEPSGName,
                            short *pnProjOp, 
                            short *pnUOMLengthCode, short *pnGeogCS );
int CPL_DLL GTIFGetProjTRFInfo( int nProjTRFCode,
                                char ** ppszProjTRFName,
                                short * pnProjMethod,
                                double * padfProjParms );
int CPL_DLL GTIFGetGCSInfo( int nGCSCode, char **ppszName,
                            short *pnDatum, short *pnPM, short *pnUOMAngle );
int CPL_DLL GTIFGetDatumInfo( int nDatumCode, char **ppszName,
                              short * pnEllipsoid );
int CPL_DLL GTIFGetEllipsoidInfo( int nEllipsoid, char ** ppszName,
                                  double * pdfSemiMajor,
                                  double * pdfSemiMinor );
int CPL_DLL GTIFGetPMInfo( int nPM, char **ppszName,
                           double * pdfLongToGreenwich );

double CPL_DLL GTIFAngleStringToDD( const char *pszAngle, int nUOMAngle );
int CPL_DLL GTIFGetUOMLengthInfo( int nUOMLengthCode,
                                  char **ppszUOMName,
                                  double * pdfInMeters );
int CPL_DLL GTIFGetUOMAngleInfo( int nUOMAngleCode,
                                 char **ppszUOMName,
                                 double * pdfInDegrees );
double CPL_DLL GTIFAngleToDD( double dfAngle, int nUOMAngle );
    

/* this should be used to free strings returned by GTIFGet... funcs */
void CPL_DLL GTIFFreeMemory( char * );
void CPL_DLL GTIFDeaccessCSV( void );

int CPL_DLL GTIFGetDefn( GTIF *psGTIF, GTIFDefn * psDefn );
void CPL_DLL GTIFPrintDefn( GTIFDefn *, FILE * );
void CPL_DLL GTIFFreeDefn( GTIF * );

void CPL_DLL SetCSVFilenameHook( const char *(*CSVFileOverride)(const char *) );

const char CPL_DLL *GTIFDecToDMS( double, const char *, int );

/*
 * These are useful for recognising UTM and State Plane, with or without
 * CSV files being found.
 */

#define MapSys_UTM_North	-9001
#define MapSys_UTM_South	-9002
#define MapSys_State_Plane_27	-9003
#define MapSys_State_Plane_83	-9004

int CPL_DLL   GTIFMapSysToPCS( int MapSys, int Datum, int nZone );
int CPL_DLL   GTIFMapSysToProj( int MapSys, int nZone );
int CPL_DLL   GTIFPCSToMapSys( int PCSCode, int * pDatum, int * pZone );
int CPL_DLL   GTIFProjToMapSys( int ProjCode, int * pZone );

/*
 * These are only useful if using libgeotiff with libproj (PROJ.4+).
 */
char CPL_DLL *GTIFGetProj4Defn( GTIFDefn * );

int  CPL_DLL  GTIFProj4ToLatLong( GTIFDefn *, int, double *, double * );
int  CPL_DLL  GTIFProj4FromLatLong( GTIFDefn *, int, double *, double * );

int  CPL_DLL  GTIFSetFromProj4( GTIF *gtif, const char *proj4 );

#if defined(HAVE_LIBPROJ) && defined(HAVE_PROJECTS_H)
#  define HAVE_GTIFPROJ4
#endif

#ifdef __cplusplus
}
#endif
    
#endif /* ndef GEO_NORMALIZE_H_INCLUDED */

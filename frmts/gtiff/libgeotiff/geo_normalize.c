/******************************************************************************
 * $Id: geo_normalize.c 2209 2012-05-09 01:34:58Z warmerdam $
 *
 * Project:  libgeotiff
 * Purpose:  Code to normalize PCS and other composite codes in a GeoTIFF file.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 
#include "cpl_serv.h"
#include "geo_tiffp.h"
#include "geovalues.h"
#include "geo_normalize.h"

#ifndef KvUserDefined
#  define KvUserDefined 32767
#endif

#ifndef PI
#  define PI 3.14159265358979323846
#endif

/* EPSG Codes for projection parameters.  Unfortunately, these bear no
   relationship to the GeoTIFF codes even though the names are so similar. */

#define EPSGNatOriginLat         8801
#define EPSGNatOriginLong        8802
#define EPSGNatOriginScaleFactor 8805
#define EPSGFalseEasting         8806
#define EPSGFalseNorthing        8807
#define EPSGProjCenterLat        8811
#define EPSGProjCenterLong       8812
#define EPSGAzimuth              8813
#define EPSGAngleRectifiedToSkewedGrid 8814
#define EPSGInitialLineScaleFactor 8815
#define EPSGProjCenterEasting    8816
#define EPSGProjCenterNorthing   8817
#define EPSGPseudoStdParallelLat 8818
#define EPSGPseudoStdParallelScaleFactor 8819
#define EPSGFalseOriginLat       8821
#define EPSGFalseOriginLong      8822
#define EPSGStdParallel1Lat      8823
#define EPSGStdParallel2Lat      8824
#define EPSGFalseOriginEasting   8826
#define EPSGFalseOriginNorthing  8827
#define EPSGSphericalOriginLat   8828
#define EPSGSphericalOriginLong  8829
#define EPSGInitialLongitude     8830
#define EPSGZoneWidth            8831
#define EPSGLatOfStdParallel     8832
#define EPSGOriginLong           8833
#define EPSGTopocentricOriginLat 8834
#define EPSGTopocentricOriginLong 8835
#define EPSGTopocentricOriginHeight 8836

/************************************************************************/
/*                           GTIFGetPCSInfo()                           */
/************************************************************************/

int GTIFGetPCSInfo( int nPCSCode, char **ppszEPSGName, 
                    short *pnProjOp, short *pnUOMLengthCode, 
                    short *pnGeogCS )

{
    char	**papszRecord;
    char	szSearchKey[24];
    const char	*pszFilename;
    int         nDatum;
    int         nZone;

    int Proj = GTIFPCSToMapSys( nPCSCode, &nDatum, &nZone );
    if ((Proj == MapSys_UTM_North || Proj == MapSys_UTM_South) &&
        nDatum != KvUserDefined)
    {
        const char* pszDatumName = NULL;
        switch (nDatum)
        {
            case GCS_NAD27: pszDatumName = "NAD27"; break;
            case GCS_NAD83: pszDatumName = "NAD83"; break;
            case GCS_WGS_72: pszDatumName = "WGS 72"; break;
            case GCS_WGS_72BE: pszDatumName = "WGS 72BE"; break;
            case GCS_WGS_84: pszDatumName = "WGS 84"; break;
            default: break;
        }

        if (pszDatumName)
        {
            if (ppszEPSGName)
            {
                char szEPSGName[64];
                sprintf(szEPSGName, "%s / UTM zone %d%c",
                        pszDatumName, nZone, (Proj == MapSys_UTM_North) ? 'N' : 'S');
                *ppszEPSGName = CPLStrdup(szEPSGName);
            }

            if (pnProjOp)
                *pnProjOp = (short) (((Proj == MapSys_UTM_North) ? Proj_UTM_zone_1N - 1 : Proj_UTM_zone_1S - 1) + nZone);

            if (pnUOMLengthCode)
                *pnUOMLengthCode = 9001; /* Linear_Meter */

            if (pnGeogCS)
                *pnGeogCS = (short) nDatum;

            return TRUE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Search the pcs.override table for this PCS.                     */
/* -------------------------------------------------------------------- */
    pszFilename = CSVFilename( "pcs.override.csv" );
    sprintf( szSearchKey, "%d", nPCSCode );
    papszRecord = CSVScanFileByName( pszFilename, "COORD_REF_SYS_CODE",
                                     szSearchKey, CC_Integer );

/* -------------------------------------------------------------------- */
/*      If not found, search the EPSG PCS database.                     */
/* -------------------------------------------------------------------- */
    if( papszRecord == NULL )
    {
        pszFilename = CSVFilename( "pcs.csv" );
        
        sprintf( szSearchKey, "%d", nPCSCode );
        papszRecord = CSVScanFileByName( pszFilename, "COORD_REF_SYS_CODE",
                                         szSearchKey, CC_Integer );

        if( papszRecord == NULL )
            return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Get the name, if requested.                                     */
/* -------------------------------------------------------------------- */
    if( ppszEPSGName != NULL )
    {
        *ppszEPSGName =
            CPLStrdup( CSLGetField( papszRecord,
                                    CSVGetFileFieldId(pszFilename,
                                                      "COORD_REF_SYS_NAME") ));
    }

/* -------------------------------------------------------------------- */
/*      Get the UOM Length code, if requested.                          */
/* -------------------------------------------------------------------- */
    if( pnUOMLengthCode != NULL )
    {
        const char	*pszValue;

        pszValue =
            CSLGetField( papszRecord,
                         CSVGetFileFieldId(pszFilename,"UOM_CODE"));
        if( atoi(pszValue) > 0 )
            *pnUOMLengthCode = (short) atoi(pszValue);
        else
            *pnUOMLengthCode = KvUserDefined;
    }

/* -------------------------------------------------------------------- */
/*      Get the UOM Length code, if requested.                          */
/* -------------------------------------------------------------------- */
    if( pnProjOp != NULL )
    {
        const char	*pszValue;

        pszValue =
            CSLGetField( papszRecord,
                         CSVGetFileFieldId(pszFilename,"COORD_OP_CODE"));
        if( atoi(pszValue) > 0 )
            *pnProjOp = (short) atoi(pszValue);
        else
            *pnUOMLengthCode = KvUserDefined;
    }

/* -------------------------------------------------------------------- */
/*      Get the GeogCS (Datum with PM) code, if requested.		*/
/* -------------------------------------------------------------------- */
    if( pnGeogCS != NULL )
    {
        const char	*pszValue;

        pszValue =
            CSLGetField( papszRecord,
                         CSVGetFileFieldId(pszFilename,"SOURCE_GEOGCRS_CODE"));
        if( atoi(pszValue) > 0 )
            *pnGeogCS = (short) atoi(pszValue);
        else
            *pnGeogCS = KvUserDefined;
    }

    return TRUE;
}

/************************************************************************/
/*                           GTIFAngleToDD()                            */
/*                                                                      */
/*      Convert a numeric angle to decimal degress.                     */
/************************************************************************/

double GTIFAngleToDD( double dfAngle, int nUOMAngle )

{
    if( nUOMAngle == 9110 )		/* DDD.MMSSsss */
    {
        char	szAngleString[32];

        sprintf( szAngleString, "%12.7f", dfAngle );
        dfAngle = GTIFAngleStringToDD( szAngleString, nUOMAngle );
    }
    else if ( nUOMAngle != KvUserDefined )
    {
        double		dfInDegrees = 1.0;
        
        GTIFGetUOMAngleInfo( nUOMAngle, NULL, &dfInDegrees );
        dfAngle = dfAngle * dfInDegrees;
    }

    return( dfAngle );
}

/************************************************************************/
/*                        GTIFAngleStringToDD()                         */
/*                                                                      */
/*      Convert an angle in the specified units to decimal degrees.     */
/************************************************************************/

double GTIFAngleStringToDD( const char * pszAngle, int nUOMAngle )

{
    double	dfAngle;
    
    if( nUOMAngle == 9110 )		/* DDD.MMSSsss */
    {
        char	*pszDecimal;
        
        dfAngle = ABS(atoi(pszAngle));
        pszDecimal = strchr(pszAngle,'.');
        if( pszDecimal != NULL && strlen(pszDecimal) > 1 )
        {
            char	szMinutes[3];
            char	szSeconds[64];

            szMinutes[0] = pszDecimal[1];
            if( pszDecimal[2] >= '0' && pszDecimal[2] <= '9' )
                szMinutes[1] = pszDecimal[2];
            else
                szMinutes[1] = '0';
            
            szMinutes[2] = '\0';
            dfAngle += atoi(szMinutes) / 60.0;

            if( strlen(pszDecimal) > 3 )
            {
                szSeconds[0] = pszDecimal[3];
                if( pszDecimal[4] >= '0' && pszDecimal[4] <= '9' )
                {
                    szSeconds[1] = pszDecimal[4];
                    szSeconds[2] = '.';
                    strncpy( szSeconds+3, pszDecimal + 5, sizeof(szSeconds) - 3 );
                    szSeconds[sizeof(szSeconds) - 1] = 0;
                }
                else
                {
                    szSeconds[1] = '0';
                    szSeconds[2] = '\0';
                }
                dfAngle += GTIFAtof(szSeconds) / 3600.0;
            }
        }

        if( pszAngle[0] == '-' )
            dfAngle *= -1;
    }
    else if( nUOMAngle == 9105 || nUOMAngle == 9106 )	/* grad */
    {
        dfAngle = 180 * (GTIFAtof(pszAngle ) / 200);
    }
    else if( nUOMAngle == 9101 )			/* radians */
    {
        dfAngle = 180 * (GTIFAtof(pszAngle ) / PI);
    }
    else if( nUOMAngle == 9103 )			/* arc-minute */
    {
        dfAngle = GTIFAtof(pszAngle) / 60;
    }
    else if( nUOMAngle == 9104 )			/* arc-second */
    {
        dfAngle = GTIFAtof(pszAngle) / 3600;
    }
    else /* decimal degrees ... some cases missing but seeminly never used */
    {
        CPLAssert( nUOMAngle == 9102 || nUOMAngle == KvUserDefined
                   || nUOMAngle == 0 );
        
        dfAngle = GTIFAtof(pszAngle );
    }

    return( dfAngle );
}

/************************************************************************/
/*                           GTIFGetGCSInfo()                           */
/*                                                                      */
/*      Fetch the datum, and prime meridian related to a particular     */
/*      GCS.                                                            */
/************************************************************************/

int GTIFGetGCSInfo( int nGCSCode, char ** ppszName,
                    short * pnDatum, short * pnPM, short *pnUOMAngle )

{
    char	szSearchKey[24];
    int		nDatum=0, nPM, nUOMAngle;
    const char *pszFilename;

/* -------------------------------------------------------------------- */
/*      Handle some "well known" GCS codes directly                     */
/* -------------------------------------------------------------------- */
    const char * pszName = NULL;
    nPM = PM_Greenwich;
    nUOMAngle = Angular_DMS_Hemisphere; 
    if( nGCSCode == GCS_NAD27 )
    {
        nDatum = Datum_North_American_Datum_1927;
        pszName = "NAD27";
    }
    else if( nGCSCode == GCS_NAD83 )
    {
        nDatum = Datum_North_American_Datum_1983;
        pszName = "NAD83";
    }
    else if( nGCSCode == GCS_WGS_84 )
    {
        nDatum = Datum_WGS84;
        pszName = "WGS 84";
    }
    else if( nGCSCode == GCS_WGS_72 )
    {
        nDatum = Datum_WGS72;
        pszName = "WGS 72";
    }
    else if ( nGCSCode == KvUserDefined )
    {
        return FALSE;
    }

    if (pszName != NULL)
    {
        if( ppszName != NULL )
            *ppszName = CPLStrdup( pszName );
        if( pnDatum != NULL )
            *pnDatum = (short) nDatum;
        if( pnPM != NULL )
            *pnPM = (short) nPM;
        if( pnUOMAngle != NULL )
            *pnUOMAngle = (short) nUOMAngle;

        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Search the database for the corresponding datum code.           */
/* -------------------------------------------------------------------- */
    pszFilename = CSVFilename("gcs.override.csv");
    sprintf( szSearchKey, "%d", nGCSCode );
    nDatum = atoi(CSVGetField( pszFilename,
                               "COORD_REF_SYS_CODE", szSearchKey, 
                               CC_Integer, "DATUM_CODE" ) );

    if( nDatum < 1 )
    {
        pszFilename = CSVFilename("gcs.csv");
        sprintf( szSearchKey, "%d", nGCSCode );
        nDatum = atoi(CSVGetField( pszFilename,
                                   "COORD_REF_SYS_CODE", szSearchKey, 
                                   CC_Integer, "DATUM_CODE" ) );
    }

    if( nDatum < 1 )
    {
        return FALSE;
    }

    if( pnDatum != NULL )
        *pnDatum = (short) nDatum;
    
/* -------------------------------------------------------------------- */
/*      Get the PM.                                                     */
/* -------------------------------------------------------------------- */
    if( pnPM != NULL )
    {
        nPM = atoi(CSVGetField( pszFilename,
                                "COORD_REF_SYS_CODE", szSearchKey, CC_Integer,
                                "PRIME_MERIDIAN_CODE" ) );

        if( nPM < 1 )
            return FALSE;

        *pnPM = (short) nPM;
    }

/* -------------------------------------------------------------------- */
/*      Get the angular units.                                          */
/* -------------------------------------------------------------------- */
    nUOMAngle = atoi(CSVGetField( pszFilename,
                                  "COORD_REF_SYS_CODE",szSearchKey, CC_Integer,
                                  "UOM_CODE" ) );

    if( nUOMAngle < 1 )
        return FALSE;

    if( pnUOMAngle != NULL )
        *pnUOMAngle = (short) nUOMAngle;

/* -------------------------------------------------------------------- */
/*      Get the name, if requested.                                     */
/* -------------------------------------------------------------------- */
    if( ppszName != NULL )
        *ppszName =
            CPLStrdup(CSVGetField( pszFilename,
                                   "COORD_REF_SYS_CODE",szSearchKey,CC_Integer,
                                   "COORD_REF_SYS_NAME" ));
    
    return( TRUE );
}

/************************************************************************/
/*                        GTIFGetEllipsoidInfo()                        */
/*                                                                      */
/*      Fetch info about an ellipsoid.  Axes are always returned in     */
/*      meters.  SemiMajor computed based on inverse flattening         */
/*      where that is provided.                                         */
/************************************************************************/

int GTIFGetEllipsoidInfo( int nEllipseCode, char ** ppszName,
                          double * pdfSemiMajor, double * pdfSemiMinor )

{
    char	szSearchKey[24];
    double	dfSemiMajor=0.0, dfToMeters = 1.0;
    int		nUOMLength;
    const char* pszFilename;

/* -------------------------------------------------------------------- */
/*      Try some well known ellipsoids.                                 */
/* -------------------------------------------------------------------- */
    double     dfInvFlattening=0.0, dfSemiMinor=0.0;
    const char *pszName = NULL;
    
    if( nEllipseCode == Ellipse_Clarke_1866 )
    {
        pszName = "Clarke 1866";
        dfSemiMajor = 6378206.4;
        dfSemiMinor = 6356583.8;
        dfInvFlattening = 0.0;
    }
    else if( nEllipseCode == Ellipse_GRS_1980 )
    {
        pszName = "GRS 1980";
        dfSemiMajor = 6378137.0;
        dfSemiMinor = 0.0;
        dfInvFlattening = 298.257222101;
    }
    else if( nEllipseCode == Ellipse_WGS_84 )
    {
        pszName = "WGS 84";
        dfSemiMajor = 6378137.0;
        dfSemiMinor = 0.0;
        dfInvFlattening = 298.257223563;
    }
    else if( nEllipseCode == 7043 )
    {
        pszName = "WGS 72";
        dfSemiMajor = 6378135.0;
        dfSemiMinor = 0.0;
        dfInvFlattening = 298.26;
    }

    if (pszName != NULL)
    {
        if( dfSemiMinor == 0.0 )
            dfSemiMinor = dfSemiMajor * (1 - 1.0/dfInvFlattening);

        if( pdfSemiMinor != NULL )
            *pdfSemiMinor = dfSemiMinor;
        if( pdfSemiMajor != NULL )
            *pdfSemiMajor = dfSemiMajor;
        if( ppszName != NULL )
            *ppszName = CPLStrdup( pszName );

        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Get the semi major axis.                                        */
/* -------------------------------------------------------------------- */
    sprintf( szSearchKey, "%d", nEllipseCode );
    pszFilename = CSVFilename("ellipsoid.csv" );

    dfSemiMajor =
        GTIFAtof(CSVGetField( pszFilename,
                          "ELLIPSOID_CODE", szSearchKey, CC_Integer,
                          "SEMI_MAJOR_AXIS" ) );

    if( dfSemiMajor == 0.0 )
    {
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*	Get the translation factor into meters.				*/
/* -------------------------------------------------------------------- */
    nUOMLength = atoi(CSVGetField( pszFilename,
                                   "ELLIPSOID_CODE", szSearchKey, CC_Integer,
                                   "UOM_CODE" ));
    GTIFGetUOMLengthInfo( nUOMLength, NULL, &dfToMeters );

    dfSemiMajor *= dfToMeters;
    
    if( pdfSemiMajor != NULL )
        *pdfSemiMajor = dfSemiMajor;
    
/* -------------------------------------------------------------------- */
/*      Get the semi-minor if requested.  If the Semi-minor axis        */
/*      isn't available, compute it based on the inverse flattening.    */
/* -------------------------------------------------------------------- */
    if( pdfSemiMinor != NULL )
    {
        *pdfSemiMinor =
            GTIFAtof(CSVGetField( pszFilename,
                              "ELLIPSOID_CODE", szSearchKey, CC_Integer,
                              "SEMI_MINOR_AXIS" )) * dfToMeters;

        if( *pdfSemiMinor == 0.0 )
        {
            double	dfInvFlattening;
            
            dfInvFlattening = 
                GTIFAtof(CSVGetField( pszFilename,
                                  "ELLIPSOID_CODE", szSearchKey, CC_Integer,
                                  "INV_FLATTENING" ));
            *pdfSemiMinor = dfSemiMajor * (1 - 1.0/dfInvFlattening);
        }
    }

/* -------------------------------------------------------------------- */
/*      Get the name, if requested.                                     */
/* -------------------------------------------------------------------- */
    if( ppszName != NULL )
        *ppszName =
            CPLStrdup(CSVGetField( pszFilename,
                                   "ELLIPSOID_CODE", szSearchKey, CC_Integer,
                                   "ELLIPSOID_NAME" ));
    
    return( TRUE );
}

/************************************************************************/
/*                           GTIFGetPMInfo()                            */
/*                                                                      */
/*      Get the offset between a given prime meridian and Greenwich     */
/*      in degrees.                                                     */
/************************************************************************/

int GTIFGetPMInfo( int nPMCode, char ** ppszName, double *pdfOffset )

{
    char	szSearchKey[24];
    int		nUOMAngle;
    const char *pszFilename;

/* -------------------------------------------------------------------- */
/*      Use a special short cut for Greenwich, since it is so common.   */
/* -------------------------------------------------------------------- */
    if( nPMCode == PM_Greenwich )
    {
        if( pdfOffset != NULL )
            *pdfOffset = 0.0;
        if( ppszName != NULL )
            *ppszName = CPLStrdup( "Greenwich" );
        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Search the database for the corresponding datum code.           */
/* -------------------------------------------------------------------- */
    pszFilename = CSVFilename("prime_meridian.csv");
    sprintf( szSearchKey, "%d", nPMCode );

    nUOMAngle =
        atoi(CSVGetField( pszFilename, 
                          "PRIME_MERIDIAN_CODE", szSearchKey, CC_Integer,
                          "UOM_CODE" ) );
    if( nUOMAngle < 1 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Get the PM offset.                                              */
/* -------------------------------------------------------------------- */
    if( pdfOffset != NULL )
    {
        *pdfOffset =
            GTIFAngleStringToDD(
                CSVGetField( pszFilename, 
                             "PRIME_MERIDIAN_CODE", szSearchKey, CC_Integer,
                             "GREENWICH_LONGITUDE" ),
                nUOMAngle );
    }
    
/* -------------------------------------------------------------------- */
/*      Get the name, if requested.                                     */
/* -------------------------------------------------------------------- */
    if( ppszName != NULL )
        *ppszName =
            CPLStrdup(
                CSVGetField( pszFilename, 
                             "PRIME_MERIDIAN_CODE", szSearchKey, CC_Integer,
                             "PRIME_MERIDIAN_NAME" ));
    
    return( TRUE );
}

/************************************************************************/
/*                          GTIFGetDatumInfo()                          */
/*                                                                      */
/*      Fetch the ellipsoid, and name for a datum.                      */
/************************************************************************/

int GTIFGetDatumInfo( int nDatumCode, char ** ppszName, short * pnEllipsoid )

{
    char	szSearchKey[24];
    int		nEllipsoid = 0;
    const char *pszFilename;
    FILE       *fp;
    const char *pszName = NULL;

/* -------------------------------------------------------------------- */
/*      Handle a few built-in datums.                                   */
/* -------------------------------------------------------------------- */
    if( nDatumCode == Datum_North_American_Datum_1927 )
    {
        nEllipsoid = Ellipse_Clarke_1866;
        pszName = "North American Datum 1927";
    }
    else if( nDatumCode == Datum_North_American_Datum_1983 )
    {
        nEllipsoid = Ellipse_GRS_1980;
        pszName = "North American Datum 1983";
    }
    else if( nDatumCode == Datum_WGS84 )
    {
        nEllipsoid = Ellipse_WGS_84;
        pszName = "World Geodetic System 1984";
    }
    else if( nDatumCode == Datum_WGS72 )
    {
        nEllipsoid = 7043; /* WGS72 */
        pszName = "World Geodetic System 1972";
    }

    if (pszName != NULL)
    {
        if( pnEllipsoid != NULL )
            *pnEllipsoid = (short) nEllipsoid;

        if( ppszName != NULL )
            *ppszName = CPLStrdup( pszName );

        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      If we can't find datum.csv then gdal_datum.csv is an            */
/*      acceptable fallback.  Mostly this is for GDAL.                  */
/* -------------------------------------------------------------------- */
    pszFilename = CSVFilename( "datum.csv" );
    if( (fp = VSIFOpen(pszFilename,"r")) == NULL )
    {
        if( (fp = VSIFOpen(CSVFilename("gdal_datum.csv"), "r")) != NULL )
        {
            pszFilename = CSVFilename( "gdal_datum.csv" );
            VSIFClose( fp );
        }        
    }
    else
        VSIFClose( fp );

/* -------------------------------------------------------------------- */
/*      Search the database for the corresponding datum code.           */
/* -------------------------------------------------------------------- */
    sprintf( szSearchKey, "%d", nDatumCode );

    nEllipsoid = atoi(CSVGetField( pszFilename,
                                   "DATUM_CODE", szSearchKey, CC_Integer,
                                   "ELLIPSOID_CODE" ) );

    if( pnEllipsoid != NULL )
        *pnEllipsoid = (short) nEllipsoid;

    if( nEllipsoid < 1 )
    {
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Get the name, if requested.                                     */
/* -------------------------------------------------------------------- */
    if( ppszName != NULL )
        *ppszName =
            CPLStrdup(CSVGetField( pszFilename,
                                   "DATUM_CODE", szSearchKey, CC_Integer,
                                   "DATUM_NAME" ));
    
    return( TRUE );
}


/************************************************************************/
/*                        GTIFGetUOMLengthInfo()                        */
/*                                                                      */
/*      Note: This function should eventually also know how to          */
/*      lookup length aliases in the UOM_LE_ALIAS table.                */
/************************************************************************/

int GTIFGetUOMLengthInfo( int nUOMLengthCode,
                          char **ppszUOMName,
                          double * pdfInMeters )

{
    char	**papszUnitsRecord;
    char	szSearchKey[24];
    int		iNameField;
    const char *pszFilename;

/* -------------------------------------------------------------------- */
/*      We short cut meter to save work and avoid failure for missing   */
/*      in the most common cases.       				*/
/* -------------------------------------------------------------------- */
    if( nUOMLengthCode == 9001 )
    {
        if( ppszUOMName != NULL )
            *ppszUOMName = CPLStrdup( "metre" );
        if( pdfInMeters != NULL )
            *pdfInMeters = 1.0;

        return TRUE;
    }

    if( nUOMLengthCode == 9002 )
    {
        if( ppszUOMName != NULL )
            *ppszUOMName = CPLStrdup( "foot" );
        if( pdfInMeters != NULL )
            *pdfInMeters = 0.3048;

        return TRUE;
    }

    if( nUOMLengthCode == 9003 )
    {
        if( ppszUOMName != NULL )
            *ppszUOMName = CPLStrdup( "US survey foot" );
        if( pdfInMeters != NULL )
            *pdfInMeters = 12.0 / 39.37;

        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Search the units database for this unit.  If we don't find      */
/*      it return failure.                                              */
/* -------------------------------------------------------------------- */
    pszFilename = CSVFilename( "unit_of_measure.csv" );

    sprintf( szSearchKey, "%d", nUOMLengthCode );
    papszUnitsRecord =
        CSVScanFileByName( pszFilename,
                           "UOM_CODE", szSearchKey, CC_Integer );

    if( papszUnitsRecord == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Get the name, if requested.                                     */
/* -------------------------------------------------------------------- */
    if( ppszUOMName != NULL )
    {
        iNameField = CSVGetFileFieldId( pszFilename,
                                        "UNIT_OF_MEAS_NAME" );
        *ppszUOMName = CPLStrdup( CSLGetField(papszUnitsRecord, iNameField) );
    }
    
/* -------------------------------------------------------------------- */
/*      Get the A and B factor fields, and create the multiplicative    */
/*      factor.                                                         */
/* -------------------------------------------------------------------- */
    if( pdfInMeters != NULL )
    {
        int	iBFactorField, iCFactorField;
        
        iBFactorField = CSVGetFileFieldId( pszFilename, "FACTOR_B" );
        iCFactorField = CSVGetFileFieldId( pszFilename, "FACTOR_C" );

        if( GTIFAtof(CSLGetField(papszUnitsRecord, iCFactorField)) > 0.0 )
            *pdfInMeters = GTIFAtof(CSLGetField(papszUnitsRecord, iBFactorField))
                / GTIFAtof(CSLGetField(papszUnitsRecord, iCFactorField));
        else
            *pdfInMeters = 0.0;
    }
    
    return( TRUE );
}

/************************************************************************/
/*                        GTIFGetUOMAngleInfo()                         */
/************************************************************************/

int GTIFGetUOMAngleInfo( int nUOMAngleCode,
                         char **ppszUOMName,
                         double * pdfInDegrees )

{
    const char	*pszUOMName = NULL;
    double	dfInDegrees = 1.0;
    const char *pszFilename;
    char	szSearchKey[24];

    switch( nUOMAngleCode )
    {
      case 9101:
        pszUOMName = "radian";
        dfInDegrees = 180.0 / PI;
        break;

      case 9102:
      case 9107:
      case 9108:
      case 9110:
      case 9122:
        pszUOMName = "degree";
        dfInDegrees = 1.0;
        break;

      case 9103:
        pszUOMName = "arc-minute";
        dfInDegrees = 1 / 60.0;
        break;

      case 9104:
        pszUOMName = "arc-second";
        dfInDegrees = 1 / 3600.0;
        break;

      case 9105:
        pszUOMName = "grad";
        dfInDegrees = 180.0 / 200.0;
        break;

      case 9106:
        pszUOMName = "gon";
        dfInDegrees = 180.0 / 200.0;
        break;

      case 9109:
        pszUOMName = "microradian";
        dfInDegrees = 180.0 / (PI * 1000000.0);
        break;

      default:
        break;
    }
    
    if (pszUOMName)
    {
        if( ppszUOMName != NULL )
        {
            if( pszUOMName != NULL )
                *ppszUOMName = CPLStrdup( pszUOMName );
            else
                *ppszUOMName = NULL;
        }

        if( pdfInDegrees != NULL )
            *pdfInDegrees = dfInDegrees;

        return TRUE;
    }

    pszFilename = CSVFilename( "unit_of_measure.csv" );
    sprintf( szSearchKey, "%d", nUOMAngleCode );
    pszUOMName = CSVGetField( pszFilename,
                              "UOM_CODE", szSearchKey, CC_Integer,
                              "UNIT_OF_MEAS_NAME" );

/* -------------------------------------------------------------------- */
/*      If the file is found, read from there.  Note that FactorC is    */
/*      an empty field for any of the DMS style formats, and in this    */
/*      case we really want to return the default InDegrees value       */
/*      (1.0) from above.                                               */
/* -------------------------------------------------------------------- */
    if( pszUOMName != NULL )
    {
        double dfFactorB, dfFactorC, dfInRadians;
        
        dfFactorB = 
            GTIFAtof(CSVGetField( pszFilename,
                              "UOM_CODE", szSearchKey, CC_Integer,
                              "FACTOR_B" ));
        
        dfFactorC = 
            GTIFAtof(CSVGetField( pszFilename,
                              "UOM_CODE", szSearchKey, CC_Integer,
                              "FACTOR_C" ));

        if( dfFactorC != 0.0 )
        {
            dfInRadians = (dfFactorB / dfFactorC);
            dfInDegrees = dfInRadians * 180.0 / PI;
        }
    }
    else
    {
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Return to caller.                                               */
/* -------------------------------------------------------------------- */
    if( ppszUOMName != NULL )
    {
        if( pszUOMName != NULL )
            *ppszUOMName = CPLStrdup( pszUOMName );
        else
            *ppszUOMName = NULL;
    }

    if( pdfInDegrees != NULL )
        *pdfInDegrees = dfInDegrees;

    return( TRUE );
}

/************************************************************************/
/*                    EPSGProjMethodToCTProjMethod()                    */
/*                                                                      */
/*      Convert between the EPSG enumeration for projection methods,    */
/*      and the GeoTIFF CT codes.                                       */
/************************************************************************/

static int EPSGProjMethodToCTProjMethod( int nEPSG )

{
    /* see trf_method.csv for list of EPSG codes */
    
    switch( nEPSG )
    {
      case 9801:
        return( CT_LambertConfConic_1SP );

      case 9802:
        return( CT_LambertConfConic_2SP );

      case 9803:
        return( CT_LambertConfConic_2SP ); /* Belgian variant not supported */

      case 9804:
        return( CT_Mercator );  /* 1SP and 2SP not differentiated */

      case 9805:
        return( CT_Mercator );  /* 1SP and 2SP not differentiated */
        
      /* Mercator 1SP (Spherical) For EPSG:3785 */
      case 9841:
        return( CT_Mercator );  /* 1SP and 2SP not differentiated */
        
      /* Google Mercator For EPSG:3857 */
      case 1024:
        return( CT_Mercator );  /* 1SP and 2SP not differentiated */

      case 9806:
        return( CT_CassiniSoldner );

      case 9807:
        return( CT_TransverseMercator );

      case 9808:
        return( CT_TransvMercator_SouthOriented );

      case 9809:
        return( CT_ObliqueStereographic );

      case 9810:
        /* case 9829: variant B not quite the same */ 
        return( CT_PolarStereographic );

      case 9811:
        return( CT_NewZealandMapGrid );

      case 9812:
        return( CT_ObliqueMercator ); /* is hotine actually different? */

      case 9813:
        return( CT_ObliqueMercator_Laborde );

      case 9814:
        return( CT_ObliqueMercator_Rosenmund ); /* swiss  */

      case 9815:
        return( CT_HotineObliqueMercatorAzimuthCenter );

      case 9816: /* tunesia mining grid has no counterpart */
        return( KvUserDefined );

      case 9820:
      case 1027:
        return( CT_LambertAzimEqualArea );

      case 9822:
        return( CT_AlbersEqualArea );

      case 9834:
        return( CT_CylindricalEqualArea );

      default: /* use the EPSG code for other methods */
        return nEPSG;
    }

    return( KvUserDefined );
}

/************************************************************************/
/*                            SetGTParmIds()                            */
/*                                                                      */
/*      This is hardcoded logic to set the GeoTIFF parmaeter            */
/*      identifiers for all the EPSG supported projections.  As the     */
/*      trf_method.csv table grows with new projections, this code      */
/*      will need to be updated.                                        */
/************************************************************************/

static int SetGTParmIds( int nCTProjection, 
                         int *panProjParmId, 
                         int *panEPSGCodes )

{
    int anWorkingDummy[7];

    if( panEPSGCodes == NULL )
        panEPSGCodes = anWorkingDummy;
    if( panProjParmId == NULL )
        panProjParmId = anWorkingDummy;

    memset( panEPSGCodes, 0, sizeof(int) * 7 );

    /* psDefn->nParms = 7; */
    
    switch( nCTProjection )
    {
      case CT_CassiniSoldner:
      case CT_NewZealandMapGrid:
        panProjParmId[0] = ProjNatOriginLatGeoKey;
        panProjParmId[1] = ProjNatOriginLongGeoKey;
        panProjParmId[5] = ProjFalseEastingGeoKey;
        panProjParmId[6] = ProjFalseNorthingGeoKey;

        panEPSGCodes[0] = EPSGNatOriginLat;
        panEPSGCodes[1] = EPSGNatOriginLong;
        panEPSGCodes[5] = EPSGFalseEasting;
        panEPSGCodes[6] = EPSGFalseNorthing;
        return TRUE;

      case CT_ObliqueMercator:
      case CT_HotineObliqueMercatorAzimuthCenter:
        panProjParmId[0] = ProjCenterLatGeoKey;
        panProjParmId[1] = ProjCenterLongGeoKey;
        panProjParmId[2] = ProjAzimuthAngleGeoKey;
        panProjParmId[3] = ProjRectifiedGridAngleGeoKey;
        panProjParmId[4] = ProjScaleAtCenterGeoKey;
        panProjParmId[5] = ProjFalseEastingGeoKey;
        panProjParmId[6] = ProjFalseNorthingGeoKey;

        panEPSGCodes[0] = EPSGProjCenterLat;
        panEPSGCodes[1] = EPSGProjCenterLong;
        panEPSGCodes[2] = EPSGAzimuth;
        panEPSGCodes[3] = EPSGAngleRectifiedToSkewedGrid;
        panEPSGCodes[4] = EPSGInitialLineScaleFactor;
        panEPSGCodes[5] = EPSGProjCenterEasting; /* EPSG proj method 9812 uses EPSGFalseEasting, but 9815 uses EPSGProjCenterEasting */
        panEPSGCodes[6] = EPSGProjCenterNorthing; /* EPSG proj method 9812 uses EPSGFalseNorthing, but 9815 uses EPSGProjCenterNorthing */
        return TRUE;

      case CT_ObliqueMercator_Laborde:
        panProjParmId[0] = ProjCenterLatGeoKey;
        panProjParmId[1] = ProjCenterLongGeoKey;
        panProjParmId[2] = ProjAzimuthAngleGeoKey;
        panProjParmId[4] = ProjScaleAtCenterGeoKey;
        panProjParmId[5] = ProjFalseEastingGeoKey;
        panProjParmId[6] = ProjFalseNorthingGeoKey;

        panEPSGCodes[0] = EPSGProjCenterLat;
        panEPSGCodes[1] = EPSGProjCenterLong;
        panEPSGCodes[2] = EPSGAzimuth;
        panEPSGCodes[4] = EPSGInitialLineScaleFactor;
        panEPSGCodes[5] = EPSGProjCenterEasting;
        panEPSGCodes[6] = EPSGProjCenterNorthing;
        return TRUE;
        
      case CT_LambertConfConic_1SP:
      case CT_Mercator:
      case CT_ObliqueStereographic:
      case CT_PolarStereographic:
      case CT_TransverseMercator:
      case CT_TransvMercator_SouthOriented:
        panProjParmId[0] = ProjNatOriginLatGeoKey;
        panProjParmId[1] = ProjNatOriginLongGeoKey;
        panProjParmId[4] = ProjScaleAtNatOriginGeoKey;
        panProjParmId[5] = ProjFalseEastingGeoKey;
        panProjParmId[6] = ProjFalseNorthingGeoKey;

        panEPSGCodes[0] = EPSGNatOriginLat;
        panEPSGCodes[1] = EPSGNatOriginLong;
        panEPSGCodes[4] = EPSGNatOriginScaleFactor;
        panEPSGCodes[5] = EPSGFalseEasting;
        panEPSGCodes[6] = EPSGFalseNorthing;
        return TRUE;

      case CT_LambertConfConic_2SP:
        panProjParmId[0] = ProjFalseOriginLatGeoKey;
        panProjParmId[1] = ProjFalseOriginLongGeoKey;
        panProjParmId[2] = ProjStdParallel1GeoKey;
        panProjParmId[3] = ProjStdParallel2GeoKey;
        panProjParmId[5] = ProjFalseEastingGeoKey;
        panProjParmId[6] = ProjFalseNorthingGeoKey;

        panEPSGCodes[0] = EPSGFalseOriginLat;
        panEPSGCodes[1] = EPSGFalseOriginLong;
        panEPSGCodes[2] = EPSGStdParallel1Lat;
        panEPSGCodes[3] = EPSGStdParallel2Lat;
        panEPSGCodes[5] = EPSGFalseOriginEasting;
        panEPSGCodes[6] = EPSGFalseOriginNorthing;
        return TRUE;

      case CT_AlbersEqualArea:
        panProjParmId[0] = ProjStdParallel1GeoKey;
        panProjParmId[1] = ProjStdParallel2GeoKey;
        panProjParmId[2] = ProjNatOriginLatGeoKey;
        panProjParmId[3] = ProjNatOriginLongGeoKey;
        panProjParmId[5] = ProjFalseEastingGeoKey;
        panProjParmId[6] = ProjFalseNorthingGeoKey;

        panEPSGCodes[0] = EPSGStdParallel1Lat;
        panEPSGCodes[1] = EPSGStdParallel2Lat;
        panEPSGCodes[2] = EPSGFalseOriginLat;
        panEPSGCodes[3] = EPSGFalseOriginLong;
        panEPSGCodes[5] = EPSGFalseOriginEasting;
        panEPSGCodes[6] = EPSGFalseOriginNorthing;
        return TRUE;

      case CT_SwissObliqueCylindrical:
        panProjParmId[0] = ProjCenterLatGeoKey;
        panProjParmId[1] = ProjCenterLongGeoKey;
        panProjParmId[5] = ProjFalseEastingGeoKey;
        panProjParmId[6] = ProjFalseNorthingGeoKey;

        /* EPSG codes? */
        return TRUE;

      case CT_LambertAzimEqualArea:
        panProjParmId[0] = ProjCenterLatGeoKey;
        panProjParmId[1] = ProjCenterLongGeoKey;
        panProjParmId[5] = ProjFalseEastingGeoKey;
        panProjParmId[6] = ProjFalseNorthingGeoKey;

        panEPSGCodes[0] = EPSGNatOriginLat;
        panEPSGCodes[1] = EPSGNatOriginLong;
        panEPSGCodes[5] = EPSGFalseEasting;
        panEPSGCodes[6] = EPSGFalseNorthing;
        return TRUE;

      case CT_CylindricalEqualArea:
        panProjParmId[0] = ProjStdParallel1GeoKey;
        panProjParmId[1] = ProjNatOriginLongGeoKey;
        panProjParmId[5] = ProjFalseEastingGeoKey;
        panProjParmId[6] = ProjFalseNorthingGeoKey;

        panEPSGCodes[0] = EPSGStdParallel1Lat;
        panEPSGCodes[1] = EPSGFalseOriginLong;
        panEPSGCodes[5] = EPSGFalseOriginEasting;
        panEPSGCodes[6] = EPSGFalseOriginNorthing;
        return TRUE;

      default:
        return( FALSE );
    }
}

/************************************************************************/
/*                         GTIFGetProjTRFInfo()                         */
/*                                                                      */
/*      Transform a PROJECTION_TRF_CODE into a projection method,       */
/*      and a set of parameters.  The parameters identify will          */
/*      depend on the returned method, but they will all have been      */
/*      normalized into degrees and meters.                             */
/************************************************************************/

int GTIFGetProjTRFInfo( /* COORD_OP_CODE from coordinate_operation.csv */
                        int nProjTRFCode, 
                        char **ppszProjTRFName,
                        short * pnProjMethod,
                        double * padfProjParms )

{
    int		nProjMethod, i, anEPSGCodes[7];
    double	adfProjParms[7];
    char	szTRFCode[16];
    int         nCTProjMethod;
    char       *pszFilename;
    
    if ((nProjTRFCode >= Proj_UTM_zone_1N && nProjTRFCode <= Proj_UTM_zone_60N) ||
        (nProjTRFCode >= Proj_UTM_zone_1S && nProjTRFCode <= Proj_UTM_zone_60S))
    {
        int bNorth;
        int nZone;
        if (nProjTRFCode <= Proj_UTM_zone_60N)
        {
            bNorth = TRUE;
            nZone = nProjTRFCode - Proj_UTM_zone_1N + 1;
        }
        else
        {
            bNorth = FALSE;
            nZone = nProjTRFCode - Proj_UTM_zone_1S + 1;
        }

        if (ppszProjTRFName)
        {
            char szProjTRFName[64];
            sprintf(szProjTRFName, "UTM zone %d%c",
                    nZone, (bNorth) ? 'N' : 'S');
            *ppszProjTRFName = CPLStrdup(szProjTRFName);
        }

        if (pnProjMethod)
            *pnProjMethod = 9807;

        if (padfProjParms)
        {
            padfProjParms[0] = 0;
            padfProjParms[1] = -183 + 6 * nZone;
            padfProjParms[2] = 0;
            padfProjParms[3] = 0;
            padfProjParms[4] = 0.9996;
            padfProjParms[5] = 500000;
            padfProjParms[6] = (bNorth) ? 0 : 10000000;
        }

        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Get the proj method.  If this fails to return a meaningful      */
/*      number, then the whole function fails.                          */
/* -------------------------------------------------------------------- */
    pszFilename = CPLStrdup(CSVFilename("projop_wparm.csv"));
    sprintf( szTRFCode, "%d", nProjTRFCode );
    nProjMethod =
        atoi( CSVGetField( pszFilename,
                           "COORD_OP_CODE", szTRFCode, CC_Integer,
                           "COORD_OP_METHOD_CODE" ) );
    if( nProjMethod == 0 )
    {
        CPLFree( pszFilename );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Initialize a definition of what EPSG codes need to be loaded    */
/*      into what fields in adfProjParms.                               */
/* -------------------------------------------------------------------- */
    nCTProjMethod = EPSGProjMethodToCTProjMethod( nProjMethod );
    SetGTParmIds( nCTProjMethod, NULL, anEPSGCodes );

/* -------------------------------------------------------------------- */
/*      Get the parameters for this projection.  For the time being     */
/*      I am assuming the first four parameters are angles, the         */
/*      fifth is unitless (normally scale), and the remainder are       */
/*      linear measures.  This works fine for the existing              */
/*      projections, but is a pretty fragile approach.                  */
/* -------------------------------------------------------------------- */

    for( i = 0; i < 7; i++ )
    {
        char    szParamUOMID[32], szParamValueID[32], szParamCodeID[32];
        const char *pszValue;
        int     nUOM;
        int     nEPSGCode = anEPSGCodes[i];
        int     iEPSG;

        /* Establish default */
        if( nEPSGCode == EPSGAngleRectifiedToSkewedGrid )
            adfProjParms[i] = 90.0;
        else if( nEPSGCode == EPSGNatOriginScaleFactor
                 || nEPSGCode == EPSGInitialLineScaleFactor
                 || nEPSGCode == EPSGPseudoStdParallelScaleFactor )
            adfProjParms[i] = 1.0;
        else
            adfProjParms[i] = 0.0;

        /* If there is no parameter, skip */
        if( nEPSGCode == 0 )
            continue;

        /* Find the matching parameter */
        for( iEPSG = 0; iEPSG < 7; iEPSG++ )
        {
            sprintf( szParamCodeID, "PARAMETER_CODE_%d", iEPSG+1 );

            if( atoi(CSVGetField( pszFilename,
                                  "COORD_OP_CODE", szTRFCode, CC_Integer, 
                                  szParamCodeID )) == nEPSGCode )
                break;
        }

        /* not found, accept the default */
        if( iEPSG == 7 )
        {
            /* for CT_ObliqueMercator try alternate parameter codes first */
            /* because EPSG proj method 9812 uses EPSGFalseXXXXX, but 9815 uses EPSGProjCenterXXXXX */
            if ( nCTProjMethod == CT_ObliqueMercator && nEPSGCode == EPSGProjCenterEasting )
                nEPSGCode = EPSGFalseEasting;
            else if ( nCTProjMethod == CT_ObliqueMercator && nEPSGCode == EPSGProjCenterNorthing )
                nEPSGCode = EPSGFalseNorthing;
            else
                continue;
                
            for( iEPSG = 0; iEPSG < 7; iEPSG++ )
            {
                sprintf( szParamCodeID, "PARAMETER_CODE_%d", iEPSG+1 );

                if( atoi(CSVGetField( pszFilename,
                                      "COORD_OP_CODE", szTRFCode, CC_Integer, 
                                      szParamCodeID )) == nEPSGCode )
                    break;
            }
            
            if( iEPSG == 7 )
                continue;
        }

        /* Get the value, and UOM */
        sprintf( szParamUOMID, "PARAMETER_UOM_%d", iEPSG+1 );
        sprintf( szParamValueID, "PARAMETER_VALUE_%d", iEPSG+1 );

        nUOM = atoi(CSVGetField( pszFilename,
                                 "COORD_OP_CODE", szTRFCode, CC_Integer, 
                                 szParamUOMID ));
        pszValue = CSVGetField( pszFilename,
                                "COORD_OP_CODE", szTRFCode, CC_Integer, 
                                szParamValueID );

        /* Transform according to the UOM */
        if( nUOM >= 9100 && nUOM < 9200 )
            adfProjParms[i] = GTIFAngleStringToDD( pszValue, nUOM );
        else if( nUOM > 9000 && nUOM < 9100 )
        {
            double dfInMeters;

            if( !GTIFGetUOMLengthInfo( nUOM, NULL, &dfInMeters ) )
                dfInMeters = 1.0;
            adfProjParms[i] = GTIFAtof(pszValue) * dfInMeters;
        }
        else
            adfProjParms[i] = GTIFAtof(pszValue);
    }

/* -------------------------------------------------------------------- */
/*      Get the name, if requested.                                     */
/* -------------------------------------------------------------------- */
    if( ppszProjTRFName != NULL )
    {
        *ppszProjTRFName =
            CPLStrdup(CSVGetField( pszFilename,
                                   "COORD_OP_CODE", szTRFCode, CC_Integer,
                                   "COORD_OP_NAME" ));
    }
    
/* -------------------------------------------------------------------- */
/*      Transfer requested data into passed variables.                  */
/* -------------------------------------------------------------------- */
    if( pnProjMethod != NULL )
        *pnProjMethod = (short) nProjMethod;

    if( padfProjParms != NULL )
    {
        for( i = 0; i < 7; i++ )
            padfProjParms[i] = adfProjParms[i];
    }

    CPLFree( pszFilename );

    return TRUE;
}

/************************************************************************/
/*                         GTIFFetchProjParms()                         */
/*                                                                      */
/*      Fetch the projection parameters for a particular projection     */
/*      from a GeoTIFF file, and fill the GTIFDefn structure out        */
/*      with them.                                                      */
/************************************************************************/

static void GTIFFetchProjParms( GTIF * psGTIF, GTIFDefn * psDefn )

{
    double dfNatOriginLong = 0.0, dfNatOriginLat = 0.0, dfRectGridAngle = 0.0;
    double dfFalseEasting = 0.0, dfFalseNorthing = 0.0, dfNatOriginScale = 1.0;
    double dfStdParallel1 = 0.0, dfStdParallel2 = 0.0, dfAzimuth = 0.0;
    int iParm;

/* -------------------------------------------------------------------- */
/*      Get the false easting, and northing if available.               */
/* -------------------------------------------------------------------- */
    if( !GTIFKeyGet(psGTIF, ProjFalseEastingGeoKey, &dfFalseEasting, 0, 1)
        && !GTIFKeyGet(psGTIF, ProjCenterEastingGeoKey,
                       &dfFalseEasting, 0, 1) 
        && !GTIFKeyGet(psGTIF, ProjFalseOriginEastingGeoKey,
                       &dfFalseEasting, 0, 1) )
        dfFalseEasting = 0.0;
        
    if( !GTIFKeyGet(psGTIF, ProjFalseNorthingGeoKey, &dfFalseNorthing,0,1)
        && !GTIFKeyGet(psGTIF, ProjCenterNorthingGeoKey,
                       &dfFalseNorthing, 0, 1)
        && !GTIFKeyGet(psGTIF, ProjFalseOriginNorthingGeoKey,
                       &dfFalseNorthing, 0, 1) )
        dfFalseNorthing = 0.0;
        
    switch( psDefn->CTProjection )
    {
/* -------------------------------------------------------------------- */
      case CT_Stereographic:
/* -------------------------------------------------------------------- */
        if( GTIFKeyGet(psGTIF, ProjNatOriginLongGeoKey, 
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        if( GTIFKeyGet(psGTIF, ProjNatOriginLatGeoKey, 
                       &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0 )
            dfNatOriginLat = 0.0;

        if( GTIFKeyGet(psGTIF, ProjScaleAtNatOriginGeoKey,
                       &dfNatOriginScale, 0, 1 ) == 0 )
            dfNatOriginScale = 1.0;
            
        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjCenterLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjCenterLongGeoKey;
        psDefn->ProjParm[4] = dfNatOriginScale;
        psDefn->ProjParmId[4] = ProjScaleAtNatOriginGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_LambertConfConic_1SP:
      case CT_Mercator:
      case CT_ObliqueStereographic:
      case CT_TransverseMercator:
      case CT_TransvMercator_SouthOriented:
/* -------------------------------------------------------------------- */
        if( GTIFKeyGet(psGTIF, ProjNatOriginLongGeoKey, 
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        if( GTIFKeyGet(psGTIF, ProjNatOriginLatGeoKey, 
                       &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0 )
            dfNatOriginLat = 0.0;

        if( GTIFKeyGet(psGTIF, ProjScaleAtNatOriginGeoKey,
                       &dfNatOriginScale, 0, 1 ) == 0 )
            dfNatOriginScale = 1.0;
            
        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjNatOriginLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjNatOriginLongGeoKey;
        psDefn->ProjParm[4] = dfNatOriginScale;
        psDefn->ProjParmId[4] = ProjScaleAtNatOriginGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_ObliqueMercator: /* hotine */
      case CT_HotineObliqueMercatorAzimuthCenter: 
/* -------------------------------------------------------------------- */
        if( GTIFKeyGet(psGTIF, ProjNatOriginLongGeoKey, 
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        if( GTIFKeyGet(psGTIF, ProjNatOriginLatGeoKey, 
                       &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0 )
            dfNatOriginLat = 0.0;

        if( GTIFKeyGet(psGTIF, ProjAzimuthAngleGeoKey, 
                       &dfAzimuth, 0, 1 ) == 0 )
            dfAzimuth = 0.0;

        if( GTIFKeyGet(psGTIF, ProjRectifiedGridAngleGeoKey,
                       &dfRectGridAngle, 0, 1 ) == 0 )
            dfRectGridAngle = 90.0;

        if( GTIFKeyGet(psGTIF, ProjScaleAtNatOriginGeoKey,
                       &dfNatOriginScale, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjScaleAtCenterGeoKey,
                          &dfNatOriginScale, 0, 1 ) == 0 )
            dfNatOriginScale = 1.0;
            
        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjCenterLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjCenterLongGeoKey;
        psDefn->ProjParm[2] = dfAzimuth;
        psDefn->ProjParmId[2] = ProjAzimuthAngleGeoKey;
        psDefn->ProjParm[3] = dfRectGridAngle;
        psDefn->ProjParmId[3] = ProjRectifiedGridAngleGeoKey;
        psDefn->ProjParm[4] = dfNatOriginScale;
        psDefn->ProjParmId[4] = ProjScaleAtCenterGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_CassiniSoldner:
      case CT_Polyconic:
/* -------------------------------------------------------------------- */
        if( GTIFKeyGet(psGTIF, ProjNatOriginLongGeoKey, 
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        if( GTIFKeyGet(psGTIF, ProjNatOriginLatGeoKey, 
                       &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0 )
            dfNatOriginLat = 0.0;

        if( GTIFKeyGet(psGTIF, ProjScaleAtNatOriginGeoKey,
                       &dfNatOriginScale, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjScaleAtCenterGeoKey,
                          &dfNatOriginScale, 0, 1 ) == 0 )
            dfNatOriginScale = 1.0;
            
        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjNatOriginLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjNatOriginLongGeoKey;
        psDefn->ProjParm[4] = dfNatOriginScale;
        psDefn->ProjParmId[4] = ProjScaleAtNatOriginGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_AzimuthalEquidistant:
      case CT_MillerCylindrical:
      case CT_Gnomonic:
      case CT_LambertAzimEqualArea:
      case CT_Orthographic:
      case CT_NewZealandMapGrid:
/* -------------------------------------------------------------------- */
        if( GTIFKeyGet(psGTIF, ProjNatOriginLongGeoKey, 
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        if( GTIFKeyGet(psGTIF, ProjNatOriginLatGeoKey, 
                       &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0 )
            dfNatOriginLat = 0.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjCenterLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjCenterLongGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_Equirectangular:
/* -------------------------------------------------------------------- */
        if( GTIFKeyGet(psGTIF, ProjNatOriginLongGeoKey, 
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        if( GTIFKeyGet(psGTIF, ProjNatOriginLatGeoKey, 
                       &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0 )
            dfNatOriginLat = 0.0;

        if( GTIFKeyGet(psGTIF, ProjStdParallel1GeoKey, 
                       &dfStdParallel1, 0, 1 ) == 0 )
            dfStdParallel1 = 0.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjCenterLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjCenterLongGeoKey;
        psDefn->ProjParm[2] = dfStdParallel1;
        psDefn->ProjParmId[2] = ProjStdParallel1GeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_Robinson:
      case CT_Sinusoidal:
      case CT_VanDerGrinten:
/* -------------------------------------------------------------------- */
        if( GTIFKeyGet(psGTIF, ProjNatOriginLongGeoKey, 
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjCenterLongGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_PolarStereographic:
/* -------------------------------------------------------------------- */
        if( GTIFKeyGet(psGTIF, ProjStraightVertPoleLongGeoKey, 
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjNatOriginLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        if( GTIFKeyGet(psGTIF, ProjNatOriginLatGeoKey, 
                       &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0 )
            dfNatOriginLat = 0.0;

        if( GTIFKeyGet(psGTIF, ProjScaleAtNatOriginGeoKey,
                       &dfNatOriginScale, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjScaleAtCenterGeoKey,
                          &dfNatOriginScale, 0, 1 ) == 0 )
            dfNatOriginScale = 1.0;
            
        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjNatOriginLatGeoKey;;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjStraightVertPoleLongGeoKey;
        psDefn->ProjParm[4] = dfNatOriginScale;
        psDefn->ProjParmId[4] = ProjScaleAtNatOriginGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_LambertConfConic_2SP:
/* -------------------------------------------------------------------- */
        if( GTIFKeyGet(psGTIF, ProjStdParallel1GeoKey, 
                       &dfStdParallel1, 0, 1 ) == 0 )
            dfStdParallel1 = 0.0;

        if( GTIFKeyGet(psGTIF, ProjStdParallel2GeoKey, 
                       &dfStdParallel2, 0, 1 ) == 0 )
            dfStdParallel1 = 0.0;

        if( GTIFKeyGet(psGTIF, ProjNatOriginLongGeoKey, 
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        if( GTIFKeyGet(psGTIF, ProjNatOriginLatGeoKey, 
                       &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0 )
            dfNatOriginLat = 0.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjFalseOriginLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjFalseOriginLongGeoKey;
        psDefn->ProjParm[2] = dfStdParallel1;
        psDefn->ProjParmId[2] = ProjStdParallel1GeoKey;
        psDefn->ProjParm[3] = dfStdParallel2;
        psDefn->ProjParmId[3] = ProjStdParallel2GeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_AlbersEqualArea:
      case CT_EquidistantConic:
/* -------------------------------------------------------------------- */
        if( GTIFKeyGet(psGTIF, ProjStdParallel1GeoKey, 
                       &dfStdParallel1, 0, 1 ) == 0 )
            dfStdParallel1 = 0.0;

        if( GTIFKeyGet(psGTIF, ProjStdParallel2GeoKey, 
                       &dfStdParallel2, 0, 1 ) == 0 )
            dfStdParallel2 = 0.0;

        if( GTIFKeyGet(psGTIF, ProjNatOriginLongGeoKey, 
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        if( GTIFKeyGet(psGTIF, ProjNatOriginLatGeoKey, 
                       &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLatGeoKey, 
                          &dfNatOriginLat, 0, 1 ) == 0 )
            dfNatOriginLat = 0.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfStdParallel1;
        psDefn->ProjParmId[0] = ProjStdParallel1GeoKey;
        psDefn->ProjParm[1] = dfStdParallel2;
        psDefn->ProjParmId[1] = ProjStdParallel2GeoKey;
        psDefn->ProjParm[2] = dfNatOriginLat;
        psDefn->ProjParmId[2] = ProjNatOriginLatGeoKey;
        psDefn->ProjParm[3] = dfNatOriginLong;
        psDefn->ProjParmId[3] = ProjNatOriginLongGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_CylindricalEqualArea:
/* -------------------------------------------------------------------- */
        if( GTIFKeyGet(psGTIF, ProjStdParallel1GeoKey, 
                       &dfStdParallel1, 0, 1 ) == 0 )
            dfStdParallel1 = 0.0;

        if( GTIFKeyGet(psGTIF, ProjNatOriginLongGeoKey, 
                       &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjFalseOriginLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0
            && GTIFKeyGet(psGTIF, ProjCenterLongGeoKey, 
                          &dfNatOriginLong, 0, 1 ) == 0 )
            dfNatOriginLong = 0.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfStdParallel1;
        psDefn->ProjParmId[0] = ProjStdParallel1GeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjNatOriginLongGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;
    }

/* -------------------------------------------------------------------- */
/*      Normalize any linear parameters into meters.  In GeoTIFF        */
/*      the linear projection parameter tags are normally in the        */
/*      units of the coordinate system described.                       */
/* -------------------------------------------------------------------- */
    for( iParm = 0; iParm < psDefn->nParms; iParm++ )
    {
        switch( psDefn->ProjParmId[iParm] )
        {
          case ProjFalseEastingGeoKey:
          case ProjFalseNorthingGeoKey:
          case ProjFalseOriginEastingGeoKey:
          case ProjFalseOriginNorthingGeoKey:
          case ProjCenterEastingGeoKey:
          case ProjCenterNorthingGeoKey:
            if( psDefn->UOMLengthInMeters != 0 
                && psDefn->UOMLengthInMeters != 1.0 )
            {
                psDefn->ProjParm[iParm] *= psDefn->UOMLengthInMeters;
            }
            break;

          default:
            break;
        }
    }
}

/************************************************************************/
/*                            GTIFGetDefn()                             */
/************************************************************************/

/**
@param psGTIF GeoTIFF information handle as returned by GTIFNew.
@param psDefn Pointer to an existing GTIFDefn structure.  This structure
does not need to have been pre-initialized at all.

@return TRUE if the function has been successful, otherwise FALSE.

This function reads the coordinate system definition from a GeoTIFF file,
and <i>normalizes</i> it into a set of component information using 
definitions from CSV (Comma Seperated Value ASCII) files derived from 
EPSG tables.  This function is intended to simplify correct support for
reading files with defined PCS (Projected Coordinate System) codes that
wouldn't otherwise be directly known by application software by reducing
it to the underlying projection method, parameters, datum, ellipsoid, 
prime meridian and units.<p>

The application should pass a pointer to an existing uninitialized 
GTIFDefn structure, and GTIFGetDefn() will fill it in.  The fuction 
currently always returns TRUE but in the future will return FALSE if 
CSV files are not found.  In any event, all geokeys actually found in the
file will be copied into the GTIFDefn.  However, if the CSV files aren't
found codes implied by other codes will not be set properly.<p>

GTIFGetDefn() will not generally work if the EPSG derived CSV files cannot
be found.  By default a modest attempt will be made to find them, but 
in general it is necessary for the calling application to override the
logic to find them.  This can be done by calling the 
SetCSVFilenameHook() function to
override the search method based on application knowledge of where they are
found.<p>

The normalization methodology operates by fetching tags from the GeoTIFF
file, and then setting all other tags implied by them in the structure.  The
implied relationships are worked out by reading definitions from the 
various EPSG derived CSV tables.<p>

For instance, if a PCS (ProjectedCSTypeGeoKey) is found in the GeoTIFF file
this code is used to lookup a record in the <tt>horiz_cs.csv</tt> CSV
file.  For example given the PCS 26746 we can find the name
(NAD27 / California zone VI), the GCS 4257 (NAD27), and the ProjectionCode
10406 (California CS27 zone VI).  The GCS, and ProjectionCode can in turn
be looked up in other tables until all the details of units, ellipsoid, 
prime meridian, datum, projection (LambertConfConic_2SP) and projection
parameters are established.  A full listgeo dump of a file 
for this result might look like the following, all based on a single PCS
value:<p>

<pre>
% listgeo -norm ~/data/geotiff/pci_eg/spaf27.tif
Geotiff_Information:
   Version: 1
   Key_Revision: 1.0
   Tagged_Information:
      ModelTiepointTag (2,3):
         0                0                0                
         1577139.71       634349.176       0                
      ModelPixelScaleTag (1,3):
         195.509321       198.32184        0                
      End_Of_Tags.
   Keyed_Information:
      GTModelTypeGeoKey (Short,1): ModelTypeProjected
      GTRasterTypeGeoKey (Short,1): RasterPixelIsArea
      ProjectedCSTypeGeoKey (Short,1): PCS_NAD27_California_VI
      End_Of_Keys.
   End_Of_Geotiff.

PCS = 26746 (NAD27 / California zone VI)
Projection = 10406 (California CS27 zone VI)
Projection Method: CT_LambertConfConic_2SP
   ProjStdParallel1GeoKey: 33.883333
   ProjStdParallel2GeoKey: 32.766667
   ProjFalseOriginLatGeoKey: 32.166667
   ProjFalseOriginLongGeoKey: -116.233333
   ProjFalseEastingGeoKey: 609601.219202
   ProjFalseNorthingGeoKey: 0.000000
GCS: 4267/NAD27
Datum: 6267/North American Datum 1927
Ellipsoid: 7008/Clarke 1866 (6378206.40,6356583.80)
Prime Meridian: 8901/Greenwich (0.000000)
Projection Linear Units: 9003/US survey foot (0.304801m)
</pre>

Note that GTIFGetDefn() does not inspect or return the tiepoints and scale.
This must be handled seperately as it normally would.  It is intended to
simplify capture and normalization of the coordinate system definition.  
Note that GTIFGetDefn() also does the following things:

<ol>
<li> Convert all angular values to decimal degrees.
<li> Convert all linear values to meters. 
<li> Return the linear units and conversion to meters for the tiepoints and
scale (though the tiepoints and scale remain in their native units). 
<li> When reading projection parameters a variety of differences between
different GeoTIFF generators are handled, and a normalized set of parameters
for each projection are always returned.
</ol>

Code fields in the GTIFDefn are filled with KvUserDefined if there is not
value to assign.  The parameter lists for each of the underlying projection
transform methods can be found at the
<a href="http://www.remotesensing.org/geotiff/proj_list">Projections</a>
page.  Note that nParms will be set based on the maximum parameter used.
Some of the parameters may not be used in which case the
GTIFDefn::ProjParmId[] will
be zero.  This is done to retain correspondence to the EPSG parameter
numbering scheme.<p>

The 
<a href="http://www.remotesensing.org/cgi-bin/cvsweb.cgi/~checkout~/osrs/geotiff/libgeotiff/geotiff_proj4.c">geotiff_proj4.c</a> module distributed with libgeotiff can 
be used as an example of code that converts a GTIFDefn into another projection
system.<p>

@see GTIFKeySet(), SetCSVFilenameHook()

*/

int GTIFGetDefn( GTIF * psGTIF, GTIFDefn * psDefn )

{
    int		i;
    short	nGeogUOMLinear;
    double	dfInvFlattening;
    
/* -------------------------------------------------------------------- */
/*      Initially we default all the information we can.                */
/* -------------------------------------------------------------------- */
    psDefn->DefnSet = 1;
    psDefn->Model = KvUserDefined;
    psDefn->PCS = KvUserDefined;
    psDefn->GCS = KvUserDefined;
    psDefn->UOMLength = KvUserDefined;
    psDefn->UOMLengthInMeters = 1.0;
    psDefn->UOMAngle = KvUserDefined;
    psDefn->UOMAngleInDegrees = 1.0;
    psDefn->Datum = KvUserDefined;
    psDefn->Ellipsoid = KvUserDefined;
    psDefn->SemiMajor = 0.0;
    psDefn->SemiMinor = 0.0;
    psDefn->PM = KvUserDefined;
    psDefn->PMLongToGreenwich = 0.0;
    psDefn->TOWGS84Count = 0;
    memset( psDefn->TOWGS84, 0, sizeof(psDefn->TOWGS84) );

    psDefn->ProjCode = KvUserDefined;
    psDefn->Projection = KvUserDefined;
    psDefn->CTProjection = KvUserDefined;

    psDefn->nParms = 0;
    for( i = 0; i < MAX_GTIF_PROJPARMS; i++ )
    {
        psDefn->ProjParm[i] = 0.0;
        psDefn->ProjParmId[i] = 0;
    }

    psDefn->MapSys = KvUserDefined;
    psDefn->Zone = 0;

/* -------------------------------------------------------------------- */
/*      Do we have any geokeys?                                         */
/* -------------------------------------------------------------------- */
    {
        int     nKeyCount = 0;
        int     anVersion[3];
        GTIFDirectoryInfo( psGTIF, anVersion, &nKeyCount );

        if( nKeyCount == 0 )
        {
            psDefn->DefnSet = 0;
            return FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*	Try to get the overall model type.				*/
/* -------------------------------------------------------------------- */
    GTIFKeyGet(psGTIF,GTModelTypeGeoKey,&(psDefn->Model),0,1);

/* -------------------------------------------------------------------- */
/*	Extract the Geog units.  					*/
/* -------------------------------------------------------------------- */
    nGeogUOMLinear = 9001; /* Linear_Meter */
    GTIFKeyGet(psGTIF, GeogLinearUnitsGeoKey, &nGeogUOMLinear, 0, 1 );

/* -------------------------------------------------------------------- */
/*      Try to get a PCS.                                               */
/* -------------------------------------------------------------------- */
    if( GTIFKeyGet(psGTIF,ProjectedCSTypeGeoKey, &(psDefn->PCS),0,1) == 1
        && psDefn->PCS != KvUserDefined )
    {
        /*
         * Translate this into useful information.
         */
        GTIFGetPCSInfo( psDefn->PCS, NULL, &(psDefn->ProjCode),
                        &(psDefn->UOMLength), &(psDefn->GCS) );
    }

/* -------------------------------------------------------------------- */
/*       If we have the PCS code, but didn't find it in the CSV files   */
/*      (likely because we can't find them) we will try some ``jiffy    */
/*      rules'' for UTM and state plane.                                */
/* -------------------------------------------------------------------- */
    if( psDefn->PCS != KvUserDefined && psDefn->ProjCode == KvUserDefined )
    {
        int	nMapSys, nZone;
        int	nGCS = psDefn->GCS;

        nMapSys = GTIFPCSToMapSys( psDefn->PCS, &nGCS, &nZone );
        if( nMapSys != KvUserDefined )
        {
            psDefn->ProjCode = (short) GTIFMapSysToProj( nMapSys, nZone );
            psDefn->GCS = (short) nGCS;
        }
    }

/* -------------------------------------------------------------------- */
/*      If the Proj_ code is specified directly, use that.              */
/* -------------------------------------------------------------------- */
    if( psDefn->ProjCode == KvUserDefined )
        GTIFKeyGet(psGTIF, ProjectionGeoKey, &(psDefn->ProjCode), 0, 1 );
    
    if( psDefn->ProjCode != KvUserDefined )
    {
        /*
         * We have an underlying projection transformation value.  Look
         * this up.  For a PCS of ``WGS 84 / UTM 11'' the transformation
         * would be Transverse Mercator, with a particular set of options.
         * The nProjTRFCode itself would correspond to the name
         * ``UTM zone 11N'', and doesn't include datum info.
         */
        GTIFGetProjTRFInfo( psDefn->ProjCode, NULL, &(psDefn->Projection),
                            psDefn->ProjParm );
        
        /*
         * Set the GeoTIFF identity of the parameters.
         */
        psDefn->CTProjection = (short) 
            EPSGProjMethodToCTProjMethod( psDefn->Projection );

        SetGTParmIds( psDefn->CTProjection, psDefn->ProjParmId, NULL);
        psDefn->nParms = 7;
    }

/* -------------------------------------------------------------------- */
/*      Try to get a GCS.  If found, it will override any implied by    */
/*      the PCS.                                                        */
/* -------------------------------------------------------------------- */
    GTIFKeyGet(psGTIF, GeographicTypeGeoKey, &(psDefn->GCS), 0, 1 );
    if( psDefn->GCS < 1 || psDefn->GCS >= KvUserDefined )
        psDefn->GCS = KvUserDefined;

/* -------------------------------------------------------------------- */
/*      Derive the datum, and prime meridian from the GCS.              */
/* -------------------------------------------------------------------- */
    if( psDefn->GCS != KvUserDefined )
    {
        GTIFGetGCSInfo( psDefn->GCS, NULL, &(psDefn->Datum), &(psDefn->PM),
                        &(psDefn->UOMAngle) );
    }
    
/* -------------------------------------------------------------------- */
/*      Handle the GCS angular units.  GeogAngularUnitsGeoKey           */
/*      overrides the GCS or PCS setting.                               */
/* -------------------------------------------------------------------- */
    GTIFKeyGet(psGTIF, GeogAngularUnitsGeoKey, &(psDefn->UOMAngle), 0, 1 );
    if( psDefn->UOMAngle != KvUserDefined )
    {
        GTIFGetUOMAngleInfo( psDefn->UOMAngle, NULL,
                             &(psDefn->UOMAngleInDegrees) );
    }

/* -------------------------------------------------------------------- */
/*      Check for a datum setting, and then use the datum to derive     */
/*      an ellipsoid.                                                   */
/* -------------------------------------------------------------------- */
    GTIFKeyGet(psGTIF, GeogGeodeticDatumGeoKey, &(psDefn->Datum), 0, 1 );

    if( psDefn->Datum != KvUserDefined )
    {
        GTIFGetDatumInfo( psDefn->Datum, NULL, &(psDefn->Ellipsoid) );
    }

/* -------------------------------------------------------------------- */
/*      Check for an explicit ellipsoid.  Use the ellipsoid to          */
/*      derive the ellipsoid characteristics, if possible.              */
/* -------------------------------------------------------------------- */
    GTIFKeyGet(psGTIF, GeogEllipsoidGeoKey, &(psDefn->Ellipsoid), 0, 1 );

    if( psDefn->Ellipsoid != KvUserDefined )
    {
        GTIFGetEllipsoidInfo( psDefn->Ellipsoid, NULL,
                              &(psDefn->SemiMajor), &(psDefn->SemiMinor) );
    }

/* -------------------------------------------------------------------- */
/*      Check for overridden ellipsoid parameters.  It would be nice    */
/*      to warn if they conflict with provided information, but for     */
/*      now we just override.                                           */
/* -------------------------------------------------------------------- */
    GTIFKeyGet(psGTIF, GeogSemiMajorAxisGeoKey, &(psDefn->SemiMajor), 0, 1 );
    GTIFKeyGet(psGTIF, GeogSemiMinorAxisGeoKey, &(psDefn->SemiMinor), 0, 1 );
    
    if( GTIFKeyGet(psGTIF, GeogInvFlatteningGeoKey, &dfInvFlattening, 
                   0, 1 ) == 1 )
    {
        if( dfInvFlattening != 0.0 )
            psDefn->SemiMinor = 
                psDefn->SemiMajor * (1 - 1.0/dfInvFlattening);
        else
            psDefn->SemiMinor = psDefn->SemiMajor;
    }
    
/* -------------------------------------------------------------------- */
/*      Get the prime meridian info.                                    */
/* -------------------------------------------------------------------- */
    GTIFKeyGet(psGTIF, GeogPrimeMeridianGeoKey, &(psDefn->PM), 0, 1 );

    if( psDefn->PM != KvUserDefined )
    {
        GTIFGetPMInfo( psDefn->PM, NULL, &(psDefn->PMLongToGreenwich) );
    }
    else
    {
        GTIFKeyGet(psGTIF, GeogPrimeMeridianLongGeoKey,
                   &(psDefn->PMLongToGreenwich), 0, 1 );

        psDefn->PMLongToGreenwich =
            GTIFAngleToDD( psDefn->PMLongToGreenwich,
                           psDefn->UOMAngle );
    }

/* -------------------------------------------------------------------- */
/*      Get the TOWGS84 parameters.                                     */
/* -------------------------------------------------------------------- */
    psDefn->TOWGS84Count = 
        GTIFKeyGet(psGTIF, GeogTOWGS84GeoKey, &(psDefn->TOWGS84), 0, 7 );

/* -------------------------------------------------------------------- */
/*      Have the projection units of measure been overridden?  We       */
/*      should likely be doing something about angular units too,       */
/*      but these are very rarely not decimal degrees for actual        */
/*      file coordinates.                                               */
/* -------------------------------------------------------------------- */
    GTIFKeyGet(psGTIF,ProjLinearUnitsGeoKey,&(psDefn->UOMLength),0,1);

    if( psDefn->UOMLength != KvUserDefined )
    {
        GTIFGetUOMLengthInfo( psDefn->UOMLength, NULL,
                              &(psDefn->UOMLengthInMeters) );
    }
    else
    {
        GTIFKeyGet(psGTIF,ProjLinearUnitSizeGeoKey,&(psDefn->UOMLengthInMeters),0,1);
    }

/* -------------------------------------------------------------------- */
/*      Handle a variety of user defined transform types.               */
/* -------------------------------------------------------------------- */
    if( GTIFKeyGet(psGTIF,ProjCoordTransGeoKey,
                   &(psDefn->CTProjection),0,1) == 1)
    {
        GTIFFetchProjParms( psGTIF, psDefn );
    }

/* -------------------------------------------------------------------- */
/*      Try to set the zoned map system information.                    */
/* -------------------------------------------------------------------- */
    psDefn->MapSys = GTIFProjToMapSys( psDefn->ProjCode, &(psDefn->Zone) );

/* -------------------------------------------------------------------- */
/*      If this is UTM, and we were unable to extract the projection    */
/*      parameters from the CSV file, just set them directly now,       */
/*      since it's pretty easy, and a common case.                      */
/* -------------------------------------------------------------------- */
    if( (psDefn->MapSys == MapSys_UTM_North
         || psDefn->MapSys == MapSys_UTM_South)
        && psDefn->CTProjection == KvUserDefined )
    {
        psDefn->CTProjection = CT_TransverseMercator;
        psDefn->nParms = 7;
        psDefn->ProjParmId[0] = ProjNatOriginLatGeoKey;
        psDefn->ProjParm[0] = 0.0;
            
        psDefn->ProjParmId[1] = ProjNatOriginLongGeoKey;
        psDefn->ProjParm[1] = psDefn->Zone*6 - 183.0;
        
        psDefn->ProjParmId[4] = ProjScaleAtNatOriginGeoKey;
        psDefn->ProjParm[4] = 0.9996;
        
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[5] = 500000.0;
        
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        if( psDefn->MapSys == MapSys_UTM_North )
            psDefn->ProjParm[6] = 0.0;
        else
            psDefn->ProjParm[6] = 10000000.0;
    }

    return TRUE;
}

/************************************************************************/
/*                            GTIFDecToDMS()                            */
/*                                                                      */
/*      Convenient function to translate decimal degrees to DMS         */
/*      format for reporting to a user.                                 */
/************************************************************************/

const char *GTIFDecToDMS( double dfAngle, const char * pszAxis,
                          int nPrecision )

{
    int		nDegrees, nMinutes;
    double	dfSeconds;
    char	szFormat[30];
    static char szBuffer[50];
    const char	*pszHemisphere = NULL;
    double	dfRound;
    int		i;

    dfRound = 0.5/60;
    for( i = 0; i < nPrecision; i++ )
        dfRound = dfRound * 0.1;

    nDegrees = (int) ABS(dfAngle);
    nMinutes = (int) ((ABS(dfAngle) - nDegrees) * 60 + dfRound);
    dfSeconds = ABS((ABS(dfAngle) * 3600 - nDegrees*3600 - nMinutes*60));

    if( EQUAL(pszAxis,"Long") && dfAngle < 0.0 )
        pszHemisphere = "W";
    else if( EQUAL(pszAxis,"Long") )
        pszHemisphere = "E";
    else if( dfAngle < 0.0 )
        pszHemisphere = "S";
    else
        pszHemisphere = "N";

    sprintf( szFormat, "%%3dd%%2d\'%%%d.%df\"%s",
             nPrecision+3, nPrecision, pszHemisphere );
    sprintf( szBuffer, szFormat, nDegrees, nMinutes, dfSeconds );

    return( szBuffer );
}

/************************************************************************/
/*                           GTIFPrintDefn()                            */
/*                                                                      */
/*      Report the contents of a GTIFDefn structure ... mostly for      */
/*      debugging.                                                      */
/************************************************************************/

void GTIFPrintDefn( GTIFDefn * psDefn, FILE * fp )

{
/* -------------------------------------------------------------------- */
/*      Do we have anything to report?                                  */
/* -------------------------------------------------------------------- */
    if( !psDefn->DefnSet )
    {
        fprintf( fp, "No GeoKeys found.\n" );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Get the PCS name if possible.                                   */
/* -------------------------------------------------------------------- */
    if( psDefn->PCS != KvUserDefined )
    {
        char	*pszPCSName = NULL;
    
        GTIFGetPCSInfo( psDefn->PCS, &pszPCSName, NULL, NULL, NULL );
        if( pszPCSName == NULL )
            pszPCSName = CPLStrdup("name unknown");
        
        fprintf( fp, "PCS = %d (%s)\n", psDefn->PCS, pszPCSName );
        CPLFree( pszPCSName );
    }

/* -------------------------------------------------------------------- */
/*	Dump the projection code if possible.				*/
/* -------------------------------------------------------------------- */
    if( psDefn->ProjCode != KvUserDefined )
    {
        char	*pszTRFName = NULL;

        GTIFGetProjTRFInfo( psDefn->ProjCode, &pszTRFName, NULL, NULL );
        if( pszTRFName == NULL )
            pszTRFName = CPLStrdup("");
                
        fprintf( fp, "Projection = %d (%s)\n",
                 psDefn->ProjCode, pszTRFName );

        CPLFree( pszTRFName );
    }

/* -------------------------------------------------------------------- */
/*      Try to dump the projection method name, and parameters if possible.*/
/* -------------------------------------------------------------------- */
    if( psDefn->CTProjection != KvUserDefined )
    {
        char	*pszName = GTIFValueName(ProjCoordTransGeoKey,
                                         psDefn->CTProjection);
        int     i;

        if( pszName == NULL )
            pszName = "(unknown)";
            
        fprintf( fp, "Projection Method: %s\n", pszName );

        for( i = 0; i < psDefn->nParms; i++ )
        {
            if( psDefn->ProjParmId[i] == 0 )
                continue;

            pszName = GTIFKeyName((geokey_t) psDefn->ProjParmId[i]);
            if( pszName == NULL )
                pszName = "(unknown)";

            if( i < 4 )
            {
                char	*pszAxisName;
                
                if( strstr(pszName,"Long") != NULL )
                    pszAxisName = "Long";
                else if( strstr(pszName,"Lat") != NULL )
                    pszAxisName = "Lat";
                else
                    pszAxisName = "?";
                
                fprintf( fp, "   %s: %f (%s)\n",
                         pszName, psDefn->ProjParm[i],
                         GTIFDecToDMS( psDefn->ProjParm[i], pszAxisName, 2 ) );
            }
            else if( i == 4 )
                fprintf( fp, "   %s: %f\n", pszName, psDefn->ProjParm[i] );
            else
                fprintf( fp, "   %s: %f m\n", pszName, psDefn->ProjParm[i] );
        }
    }

/* -------------------------------------------------------------------- */
/*      Report the GCS name, and number.                                */
/* -------------------------------------------------------------------- */
    if( psDefn->GCS != KvUserDefined )
    {
        char	*pszName = NULL;

        GTIFGetGCSInfo( psDefn->GCS, &pszName, NULL, NULL, NULL );
        if( pszName == NULL )
            pszName = CPLStrdup("(unknown)");
        
        fprintf( fp, "GCS: %d/%s\n", psDefn->GCS, pszName );
        CPLFree( pszName );
    }

/* -------------------------------------------------------------------- */
/*      Report the datum name.                                          */
/* -------------------------------------------------------------------- */
    if( psDefn->Datum != KvUserDefined )
    {
        char	*pszName = NULL;

        GTIFGetDatumInfo( psDefn->Datum, &pszName, NULL );
        if( pszName == NULL )
            pszName = CPLStrdup("(unknown)");
        
        fprintf( fp, "Datum: %d/%s\n", psDefn->Datum, pszName );
        CPLFree( pszName );
    }

/* -------------------------------------------------------------------- */
/*      Report the ellipsoid.                                           */
/* -------------------------------------------------------------------- */
    if( psDefn->Ellipsoid != KvUserDefined )
    {
        char	*pszName = NULL;

        GTIFGetEllipsoidInfo( psDefn->Ellipsoid, &pszName, NULL, NULL );
        if( pszName == NULL )
            pszName = CPLStrdup("(unknown)");
        
        fprintf( fp, "Ellipsoid: %d/%s (%.2f,%.2f)\n",
                 psDefn->Ellipsoid, pszName,
                 psDefn->SemiMajor, psDefn->SemiMinor );
        CPLFree( pszName );
    }
    
/* -------------------------------------------------------------------- */
/*      Report the prime meridian.                                      */
/* -------------------------------------------------------------------- */
    if( psDefn->PM != KvUserDefined )
    {
        char	*pszName = NULL;

        GTIFGetPMInfo( psDefn->PM, &pszName, NULL );

        if( pszName == NULL )
            pszName = CPLStrdup("(unknown)");
        
        fprintf( fp, "Prime Meridian: %d/%s (%f/%s)\n",
                 psDefn->PM, pszName,
                 psDefn->PMLongToGreenwich,
                 GTIFDecToDMS( psDefn->PMLongToGreenwich, "Long", 2 ) );
        CPLFree( pszName );
    }

/* -------------------------------------------------------------------- */
/*      Report TOWGS84 parameters.                                      */
/* -------------------------------------------------------------------- */
    if( psDefn->TOWGS84Count > 0 )
    {
        int i;

        fprintf( fp, "TOWGS84: " );
        
        for( i = 0; i < psDefn->TOWGS84Count; i++ )
        {
            if( i > 0 )
                fprintf( fp, "," );
            fprintf( fp, "%g", psDefn->TOWGS84[i] );
        }

        fprintf( fp, "\n" );
    }

/* -------------------------------------------------------------------- */
/*      Report the projection units of measure (currently just          */
/*      linear).                                                        */
/* -------------------------------------------------------------------- */
    if( psDefn->UOMLength != KvUserDefined )
    {
        char	*pszName = NULL;

        GTIFGetUOMLengthInfo( psDefn->UOMLength, &pszName, NULL );
        if( pszName == NULL )
            pszName = CPLStrdup( "(unknown)" );
        
        fprintf( fp, "Projection Linear Units: %d/%s (%fm)\n",
                 psDefn->UOMLength, pszName, psDefn->UOMLengthInMeters );
        CPLFree( pszName );
    }
    else
    {
        fprintf( fp, "Projection Linear Units: User-Defined (%fm)\n",
                 psDefn->UOMLengthInMeters );
    }
}

/************************************************************************/
/*                           GTIFFreeMemory()                           */
/*                                                                      */
/*      Externally visible function to free memory allocated within     */
/*      geo_normalize.c.                                                */
/************************************************************************/

void GTIFFreeMemory( char * pMemory )

{
    if( pMemory != NULL )
        VSIFree( pMemory );
}

/************************************************************************/
/*                          GTIFDeaccessCSV()                           */
/*                                                                      */
/*      Free all cached CSV info.                                       */
/************************************************************************/

void GTIFDeaccessCSV()

{
    CSVDeaccess( NULL );
}

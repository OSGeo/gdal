/******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
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
 * gt_gs.c: GeoTIFF to Geosoft GXF.
 *
 * $Log$
 * Revision 1.1  1999/01/05 16:51:55  warmerda
 * New
 *
 */

#include "cpl_csv.h"

#include "geotiff.h"   /* public interface        */
#include "geo_tiffp.h" /* external TIFF interface */
#include "geo_keyp.h"  /* private interface       */
#include "geovalues.h"

#ifdef GEOSOFT
#include <sys.h>
#include <geosoft.h>
#include <gs.h>
#endif


#ifndef GEOSOFT 
/************************************************************************/
/*                            CSVFilename()                             */
/*                                                                      */
/*      Return the full path to a particular CSV file.                  */
/************************************************************************/

static const char * CSVFilename( const char *pszBasename )

{
    static char		szPath[512];

    sprintf( szPath, "/home/warmerda/geosoft/newcsv/%s", pszBasename );

    return( szPath );
}
#endif

/************************************************************************/
/*                             AngleToDD()                              */
/*                                                                      */
/*      Convert an angle in the specified units to decimal degrees.     */
/************************************************************************/

static double AngleToDD( const char * pszAngle, const char * pszUnits )

{
    double	dfAngle;
    
    if( EQUAL(pszUnits,"DDD.MMSSsss") )
    {
        char	*pszDecimal;
        
        dfAngle = ABS(atoi(pszAngle));
        pszDecimal = strchr(pszAngle,'.');
        if( pszDecimal != NULL )
        {
            dfAngle += ((int) (atof(pszDecimal)*100)) / 60.0;
            if( strlen(pszDecimal) > 3 )
                dfAngle += atof(pszDecimal+3) / 3600.0;
        }

        if( pszAngle[0] == '-' )
            dfAngle *= -1;
    }
    else if( EQUAL(pszUnits,"grad") )
    {
        dfAngle = 180 * (atof(pszAngle ) / 200);
    }
    else
    {
        dfAngle = atof(pszAngle );
    }

    return( dfAngle );
}

/************************************************************************/
/*                           PCSToProjGCS()                             */
/*                                                                      */
/*      Convert a PCS code number into the corresponding projection     */
/*      (Proj_xxx) and datum (GCS_xxx) code.                            */
/************************************************************************/

static int PCSToProjGCS( int nPCS, uint16 *pnProjId, uint16 *pnGCS )

{
    char	**papszFields;
    char	szPCSString[12];

/* -------------------------------------------------------------------- */
/*      Find the corresponding record in ipj_pcs.csv.                   */
/* -------------------------------------------------------------------- */
    sprintf( szPCSString, "%d", nPCS );
    
    papszFields = CSVScanFile( CSVFilename( "ipj_pcs.csv" ), 1, szPCSString,
                               CC_Integer );

    if( papszFields == NULL )
        return( FALSE );

/* -------------------------------------------------------------------- */
/*      Lookup the datum string in the datum.csv file, and extract a    */
/*      datum code.                                                     */
/* -------------------------------------------------------------------- */
    if( CSLCount( papszFields ) > 5 )
    {
        char	**papszDatumFields;

        papszDatumFields = CSVScanFile( CSVFilename( "datum.csv" ), 0,
                                        papszFields[5], CC_ExactString );

        if( CSLCount( papszDatumFields ) > 1 )
        {
            *pnGCS = atoi(papszDatumFields[1]);
        }

        CSLDestroy( papszDatumFields );
    }
    
/* -------------------------------------------------------------------- */
/*      Lookup the projection id (Proj_*) in transform.csv.		*/
/* -------------------------------------------------------------------- */
    if( CSLCount( papszFields ) > 4 )
    {
        char	**papszTransformFields;

        papszTransformFields = CSVScanFile( CSVFilename( "transform.csv" ), 0,
                                            papszFields[4], CC_ExactString );

        if( CSLCount( papszTransformFields ) > 1 )
        {
            *pnProjId = atoi(papszTransformFields[1]);
        }

        CSLDestroy( papszTransformFields );
    }
    
    return( TRUE );
}

/************************************************************************/
/*                       GCSToDatumPMEllipsoid()                        */
/*                                                                      */
/*      Convet a GCS to a datum, prime meridian and ellipsoid.          */
/************************************************************************/

int GCSToDatumPMEllipsoid( int nGCS,
                           uint16 *pnDatum, 
                           double *pdfPM,
                           uint16 *pnEllipsoid )

{
    char	**papszDatumFields;
    char	**papszEllipsoidFields;
    char	szDatumId[20];
    
    /* notdef: for now we don't seem to have a table for transforming
       the GCS to a datum id, and and PM id.

       We set the datum, but this is only accurate if the PM is Greenwich. */

    *pnDatum = nGCS + 2000;

/* -------------------------------------------------------------------- */
/*      Find the record in the datum file, so we can get the            */
/*      ellipsoid name.                                                 */
/* -------------------------------------------------------------------- */
    sprintf( szDatumId, "%d", nGCS );

    papszDatumFields = CSVScanFile( CSVFilename( "datum.csv" ), 1,
                                    szDatumId, CC_Integer );

    if( CSLCount( papszDatumFields ) < 5 )
    {
        CSLDestroy( papszDatumFields );
        return( FALSE );
    }

/* -------------------------------------------------------------------- */
/*	Extract the PM.							*/
/* -------------------------------------------------------------------- */
    if( CSLCount(papszDatumFields) > 5 )
        *pdfPM = atof(papszDatumFields[5]);

/* -------------------------------------------------------------------- */
/*      Look up the ellipsoid in ellipsoid.csv to get the number.	*/
/* -------------------------------------------------------------------- */
    papszEllipsoidFields = CSVScanFile( CSVFilename( "ellipsoid.csv" ),
                                        0, papszDatumFields[4],
                                        CC_ExactString );

    CSLDestroy( papszDatumFields );

    if( CSLCount( papszEllipsoidFields ) > 1 )
    {
        *pnEllipsoid = atoi(papszEllipsoidFields[1]);
    }

    CSLDestroy( papszEllipsoidFields );

    return( TRUE );
}


/************************************************************************/
/*                          GeoTIFFToGXFProj()                          */
/************************************************************************/

int GeoTIFFToGXFProj( TIFF * hTIFF,
                      char *** ppapszMapProjection,
                      char *** ppapszMapDatum,
                      char *** ppapszMapUnits )

{
    uint16	nPCS = KvUserDefined, nModel = KvUserDefined;
    uint16	nProjMethod = KvUserDefined, nProjId = KvUserDefined;
    uint16	nEllipsoid = KvUserDefined, nUnitsId = KvUserDefined;
    uint16	nGCS = KvUserDefined, nDatum = KvUserDefined;
    uint16	nTransId = KvUserDefined;
    double	dfPM = 0.0;
    GTIF	*hGTiff;
    char	**papszMapProjection = NULL;
    char	**papszMapDatum = NULL;
    char	**papszMapUnits = NULL;
    char	**papszRecord;
    char	szId[12], szGCSName[128];

    hGTiff = GTIFNew( hTIFF );

    
    if( GTIFKeyGet(hGTiff, GTModelTypeGeoKey, &nModel, 0, 1 ) != 1 )
        nModel = KvUserDefined;
    
    if( GTIFKeyGet(hGTiff, ProjCoordTransGeoKey, &nProjMethod, 0, 1 ) != 1 )
        nProjMethod = KvUserDefined;
    
/* -------------------------------------------------------------------- */
/*      Look for a PCS.  If we find one try to get a ``Transform        */
/*      name'', and ``datum name'' for that PCS.                        */
/* -------------------------------------------------------------------- */
    if( GTIFKeyGet(hGTiff, ProjectedCSTypeGeoKey, &nPCS, 0, 1 ) == 1 )
    {
        PCSToProjGCS( nPCS, &nProjId, &nGCS );
    }

/* -------------------------------------------------------------------- */
/*	Is there a supplied datum GCS Code?  If so this implies a	*/
/*	Datum and we should use that. 					*/    
/* -------------------------------------------------------------------- */
    GTIFKeyGet(hGTiff, GeographicTypeGeoKey, &nGCS, 0, 1 );

    if( nGCS != KvUserDefined )
    {
        char	**papszFields;

        sprintf( szId, "%d", nGCS );

        papszFields = CSVScanFile( CSVFilename( "datum.csv" ), 1,
                                   szId, CC_Integer );

        if( CSLCount(papszFields) > 0 )
        {
            strcpy( szGCSName, papszFields[0] );
        }
        else
        {
            strcpy( szGCSName, "*Unknown" );
        }

        CSLDestroy( papszFields );
    }
    else
    {
            strcpy( szGCSName, "*Unknown" );
    }

/* -------------------------------------------------------------------- */
/*      Get the underlying datum, and prime meridian                    */
/* -------------------------------------------------------------------- */
    if( nGCS != KvUserDefined )
        GCSToDatumPMEllipsoid( nGCS, &nDatum, &dfPM, &nEllipsoid );

/* -------------------------------------------------------------------- */
/*      Is there a directly supplied datum?  If so, use that, even      */
/*      overriding what was supplied with the PCS.                      */
/* -------------------------------------------------------------------- */
    GTIFKeyGet(hGTiff, GeogGeodeticDatumGeoKey, &nDatum, 0, 1 );

/* -------------------------------------------------------------------- */
/*      Get the name of the PCS if possible.                            */
/* -------------------------------------------------------------------- */
    if( nPCS != KvUserDefined )
    {
        sprintf( szId, "%d", nPCS );
        papszRecord = CSVScanFile( CSVFilename( "ipj_pcs.csv" ),
                                   1, szId, CC_Integer );
    }
    else
    {
        papszRecord = NULL;
    }
                               
    if( CSLCount( papszRecord ) < 2 )
    {
        char	szPCSName[128];

        if( nModel == ModelTypeGeographic )
            sprintf( szPCSName, "\"%s\"", szGCSName );
        else
            sprintf( szPCSName, "\"%s / *Unknown\"", szGCSName );
        
        papszMapProjection = CSLAddString( papszMapProjection,
                                           szPCSName );
    }
    else
    {
        char	szPCSName[128];

        sprintf( szPCSName, "\"%s\"", papszRecord[0] );
        papszMapProjection = CSLAddString( papszMapProjection, szPCSName );
    }

    CSLDestroy( papszRecord );

/* -------------------------------------------------------------------- */
/*      Get the ellipsoid, and parameters.                              */
/* -------------------------------------------------------------------- */
    if( nEllipsoid != KvUserDefined )
    {
        sprintf( szId, "%d", nEllipsoid );
        papszRecord = CSVScanFile( CSVFilename( "ellipsoid.csv" ),
                                   1, szId, CC_Integer );
    }
    else
        papszRecord = NULL;
                               
    if( CSLCount( papszRecord ) < 2 )
    {
        papszMapProjection = CSLAddString( papszMapProjection, "*Unknown" );
    }
    else
    {
        char	szEllipseDefn[256];

        sprintf( szEllipseDefn, "\"%s\",%s,%s,%.7f",
                 papszRecord[0], papszRecord[2], papszRecord[3],
                 dfPM );
        papszMapProjection = CSLAddString( papszMapProjection, szEllipseDefn );
    }

    CSLDestroy( papszRecord );

/* -------------------------------------------------------------------- */
/*      Define the projection method, if we can establish it from       */
/*      the projection id.                                              */
/* -------------------------------------------------------------------- */
    if( nProjId != KvUserDefined )
    {
        sprintf( szId, "%d", nProjId );
        papszRecord = CSVScanFile( CSVFilename( "transform.csv" ),
                                   1, szId, CC_Integer );
    }
    else
    {
        papszRecord = NULL;
    }
                               
    if( CSLCount( papszRecord ) > 12 )
    {
        char	szProjMethod[256];
        
        sprintf( szProjMethod, "*Unknown" );
        
        if( EQUAL(papszRecord[3],"Transverse Mercator") )
        {
            sprintf( szProjMethod, "\"%s\",%.7f,%.7f,%s,%s,%s",
                     papszRecord[3],
                     AngleToDD(papszRecord[6],papszRecord[5]),
                     AngleToDD(papszRecord[7],papszRecord[5]),
                     papszRecord[10],
                     papszRecord[11], papszRecord[12] );
        }
        else if( EQUAL(papszRecord[3],"Lambert Conic Conformal (2SP)") )
        {
            sprintf( szProjMethod, "\"%s\",%.7f,%.7f,%.7f,%.7f,%s,%s",
                     papszRecord[3],
                     AngleToDD(papszRecord[6],papszRecord[5]),
                     AngleToDD(papszRecord[7],papszRecord[5]),
                     AngleToDD(papszRecord[8],papszRecord[5]),
                     AngleToDD(papszRecord[9],papszRecord[5]),
                     papszRecord[11], papszRecord[12] );
        }
        else if( EQUAL(papszRecord[3],"Lambert Conic Conformal (1SP)") )
        {
            sprintf( szProjMethod, "\"%s\",%.7f,%.7f,%s,%s,%s",
                     papszRecord[3],
                     AngleToDD(papszRecord[6],papszRecord[5]),
                     AngleToDD(papszRecord[7],papszRecord[5]),
                     papszRecord[10],
                     papszRecord[11], papszRecord[12] );
        }
        else if( EQUAL(papszRecord[3],"Polar Stereographic") )
        {
            sprintf( szProjMethod, "\"%s\",%.7f,%.7f,%s,%s,%s",
                     papszRecord[3],
                     AngleToDD(papszRecord[6],papszRecord[5]),
                     AngleToDD(papszRecord[7],papszRecord[5]),
                     papszRecord[10], papszRecord[11], papszRecord[12] );
        }
        else if( EQUAL(papszRecord[3],"Hotine Oblique Mercator") )
        {
            /* notdef: are Azimuth, and other angle in decimal degrees,
               or the existing angular units? */
            
            sprintf( szProjMethod, "\"%s\",%.7f,%.7f,%s,%s,%s,%s,%s",
                     papszRecord[3],
                     AngleToDD(papszRecord[6],papszRecord[5]),
                     AngleToDD(papszRecord[7],papszRecord[5]),
                     papszRecord[8], papszRecord[9], 
                     papszRecord[10], papszRecord[11], papszRecord[12] );
        }

        papszMapProjection = CSLAddString( papszMapProjection, szProjMethod );
    }

    CSLDestroy( papszRecord );

/* ==================================================================== */
/*      Define the projection method if read directly from the file.	*/
/* ==================================================================== */
    if( nProjId == KvUserDefined &&
        GTIFKeyGet(hGTiff, ProjCoordTransGeoKey, &nTransId, 0, 1 ) == 1 )
    {
        char	szProjMethod[256];
        double	dfFalseEasting, dfFalseNorthing;

        sprintf( szProjMethod, "*Unknown" );

        if( !GTIFKeyGet(hGTiff, ProjFalseEastingGeoKey, &dfFalseEasting, 0, 1)
            && !GTIFKeyGet(hGTiff, ProjCenterEastingGeoKey,
                           &dfFalseEasting, 0, 1) )
            dfFalseEasting = 0.0;
        
        if( !GTIFKeyGet(hGTiff, ProjFalseNorthingGeoKey, &dfFalseNorthing,0,1)
            && !GTIFKeyGet(hGTiff, ProjCenterNorthingGeoKey,
                           &dfFalseEasting, 0, 1) )
            dfFalseNorthing = 0.0;
        
/* -------------------------------------------------------------------- */
/*      TransverseMercator                                              */
/* -------------------------------------------------------------------- */
        if( nTransId == CT_TransverseMercator )
        {
            double	dfNatOriginLat, dfNatOriginLong;
            double	dfNatOriginScale;

            if( GTIFKeyGet(hGTiff, ProjNatOriginLongGeoKey, 
                           &dfNatOriginLong, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjFalseOriginLongGeoKey, 
                              &dfNatOriginLong, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjCenterLongGeoKey, 
                              &dfNatOriginLong, 0, 1 ) == 0 )
                dfNatOriginLong = 0.0;

            if( GTIFKeyGet(hGTiff, ProjNatOriginLatGeoKey, 
                           &dfNatOriginLat, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjFalseOriginLatGeoKey, 
                              &dfNatOriginLat, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjCenterLatGeoKey, 
                              &dfNatOriginLat, 0, 1 ) == 0 )
                dfNatOriginLat = 0.0;

            if( GTIFKeyGet(hGTiff, ProjScaleAtNatOriginGeoKey,
                           &dfNatOriginScale, 0, 1 ) == 0 )
                dfNatOriginScale = 1.0;
            
            sprintf( szProjMethod, "\"%s\",%.7f,%.7f,%f,%f,%f",
                     "Transverse Mercator",
                     dfNatOriginLat, dfNatOriginLong,
                     dfNatOriginScale, dfFalseEasting, dfFalseNorthing );
        }

/* -------------------------------------------------------------------- */
/*      Transverse Mercator South Oriented (untested)                   */
/* -------------------------------------------------------------------- */
        else if( nTransId == CT_TransvMercator_SouthOriented )
        {
            double	dfNatOriginLat, dfNatOriginLong;
            double	dfNatOriginScale;

            if( GTIFKeyGet(hGTiff, ProjNatOriginLongGeoKey, 
                           &dfNatOriginLong, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjFalseOriginLongGeoKey, 
                              &dfNatOriginLong, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjCenterLongGeoKey, 
                              &dfNatOriginLong, 0, 1 ) == 0 )
                dfNatOriginLong = 0.0;

            if( GTIFKeyGet(hGTiff, ProjNatOriginLatGeoKey, 
                           &dfNatOriginLat, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjFalseOriginLatGeoKey, 
                              &dfNatOriginLat, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjCenterLatGeoKey, 
                              &dfNatOriginLat, 0, 1 ) == 0 )
                dfNatOriginLat = 0.0;

            if( GTIFKeyGet(hGTiff, ProjScaleAtNatOriginGeoKey,
                           &dfNatOriginScale, 0, 1 ) == 0 )
                dfNatOriginScale = 1.0;
            
            sprintf( szProjMethod, "\"%s\",%.7f,%.7f,%f,%f,%f",
                     "Transverse Mercator (South Oriented)",
                     dfNatOriginLat, dfNatOriginLong,
                     dfNatOriginScale, dfFalseEasting, dfFalseNorthing );
        }

/* -------------------------------------------------------------------- */
/*      Oblique Stereographic (untested)                                */
/* -------------------------------------------------------------------- */
        else if( nTransId == CT_ObliqueStereographic )
        {
            double	dfNatOriginLat, dfNatOriginLong;
            double	dfNatOriginScale;

            if( GTIFKeyGet(hGTiff, ProjNatOriginLongGeoKey, 
                           &dfNatOriginLong, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjFalseOriginLongGeoKey, 
                              &dfNatOriginLong, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjCenterLongGeoKey, 
                              &dfNatOriginLong, 0, 1 ) == 0 )
                dfNatOriginLong = 0.0;

            if( GTIFKeyGet(hGTiff, ProjNatOriginLatGeoKey, 
                           &dfNatOriginLat, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjFalseOriginLatGeoKey, 
                              &dfNatOriginLat, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjCenterLatGeoKey, 
                              &dfNatOriginLat, 0, 1 ) == 0 )
                dfNatOriginLat = 0.0;

            if( GTIFKeyGet(hGTiff, ProjScaleAtNatOriginGeoKey,
                           &dfNatOriginScale, 0, 1 ) == 0 )
                dfNatOriginScale = 1.0;
            
            sprintf( szProjMethod, "\"%s\",%.7f,%.7f,%f,%f,%f",
                     "Oblique Stereographic",
                     dfNatOriginLat, dfNatOriginLong,
                     dfNatOriginScale, dfFalseEasting, dfFalseNorthing );
        }

/* -------------------------------------------------------------------- */
/*      Lambert Conic Conformal (1SP) (untested)                        */
/* -------------------------------------------------------------------- */
        else if( nTransId == CT_LambertConfConic_1SP )
        {
            double	dfNatOriginLat, dfNatOriginLong;
            double	dfNatOriginScale;

            if( GTIFKeyGet(hGTiff, ProjNatOriginLongGeoKey, 
                           &dfNatOriginLong, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjFalseOriginLongGeoKey, 
                              &dfNatOriginLong, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjCenterLongGeoKey, 
                              &dfNatOriginLong, 0, 1 ) == 0 )
                dfNatOriginLong = 0.0;

            if( GTIFKeyGet(hGTiff, ProjNatOriginLatGeoKey, 
                           &dfNatOriginLat, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjFalseOriginLatGeoKey, 
                              &dfNatOriginLat, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjCenterLatGeoKey, 
                              &dfNatOriginLat, 0, 1 ) == 0 )
                dfNatOriginLat = 0.0;

            if( GTIFKeyGet(hGTiff, ProjScaleAtNatOriginGeoKey,
                           &dfNatOriginScale, 0, 1 ) == 0 )
                dfNatOriginScale = 1.0;
            
            sprintf( szProjMethod, "\"%s\",%.7f,%.7f,%f,%f,%f",
                     "Lambert Conic Conformal (1SP)",
                     dfNatOriginLat, dfNatOriginLong,
                     dfNatOriginScale, dfFalseEasting, dfFalseNorthing );
        }

/* -------------------------------------------------------------------- */
/*      Lambert Conformal Conic (2SP)                                   */
/* -------------------------------------------------------------------- */
        else if( nTransId == CT_LambertConfConic_2SP )
        {
            double	dfNatOriginLat, dfNatOriginLong;
            double	dfStdP1, dfStdP2;
            
            if( GTIFKeyGet(hGTiff, ProjStdParallelGeoKey,
                           &dfStdP1, 0, 1 ) == 0 )
                dfStdP1 = 0.0;

            if( GTIFKeyGet(hGTiff, ProjStdParallel2GeoKey,
                           &dfStdP2, 0, 1 ) == 0 )
                dfStdP2 = 0.0;

            /* notdef: The following following odd construction is intended
               to allow support for broken PCI files which write out a
               NatOriginLong with a value, and 0.0 to FalseOriginLong */
            if( GTIFKeyGet(hGTiff, ProjNatOriginLongGeoKey, 
                           &dfNatOriginLong, 0, 1 ) == 0 )
                dfNatOriginLong = 0.0;

            if( dfNatOriginLong == 0.0 
                && GTIFKeyGet(hGTiff, ProjFalseOriginLongGeoKey, 
                              &dfNatOriginLong, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjCenterLongGeoKey, 
                              &dfNatOriginLong, 0, 1 ) == 0 )
                dfNatOriginLong = 0.0;

            if( GTIFKeyGet(hGTiff, ProjNatOriginLatGeoKey, 
                           &dfNatOriginLat, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjFalseOriginLatGeoKey, 
                              &dfNatOriginLat, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjCenterLatGeoKey, 
                              &dfNatOriginLat, 0, 1 ) == 0 )
                dfNatOriginLat = 0.0;

            sprintf( szProjMethod, "\"%s\",%.7f,%.7f,%.7f,%.7f,%f,%f",
                     "Lambert Conic Conformal (2SP)",
                     dfStdP1, dfStdP2,
                     dfNatOriginLat, dfNatOriginLong,
                     dfFalseEasting, dfFalseNorthing );
        }

/* -------------------------------------------------------------------- */
/*      Polar Stereographic                                             */
/* -------------------------------------------------------------------- */
        else if( nTransId == CT_PolarStereographic )
        {
            double	dfNatOriginLat, dfNatOriginLong;
            double	dfNatOriginScale;

            if( GTIFKeyGet(hGTiff, ProjNatOriginLongGeoKey, 
                           &dfNatOriginLong, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjFalseOriginLongGeoKey, 
                              &dfNatOriginLong, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjCenterLongGeoKey, 
                              &dfNatOriginLong, 0, 1 ) == 0 
                && GTIFKeyGet(hGTiff, ProjStraightVertPoleLongGeoKey, 
                              &dfNatOriginLong, 0, 1 ) == 0 )
                dfNatOriginLong = 0.0;

            if( GTIFKeyGet(hGTiff, ProjNatOriginLatGeoKey, 
                           &dfNatOriginLat, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjFalseOriginLatGeoKey, 
                              &dfNatOriginLat, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjCenterLatGeoKey, 
                              &dfNatOriginLat, 0, 1 ) == 0 )
                dfNatOriginLat = 0.0;

            if( GTIFKeyGet(hGTiff, ProjScaleAtNatOriginGeoKey,
                           &dfNatOriginScale, 0, 1 ) == 0 )
                dfNatOriginScale = 1.0;
            
            sprintf( szProjMethod, "\"%s\",%.7f,%.7f,%f,%f,%f",
                     "Polar Stereographic",
                     dfNatOriginLat, dfNatOriginLong,
                     dfNatOriginScale, dfFalseEasting, dfFalseNorthing );
        }
        
/* -------------------------------------------------------------------- */
/*      Mercator (1SP)                                                  */
/* -------------------------------------------------------------------- */
        else if( nTransId == CT_Mercator )
        {
            double	dfNatOriginLat, dfNatOriginLong;
            double	dfNatOriginScale;

            if( GTIFKeyGet(hGTiff, ProjNatOriginLongGeoKey, 
                           &dfNatOriginLong, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjFalseOriginLongGeoKey, 
                              &dfNatOriginLong, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjCenterLongGeoKey, 
                              &dfNatOriginLong, 0, 1 ) == 0 )
                dfNatOriginLong = 0.0;

            if( GTIFKeyGet(hGTiff, ProjNatOriginLatGeoKey, 
                           &dfNatOriginLat, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjFalseOriginLatGeoKey, 
                              &dfNatOriginLat, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjCenterLatGeoKey, 
                              &dfNatOriginLat, 0, 1 ) == 0 )
                dfNatOriginLat = 0.0;

            if( GTIFKeyGet(hGTiff, ProjScaleAtNatOriginGeoKey,
                           &dfNatOriginScale, 0, 1 ) == 0 )
                dfNatOriginScale = 1.0;
            
            sprintf( szProjMethod, "\"%s\",%.7f,%.7f,%f,%f,%f",
                     "Mercator (1SP)",
                     dfNatOriginLat, dfNatOriginLong,
                     dfNatOriginScale, dfFalseEasting, dfFalseNorthing );
        }
        
/* -------------------------------------------------------------------- */
/*      New Zealand grid (untested)                                     */
/* -------------------------------------------------------------------- */
        else if( nTransId == CT_NewZealandMapGrid )
        {
            double	dfNatOriginLat, dfNatOriginLong;

            if( GTIFKeyGet(hGTiff, ProjNatOriginLongGeoKey, 
                           &dfNatOriginLong, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjFalseOriginLongGeoKey, 
                              &dfNatOriginLong, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjCenterLongGeoKey, 
                              &dfNatOriginLong, 0, 1 ) == 0 )
                dfNatOriginLong = 0.0;

            if( GTIFKeyGet(hGTiff, ProjNatOriginLatGeoKey, 
                           &dfNatOriginLat, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjFalseOriginLatGeoKey, 
                              &dfNatOriginLat, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjCenterLatGeoKey, 
                              &dfNatOriginLat, 0, 1 ) == 0 )
                dfNatOriginLat = 0.0;

            sprintf( szProjMethod, "\"%s\",%.7f,%.7f,%f,%f",
                     "New Zealand Map Grid",
                     dfNatOriginLat, dfNatOriginLong,
                     dfFalseEasting, dfFalseNorthing );
        }
        
/* -------------------------------------------------------------------- */
/*	Polyconic							*/
/* -------------------------------------------------------------------- */
        else if( nTransId == CT_Polyconic )
        {
            double	dfNatOriginLat, dfNatOriginLong;
            double	dfNatOriginScale;

            if( GTIFKeyGet(hGTiff, ProjNatOriginLongGeoKey, 
                           &dfNatOriginLong, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjFalseOriginLongGeoKey, 
                              &dfNatOriginLong, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjCenterLongGeoKey, 
                              &dfNatOriginLong, 0, 1 ) == 0 )
                dfNatOriginLong = 0.0;

            if( GTIFKeyGet(hGTiff, ProjNatOriginLatGeoKey, 
                           &dfNatOriginLat, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjFalseOriginLatGeoKey, 
                              &dfNatOriginLat, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjCenterLatGeoKey, 
                              &dfNatOriginLat, 0, 1 ) == 0 )
                dfNatOriginLat = 0.0;

            if( GTIFKeyGet(hGTiff, ProjScaleAtNatOriginGeoKey,
                           &dfNatOriginScale, 0, 1 ) == 0 )
                dfNatOriginScale = 1.0;
            
            sprintf( szProjMethod, "\"%s\",%.7f,%.7f,%f,%f,%f",
                     "*Polyconic",
                     dfNatOriginLat, dfNatOriginLong,
                     dfNatOriginScale, dfFalseEasting, dfFalseNorthing );
        }
        
/* -------------------------------------------------------------------- */
/*      Oblique Mercator (Hotine)                                       */
/* -------------------------------------------------------------------- */
        else if( nTransId == CT_ObliqueMercator )
        {
            double	dfCenterLat, dfCenterLong;
            double	dfScale, dfAzimuth, dfAngle;

            if( GTIFKeyGet(hGTiff, ProjNatOriginLongGeoKey, 
                           &dfCenterLong, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjFalseOriginLongGeoKey, 
                              &dfCenterLong, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjCenterLongGeoKey, 
                              &dfCenterLong, 0, 1 ) == 0 )
                dfCenterLong = 0.0;

            if( GTIFKeyGet(hGTiff, ProjNatOriginLatGeoKey, 
                           &dfCenterLat, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjFalseOriginLatGeoKey, 
                              &dfCenterLat, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjCenterLatGeoKey, 
                              &dfCenterLat, 0, 1 ) == 0 )
                dfCenterLat = 0.0;

            if( GTIFKeyGet(hGTiff, ProjScaleAtNatOriginGeoKey,
                           &dfScale, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjScaleAtCenterGeoKey,
                           &dfScale, 0, 1 ) == 0 )
                dfScale = 1.0;

            if( GTIFKeyGet(hGTiff, ProjAzimuthAngleGeoKey,
                           &dfAzimuth, 0, 1 ) == 0 )
                dfAzimuth = 1.0;

            dfAngle = 0.0; /* notdef: should this be in geotiff? */
            
            sprintf( szProjMethod, "\"%s\",%.7f,%.7f,%f,%f,%f,%f,%f",
                     "Hotine Oblique Mercator",
                     dfCenterLat, dfCenterLong,
                     dfAzimuth, 0.0, dfScale,
                     dfFalseEasting, dfFalseNorthing );
        }
        
/* -------------------------------------------------------------------- */
/*      Laborde Oblique Mercator (untested)                             */
/* -------------------------------------------------------------------- */
        else if( nTransId == CT_ObliqueMercator_Laborde )
        {
            double	dfCenterLat, dfCenterLong;
            double	dfScale, dfAzimuth;

            if( GTIFKeyGet(hGTiff, ProjNatOriginLongGeoKey, 
                           &dfCenterLong, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjFalseOriginLongGeoKey, 
                              &dfCenterLong, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjCenterLongGeoKey, 
                              &dfCenterLong, 0, 1 ) == 0 )
                dfCenterLong = 0.0;

            if( GTIFKeyGet(hGTiff, ProjNatOriginLatGeoKey, 
                           &dfCenterLat, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjFalseOriginLatGeoKey, 
                              &dfCenterLat, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjCenterLatGeoKey, 
                              &dfCenterLat, 0, 1 ) == 0 )
                dfCenterLat = 0.0;

            if( GTIFKeyGet(hGTiff, ProjScaleAtNatOriginGeoKey,
                           &dfScale, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjScaleAtCenterGeoKey,
                           &dfScale, 0, 1 ) == 0 )
                dfScale = 1.0;

            if( GTIFKeyGet(hGTiff, ProjAzimuthAngleGeoKey,
                           &dfAzimuth, 0, 1 ) == 0 )
                dfAzimuth = 1.0;

            sprintf( szProjMethod, "\"%s\",%.7f,%.7f,%f,%f,%f,%f",
                     "Laborde Oblique Mercator",
                     dfCenterLat, dfCenterLong,
                     dfScale, dfAzimuth,
                     dfFalseEasting, dfFalseNorthing );
        }
        
/* -------------------------------------------------------------------- */
/*      Swiss Oblique Cylindrical (untested)                            */
/* -------------------------------------------------------------------- */
        else if( nTransId == CT_SwissObliqueCylindrical )
        {
            double	dfNatOriginLat, dfNatOriginLong;

            if( GTIFKeyGet(hGTiff, ProjNatOriginLongGeoKey, 
                           &dfNatOriginLong, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjFalseOriginLongGeoKey, 
                              &dfNatOriginLong, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjCenterLongGeoKey, 
                              &dfNatOriginLong, 0, 1 ) == 0 )
                dfNatOriginLong = 0.0;

            if( GTIFKeyGet(hGTiff, ProjNatOriginLatGeoKey, 
                           &dfNatOriginLat, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjFalseOriginLatGeoKey, 
                              &dfNatOriginLat, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjCenterLatGeoKey, 
                              &dfNatOriginLat, 0, 1 ) == 0 )
                dfNatOriginLat = 0.0;

            sprintf( szProjMethod, "\"%s\",%.7f,%.7f,%f,%f",
                     "Swiss Oblique Cylindrical",
                     dfNatOriginLat, dfNatOriginLong,
                     dfFalseEasting, dfFalseNorthing );
        }

/* -------------------------------------------------------------------- */
/*      Equidistant Conic                                               */
/* -------------------------------------------------------------------- */
        else if( nTransId == CT_EquidistantConic )
        {
            double	dfNatOriginLat, dfCenterLong;
            double	dfStdP1, dfStdP2;

            if( GTIFKeyGet(hGTiff, ProjNatOriginLongGeoKey, 
                           &dfCenterLong, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjFalseOriginLongGeoKey, 
                              &dfCenterLong, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjCenterLongGeoKey, 
                              &dfCenterLong, 0, 1 ) == 0 
                && GTIFKeyGet(hGTiff, ProjStraightVertPoleLongGeoKey, 
                              &dfCenterLong, 0, 1 ) == 0 )
                dfCenterLong = 0.0;

            if( GTIFKeyGet(hGTiff, ProjStdParallelGeoKey,
                           &dfStdP1, 0, 1 ) == 0 )
                dfStdP1 = dfCenterLong;

            if( GTIFKeyGet(hGTiff, ProjStdParallel2GeoKey,
                           &dfStdP2, 0, 1 ) == 0 )
                dfStdP2 = dfStdP1;

            if( GTIFKeyGet(hGTiff, ProjNatOriginLatGeoKey, 
                           &dfNatOriginLat, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjFalseOriginLatGeoKey, 
                              &dfNatOriginLat, 0, 1 ) == 0
                && GTIFKeyGet(hGTiff, ProjCenterLatGeoKey, 
                              &dfNatOriginLat, 0, 1 ) == 0 )
                dfNatOriginLat = 0.0;

            sprintf( szProjMethod, "\"%s\",%.7f,%.7f,%.7f,%.7f,%f,%f",
                     "*Equidistant Conic",
                     dfStdP1, dfStdP2,
                     dfNatOriginLat, dfCenterLong,
                     dfFalseEasting, dfFalseNorthing );
        }
        
        papszMapProjection = CSLAddString( papszMapProjection, szProjMethod );
    }

/* ==================================================================== */
/*      Build up the MAP_DATUM definition, if possible.                 */
/* ==================================================================== */
    /* notdef: for now no datum transform is returned because in cases with
       multiple possible transforms (for different regions) we can't
       know which to pick */

/* ==================================================================== */
/*      Build up the units definition.                                  */
/* ==================================================================== */
    nUnitsId = Linear_Meter;
    
/* -------------------------------------------------------------------- */
/*      If we have a projection id, derive the units code from that.    */
/* -------------------------------------------------------------------- */
    if( nProjId != KvUserDefined )
    {
        sprintf( szId, "%d", nProjId );
        papszRecord = CSVScanFile( CSVFilename( "transform.csv" ),
                                   1, szId, CC_Integer );

        if( CSLCount(papszRecord) > 4 )
        {
            char	**papszUnitsRecord;

            papszUnitsRecord = CSVScanFile( CSVFilename( "units.csv" ),
                                            0, papszRecord[4],
                                            CC_ExactString );
            if( CSLCount( papszUnitsRecord ) > 1 )
                nUnitsId = atoi(papszUnitsRecord[1] );

            CSLDestroy( papszUnitsRecord );
        }

        CSLDestroy( papszRecord );
    }

/* -------------------------------------------------------------------- */
/*      If an explicit units code is provided, use that.                */
/* -------------------------------------------------------------------- */
    GTIFKeyGet(hGTiff, GeogLinearUnitsGeoKey, &nUnitsId, 0, 1 );
    GTIFKeyGet(hGTiff, ProjLinearUnitsGeoKey, &nUnitsId, 0, 1 );
    
/* -------------------------------------------------------------------- */
/*      Look this unit code up, and find the translation.               */
/* -------------------------------------------------------------------- */
    if( nUnitsId != KvUserDefined && nModel != ModelTypeGeographic )
    {
        sprintf( szId, "%d", nUnitsId );
        papszRecord = CSVScanFile( CSVFilename( "units.csv" ),
                                   1, szId, CC_ExactString );
        
        if( CSLCount(papszRecord) > 3 )
        {
            char	szDefn[128];
            
            sprintf( szDefn, "\"%s\",%s", papszRecord[0], papszRecord[3] );
            papszMapUnits =
                CSLAddString( papszMapUnits, szDefn );
        }
    }
    else if( nModel == ModelTypeGeographic )
    {
        /* the units.csv is lacking on the angular units side */
        
        papszMapUnits =
            CSLAddString( papszMapUnits, "dega,1" );
    }
    
/* -------------------------------------------------------------------- */
/*      Assign lists to pass back.                                      */
/* -------------------------------------------------------------------- */
    *ppapszMapProjection = papszMapProjection;
    *ppapszMapDatum = papszMapDatum;
    *ppapszMapUnits = papszMapUnits;

    GTIFFree( hGTiff );

    return( TRUE );
}

/************************************************************************/
/*                          GeoTIFFToGXFProj()                          */
/************************************************************************/

int GXFProjToGeoTIFF( TIFF * hTIFF,
                      const char * pszProjName,
                      const char * pszEllipse,
                      const char * pszMethod,
                      const char * pszUnits,
                      const char * pszDatumTr )


{
    int16	nPCS, nUnitsCode;
    char	**papszFields, **papszTokens;
    GTIF	*hGTiff;
    char	*pszCitation, *pszStr;
    int		nProjected = TRUE;
    int		bSuccess = FALSE;
    
    hGTiff = GTIFNew( hTIFF );

/* -------------------------------------------------------------------- */
/*      Write out the pixelisarea message.  Should we consider          */
/*      writing PixelIsPoint for Geosoft, where they normally think     */
/*      in these terms?                                                 */
/* -------------------------------------------------------------------- */
    GTIFKeySet(hGTiff, GTRasterTypeGeoKey, TYPE_SHORT,  1,
               RasterPixelIsArea );

/* -------------------------------------------------------------------- */
/*      Write a citation based on the GXF info.                         */
/* -------------------------------------------------------------------- */
    pszCitation = (char *) CPLMalloc( strlen(pszProjName)
                                      + strlen(pszEllipse)
                                      + strlen(pszMethod)
                                      + strlen(pszUnits)
                                      + strlen(pszDatumTr) + 200 );

    sprintf( pszCitation,
             "#MAP_PROJECTION\n%s\n%s\n%s\n"
             "#UNITS_LENGTH\n%s\n"
             "#MAP_DATUM_TRANSFORM\n%s\n",
             pszProjName, pszEllipse, pszMethod, pszUnits, pszDatumTr );


    GTIFKeySet(hGTiff, GTCitationGeoKey, TYPE_ASCII,  0, pszCitation );

    CPLFree( pszCitation );
    
/* -------------------------------------------------------------------- */
/*      Try to find the units code.  If found we assign it even if      */
/*      it agrees with the PCS.                                         */
/* -------------------------------------------------------------------- */
    papszTokens = CSLTokenizeStringComplex( pszUnits, ",", TRUE, TRUE );
    if( CSLCount(papszTokens) > 0 && strcmp(papszTokens[0],"dega") == 0 )
    {
        nProjected = FALSE;
        GTIFKeySet(hGTiff, GeogAngularUnitsGeoKey, TYPE_SHORT,  1,
                   Angular_Degree );
    }
    else if( CSLCount(papszTokens) > 0 )
    {
        papszFields = CSVScanFile( CSVFilename( "units.csv" ), 0,
                                   papszTokens[0], CC_ExactString );
        if( CSLCount(papszFields) > 1 )
        {
            nUnitsCode = atoi(papszFields[1]);
            GTIFKeySet(hGTiff, ProjLinearUnitsGeoKey, TYPE_SHORT,  1,
                       nUnitsCode );
            
        }

        CSLDestroy( papszFields );
    }

    CSLDestroy( papszTokens );

/* -------------------------------------------------------------------- */
/*      Write out the general model type.                               */
/* -------------------------------------------------------------------- */
    if( nProjected )
        GTIFKeySet(hGTiff, GTModelTypeGeoKey, TYPE_SHORT,  1,
                   ModelTypeProjected );
    else
    {
        GTIFKeySet(hGTiff, GTModelTypeGeoKey, TYPE_SHORT,  1,
                   ModelTypeGeographic );
        bSuccess = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Try to find a PCS code for this projection.  We sort of         */
/*      tokenize the projection name to strip off double quotes if      */
/*      present.                                                        */
/* -------------------------------------------------------------------- */
    papszTokens = CSLTokenizeStringComplex( pszProjName, "", TRUE, TRUE );

    if( CSLCount( papszTokens ) > 0 )
        papszFields = CSVScanFile( CSVFilename( "ipj_pcs.csv" ), 0,
                                   papszTokens[0], CC_ExactString );
    else
        papszFields = NULL;

    if( CSLCount( papszFields) < 2 )
    {
        nPCS = KvUserDefined;
    }
    else
    {
        nPCS = atoi(papszFields[1]);
        if( nPCS > 0 )
        {
            bSuccess = TRUE;
            GTIFKeySet(hGTiff, ProjectedCSTypeGeoKey, TYPE_SHORT,  1,
                       nPCS );
        }
        else
            nPCS = KvUserDefined;
    }

    CSLDestroy( papszFields );
    
/* -------------------------------------------------------------------- */
/*      If we couldn't look up the PCS, try to desire a GCS from the    */
/*      projection name.  If we find a slash in the projection name,    */
/*      we strip it, and preceeding white space off.                    */
/* -------------------------------------------------------------------- */
    if( CSLCount(papszTokens) > 0 && nPCS == KvUserDefined )
    {
        pszStr = strchr( papszTokens[0], '/' );
        if( pszStr != NULL )
        {
            *pszStr = '\0';
            while( pszStr != papszTokens[0] && *(--pszStr) == ' ' )
                *pszStr = '\0';
        }

        papszFields = CSVScanFile( CSVFilename( "datum.csv" ), 0,
                                   papszTokens[0], CC_ExactString );
        if( CSLCount(papszFields) > 0 && atoi(papszFields[1]) > 0 )
        {
            GTIFKeySet(hGTiff, GeographicTypeGeoKey, TYPE_SHORT,  1,
                       atoi(papszFields[1]) );
        }

        CSLDestroy( papszFields );
    }
    
    CSLDestroy( papszTokens );
    
/* ==================================================================== */
/*	If we had no PCS, and the data is projected, try to work 	*/
/*	something out from the projection method 			*/
/* ==================================================================== */
    papszTokens = CSLTokenizeStringComplex( pszMethod, ",", TRUE, TRUE );

    if( CSLCount( papszTokens ) < 1 || nPCS != KvUserDefined )
    {
        /* do nothing */
    }
    else
    {
        double	dfP1=0.0, dfP2=0.0, dfP3=0.0, dfP4=0.0,
                dfP5=0.0, dfP6=0.0, dfP7=0.0, dfP8=0.0;

        if( CSLCount( papszTokens ) > 1 )
            dfP1 = atof(papszTokens[1] );
        if( CSLCount( papszTokens ) > 2 )
            dfP2 = atof(papszTokens[2] );
        if( CSLCount( papszTokens ) > 3 )
            dfP3 = atof(papszTokens[3] );
        if( CSLCount( papszTokens ) > 4 )
            dfP4 = atof(papszTokens[4] );
        if( CSLCount( papszTokens ) > 5 )
            dfP5 = atof(papszTokens[5] );
        if( CSLCount( papszTokens ) > 6 )
            dfP6 = atof(papszTokens[6] );
        if( CSLCount( papszTokens ) > 7 )
            dfP7 = atof(papszTokens[7] );
        if( CSLCount( papszTokens ) > 8 )
            dfP8 = atof(papszTokens[8] );

/* -------------------------------------------------------------------- */
/*      Transverse Mercator                                             */
/* -------------------------------------------------------------------- */
        if( strcmp(papszTokens[0],"Transverse Mercator") == 0 )
        {
            bSuccess = TRUE;
            if( dfP3 == 0.0 )
                dfP3 = 1.0;

            GTIFKeySet(hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                       CT_TransverseMercator );
            GTIFKeySet(hGTiff, ProjNatOriginLatGeoKey, TYPE_DOUBLE,1, dfP1 );
            GTIFKeySet(hGTiff, ProjNatOriginLongGeoKey, TYPE_DOUBLE,1, dfP2 );
            GTIFKeySet(hGTiff, ProjScaleAtNatOriginGeoKey,TYPE_DOUBLE,1, dfP3);
            GTIFKeySet(hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE,1, dfP4 );
            GTIFKeySet(hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE,1, dfP5 );
        }
        
/* -------------------------------------------------------------------- */
/*      Transverse Mercator - South Oriented (untested)                 */
/* -------------------------------------------------------------------- */
        if( strcmp(papszTokens[0],
                   "Transverse Mercator (South Oriented)") == 0 )
        {
            bSuccess = TRUE;
            if( dfP3 == 0.0 )
                dfP3 = 1.0;

            GTIFKeySet(hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                       CT_TransvMercator_SouthOriented );
            GTIFKeySet(hGTiff, ProjNatOriginLatGeoKey, TYPE_DOUBLE,1, dfP1 );
            GTIFKeySet(hGTiff, ProjNatOriginLongGeoKey, TYPE_DOUBLE,1, dfP2 );
            GTIFKeySet(hGTiff, ProjScaleAtNatOriginGeoKey,TYPE_DOUBLE,1, dfP3);
            GTIFKeySet(hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE,1, dfP4 );
            GTIFKeySet(hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE,1, dfP5 );
        }
        
/* -------------------------------------------------------------------- */
/*      Oblique Stereographic (untested)                                */
/* -------------------------------------------------------------------- */
        if( strcmp(papszTokens[0], "Oblique Stereographic") == 0 )
        {
            bSuccess = TRUE;
            if( dfP3 == 0.0 )
                dfP3 = 1.0;

            GTIFKeySet(hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                       CT_ObliqueStereographic );
            GTIFKeySet(hGTiff, ProjNatOriginLatGeoKey, TYPE_DOUBLE,1, dfP1 );
            GTIFKeySet(hGTiff, ProjNatOriginLongGeoKey, TYPE_DOUBLE,1, dfP2 );
            GTIFKeySet(hGTiff, ProjScaleAtNatOriginGeoKey,TYPE_DOUBLE,1, dfP3);
            GTIFKeySet(hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE,1, dfP4 );
            GTIFKeySet(hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE,1, dfP5 );
        }
        
/* -------------------------------------------------------------------- */
/*      Lambert Conic Conformal (1SP) (untested)                        */
/* -------------------------------------------------------------------- */
        if( strcmp(papszTokens[0], "Lambert Conic Conformal (1SP)") == 0 )
        {
            bSuccess = TRUE;
            if( dfP3 == 0.0 )
                dfP3 = 1.0;

            GTIFKeySet(hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                       CT_LambertConfConic_1SP );
            GTIFKeySet(hGTiff, ProjNatOriginLatGeoKey, TYPE_DOUBLE,1, dfP1 );
            GTIFKeySet(hGTiff, ProjNatOriginLongGeoKey, TYPE_DOUBLE,1, dfP2 );
            GTIFKeySet(hGTiff, ProjScaleAtNatOriginGeoKey,TYPE_DOUBLE,1, dfP3);
            GTIFKeySet(hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE,1, dfP4 );
            GTIFKeySet(hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE,1, dfP5 );
        }
        
/* -------------------------------------------------------------------- */
/*      Lambert conic conformal                                         */
/* -------------------------------------------------------------------- */
        if( strcmp(papszTokens[0],"Lambert Conic Conformal (2SP)") == 0 )
        {
            bSuccess = TRUE;
            /* note: writing NatOriginLongGeoKey, though it seems unneeded,
               and would always have to match the NatOriginLong */
            GTIFKeySet(hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                       CT_LambertConfConic_2SP );
            GTIFKeySet(hGTiff, ProjStdParallelGeoKey, TYPE_DOUBLE, 1, dfP1 );
            GTIFKeySet(hGTiff, ProjStdParallel2GeoKey, TYPE_DOUBLE, 1, dfP2 );
            GTIFKeySet(hGTiff, ProjFalseOriginLatGeoKey, TYPE_DOUBLE, 1, dfP3);
            GTIFKeySet(hGTiff, ProjFalseOriginLongGeoKey, TYPE_DOUBLE, 1,dfP4);
            GTIFKeySet(hGTiff, ProjNatOriginLongGeoKey, TYPE_DOUBLE, 1, dfP4 );
            GTIFKeySet(hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1, dfP5 );
            GTIFKeySet(hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1, dfP6 );
        }

/* -------------------------------------------------------------------- */
/*      Polar Stereographic                                             */
/*                                                                      */
/*      Note that I write StraightVertPoleLong as per the Erdas         */
/*      proposal even though the EPSG transform_parameters table        */
/*      suggests this is just a natural origin long.  I am also         */
/*      writing out a scale factor even though this isn't mentioned     */
/*      in any of the GeoTIFF literature.  It is in the EPSG tables     */
/*      though.                                                         */
/* -------------------------------------------------------------------- */
        if( strcmp(papszTokens[0], "Polar Stereographic") == 0 )
        {
            bSuccess = TRUE;
            if( dfP3 == 0.0 )
                dfP3 = 1.0;

            GTIFKeySet(hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                       CT_PolarStereographic );
            GTIFKeySet(hGTiff, ProjNatOriginLatGeoKey, TYPE_DOUBLE,1, dfP1 );
            GTIFKeySet(hGTiff, ProjStraightVertPoleLongGeoKey,
                       					TYPE_DOUBLE,1, dfP2 );
            GTIFKeySet(hGTiff, ProjScaleAtNatOriginGeoKey,TYPE_DOUBLE,1, dfP3);
            GTIFKeySet(hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE,1, dfP4 );
            GTIFKeySet(hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE,1, dfP5 );
        }
        
/* -------------------------------------------------------------------- */
/*      Mercator (1SP)                                                  */
/* -------------------------------------------------------------------- */
        if( strcmp(papszTokens[0], "Mercator (1SP)") == 0 )
        {
            bSuccess = TRUE;
            if( dfP3 == 0.0 )
                dfP3 = 1.0;

            GTIFKeySet(hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                       CT_Mercator );
            GTIFKeySet(hGTiff, ProjNatOriginLatGeoKey, TYPE_DOUBLE,1, dfP1 );
            GTIFKeySet(hGTiff, ProjNatOriginLongGeoKey, TYPE_DOUBLE,1, dfP2 );
            GTIFKeySet(hGTiff, ProjScaleAtNatOriginGeoKey,TYPE_DOUBLE,1, dfP3);
            GTIFKeySet(hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE,1, dfP4 );
            GTIFKeySet(hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE,1, dfP5 );
        }

/* -------------------------------------------------------------------- */
/*      New Zealand grid (untested)                                     */
/* -------------------------------------------------------------------- */
        if( strcmp(papszTokens[0], "New Zealand Map Grid") == 0 )
        {
            bSuccess = TRUE;
            GTIFKeySet(hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                       CT_NewZealandMapGrid );
            GTIFKeySet(hGTiff, ProjNatOriginLatGeoKey, TYPE_DOUBLE,1, dfP1 );
            GTIFKeySet(hGTiff, ProjNatOriginLongGeoKey, TYPE_DOUBLE,1, dfP2 );
            GTIFKeySet(hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE,1, dfP3 );
            GTIFKeySet(hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE,1, dfP4 );
        }

/* -------------------------------------------------------------------- */
/*      Polyconic                                                       */
/*                                                                      */
/*      The GXF tables include a scale at natural origin which          */
/*      isn't in the GeoTIFF definition of polyconic.  I write it       */
/*      even though it isn't in the standard.                           */
/* -------------------------------------------------------------------- */
        if( strcmp(papszTokens[0], "*Polyconic") == 0 )
        {
            bSuccess = TRUE;
            if( dfP3 == 0.0 )
                dfP3 = 1.0;

            GTIFKeySet(hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                       CT_Polyconic );
            GTIFKeySet(hGTiff, ProjNatOriginLatGeoKey, TYPE_DOUBLE,1, dfP1 );
            GTIFKeySet(hGTiff, ProjNatOriginLongGeoKey, TYPE_DOUBLE,1, dfP2 );
            GTIFKeySet(hGTiff, ProjScaleAtNatOriginGeoKey,TYPE_DOUBLE,1, dfP3);
            GTIFKeySet(hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE,1, dfP4 );
            GTIFKeySet(hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE,1, dfP5 );
        }

/* -------------------------------------------------------------------- */
/*      Oblique Mercator (Hotine)                                       */
/* -------------------------------------------------------------------- */
        if( strcmp(papszTokens[0], "Hotine Oblique Mercator") == 0 )
        {
            bSuccess = TRUE;
            if( dfP5 == 0.0 )
                dfP5 = 1.0;

            GTIFKeySet(hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                       CT_ObliqueMercator );
            GTIFKeySet(hGTiff, ProjCenterLatGeoKey, TYPE_DOUBLE,1, dfP1 );
            GTIFKeySet(hGTiff, ProjCenterLongGeoKey, TYPE_DOUBLE,1, dfP2 );
            GTIFKeySet(hGTiff, ProjAzimuthAngleGeoKey, TYPE_DOUBLE,1, dfP3 );
            /* ignoring 'Angle from Rectified to Skew grid' - not in Geotiff */
            GTIFKeySet(hGTiff, ProjScaleAtNatOriginGeoKey,TYPE_DOUBLE,1, dfP5);
            GTIFKeySet(hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE,1, dfP6 );
            GTIFKeySet(hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE,1, dfP7 );
        }

/* -------------------------------------------------------------------- */
/*      Laborde Oblique Mercator (untested)                             */
/* -------------------------------------------------------------------- */
        if( strcmp(papszTokens[0], "Laborde Oblique Mercator") == 0 )
        {
            bSuccess = TRUE;
            if( dfP4 == 0.0 )
                dfP4 = 1.0;
            
            GTIFKeySet(hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                       CT_ObliqueMercator_Laborde );
            GTIFKeySet(hGTiff, ProjCenterLatGeoKey, TYPE_DOUBLE,1, dfP1 );
            GTIFKeySet(hGTiff, ProjCenterLongGeoKey, TYPE_DOUBLE,1, dfP2 );
            GTIFKeySet(hGTiff, ProjAzimuthAngleGeoKey, TYPE_DOUBLE,1, dfP3 );
            GTIFKeySet(hGTiff, ProjScaleAtNatOriginGeoKey,TYPE_DOUBLE,1, dfP4);
            GTIFKeySet(hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE,1, dfP5 );
            GTIFKeySet(hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE,1, dfP6 );
        }

/* -------------------------------------------------------------------- */
/*      Swiss Oblique Cylindrical (untested)                            */
/* -------------------------------------------------------------------- */
        if( strcmp(papszTokens[0], "Swiss Oblique Cylindrical") == 0 )
        {
            bSuccess = TRUE;
            GTIFKeySet(hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                       CT_SwissObliqueCylindrical );
            GTIFKeySet(hGTiff, ProjNatOriginLatGeoKey, TYPE_DOUBLE,1, dfP1 );
            GTIFKeySet(hGTiff, ProjNatOriginLongGeoKey, TYPE_DOUBLE,1, dfP2 );
            GTIFKeySet(hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE,1, dfP3 );
            GTIFKeySet(hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE,1, dfP4 );
        }

/* -------------------------------------------------------------------- */
/*      Equidistant Conic                                               */
/* -------------------------------------------------------------------- */
        if( strcmp(papszTokens[0],"*Equidistant Conic") == 0 )
        {
            bSuccess = TRUE;
            if( dfP3 == 0.0 )
                dfP3 = 1.0;
            
            GTIFKeySet(hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                       CT_EquidistantConic );
            GTIFKeySet(hGTiff, ProjStdParallelGeoKey, TYPE_DOUBLE, 1, dfP1 );
            GTIFKeySet(hGTiff, ProjStdParallel2GeoKey, TYPE_DOUBLE, 1, dfP2 );
            GTIFKeySet(hGTiff, ProjNatOriginLatGeoKey, TYPE_DOUBLE, 1, dfP3);
            GTIFKeySet(hGTiff, ProjCenterLongGeoKey, TYPE_DOUBLE, 1, dfP4);
            GTIFKeySet(hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1, dfP5 );
            GTIFKeySet(hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1, dfP6 );
        }

/* -------------------------------------------------------------------- */
/*      If we wrote it as a userdefined projection, then write out      */
/*      the ProjectionGeoKey as well.                                   */
/* -------------------------------------------------------------------- */
        if( bSuccess )
        {
            GTIFKeySet(hGTiff, ProjectionGeoKey, TYPE_SHORT, 1,
                       KvUserDefined );
            GTIFKeySet(hGTiff, ProjectedCSTypeGeoKey, TYPE_SHORT,  1,
                       KvUserDefined );	
        }

    }

    CSLDestroy( papszTokens );
    
    GTIFWriteKeys( hGTiff );
    GTIFFree( hGTiff );

    return bSuccess;
}


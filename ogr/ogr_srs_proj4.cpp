/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGRSpatialReference interface to PROJ.4.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
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
 * Revision 1.12  2000/09/25 20:22:21  warmerda
 * ensure pszPROJ4Units is initialized
 *
 * Revision 1.11  2000/09/25 15:46:56  warmerda
 * added sphere
 *
 * Revision 1.10  2000/08/28 20:13:23  warmerda
 * added importFromProj4
 *
 * Revision 1.9  2000/07/11 01:02:06  warmerda
 * added ExportToProj4()
 *
 * Revision 1.8  2000/07/09 20:49:21  warmerda
 * added +datum support
 *
 * Revision 1.7  2000/03/06 02:23:54  warmerda
 * don't use +datum syntax
 *
 * Revision 1.6  1999/12/22 15:39:43  warmerda
 * fix to differentiate WGS variants
 *
 * Revision 1.5  1999/12/13 16:29:59  warmerda
 * Added improved units, and ellipse support.
 *
 * Revision 1.4  1999/12/08 16:34:05  warmerda
 * added five or six more projections
 *
 * Revision 1.1  1999/07/29 17:29:15  warmerda
 * New
 *
 */

#include "ogr_spatialref.h"
#include "ogr_p.h"

/* -------------------------------------------------------------------- */
/*      The following list comes from osrs/proj/src/pj_ellps.c          */
/*      ... please update from time to time.                            */
/* -------------------------------------------------------------------- */
static const char *ogr_pj_ellps[] = {
"MERIT",	"a=6378137.0", "rf=298.257", "MERIT 1983",
"SGS85",	"a=6378136.0", "rf=298.257",  "Soviet Geodetic System 85",
"GRS80",	"a=6378137.0", "rf=298.257222101", "GRS 1980(IUGG, 1980)",
"IAU76",	"a=6378140.0", "rf=298.257", "IAU 1976",
"airy",		"a=6377563.396", "b=6356256.910", "Airy 1830",
"APL4.9",	"a=6378137.0.",  "rf=298.25", "Appl. Physics. 1965",
"NWL9D",	"a=6378145.0.",  "rf=298.25", "Naval Weapons Lab., 1965",
"mod_airy",	"a=6377340.189", "b=6356034.446", "Modified Airy",
"andrae",	"a=6377104.43",  "rf=300.0", 	"Andrae 1876 (Den., Iclnd.)",
"aust_SA",	"a=6378160.0", "rf=298.25", "Australian Natl & S. Amer. 1969",
"GRS67",	"a=6378160.0", "rf=298.2471674270", "GRS 67(IUGG 1967)",
"bessel",	"a=6377397.155", "rf=299.1528128", "Bessel 1841",
"bess_nam",	"a=6377483.865", "rf=299.1528128", "Bessel 1841 (Namibia)",
"clrk66",	"a=6378206.4", "b=6356583.8", "Clarke 1866",
"clrk80",	"a=6378249.145", "rf=293.4663", "Clarke 1880 mod.",
"CPM",  	"a=6375738.7", "rf=334.29", "Comm. des Poids et Mesures 1799",
"delmbr",	"a=6376428.",  "rf=311.5", "Delambre 1810 (Belgium)",
"engelis",	"a=6378136.05", "rf=298.2566", "Engelis 1985",
"evrst30",  "a=6377276.345", "rf=300.8017",  "Everest 1830",
"evrst48",  "a=6377304.063", "rf=300.8017",  "Everest 1948",
"evrst56",  "a=6377301.243", "rf=300.8017",  "Everest 1956",
"evrst69",  "a=6377295.664", "rf=300.8017",  "Everest 1969",
"evrstSS",  "a=6377298.556", "rf=300.8017",  "Everest (Sabah & Sarawak)",
"fschr60",  "a=6378166.",   "rf=298.3", "Fischer (Mercury Datum) 1960",
"fschr60m", "a=6378155.",   "rf=298.3", "Modified Fischer 1960",
"fschr68",  "a=6378150.",   "rf=298.3", "Fischer 1968",
"helmert",  "a=6378200.",   "rf=298.3", "Helmert 1906",
"hough",	"a=6378270.0", "rf=297.", "Hough",
"intl",		"a=6378388.0", "rf=297.", "International 1909 (Hayford)",
"krass",	"a=6378245.0", "rf=298.3", "Krassovsky, 1942",
"kaula",	"a=6378163.",  "rf=298.24", "Kaula 1961",
"lerch",	"a=6378139.",  "rf=298.257", "Lerch 1979",
"mprts",	"a=6397300.",  "rf=191.", "Maupertius 1738",
"new_intl",	"a=6378157.5", "b=6356772.2", "New International 1967",
"plessis",	"a=6376523.",  "b=6355863.", "Plessis 1817 (France)",
"SEasia",	"a=6378155.0", "b=6356773.3205", "Southeast Asia",
"walbeck",	"a=6376896.0", "b=6355834.8467", "Walbeck",
"WGS60",    "a=6378165.0",  "rf=298.3", "WGS 60",
"WGS66",	"a=6378145.0", "rf=298.25", "WGS 66",
"WGS72",	"a=6378135.0", "rf=298.26", "WGS 72",
"WGS84",    "a=6378137.0",  "rf=298.257223563", "WGS 84",
"sphere",   "a=6370997.0",  "b=6370997.0", "Normal Sphere (r=6370997)",
0, 0, 0, 0,
};

/************************************************************************/
/*                         OSRImportFromProj4()                         */
/************************************************************************/

OGRErr OSRImportFromProj4( OGRSpatialReferenceH hSRS, const char *pszProj4 )

{
    return ((OGRSpatialReference *) hSRS)->importFromProj4( pszProj4 );
}

/************************************************************************/
/*                              OSR_GDV()                               */
/*                                                                      */
/*      Fetch a particular parameter out of the parameter list, or      */
/*      the indicated default if it isn't available.  This is a         */
/*      helper function for importFromProj4().                          */
/************************************************************************/

static double OSR_GDV( char **papszNV, const char * pszField, 
                       double dfDefaultValue )

{
    const char * pszValue;

    pszValue = CSLFetchNameValue( papszNV, pszField );
    if( pszValue == NULL )
        return dfDefaultValue;
    else
        return atof(pszValue);
}

/************************************************************************/
/*                          importFromProj4()                           */
/************************************************************************/

OGRErr OGRSpatialReference::importFromProj4( const char * pszProj4 )

{
    char **papszNV = NULL;
    char **papszTokens;
    int  i;

/* -------------------------------------------------------------------- */
/*      Parse the PROJ.4 string into a cpl_string.h style name/value    */
/*      list.                                                           */
/* -------------------------------------------------------------------- */
    papszTokens = CSLTokenizeStringComplex( pszProj4, "+ ", TRUE, FALSE );
    
    for( i = 0; papszTokens != NULL && papszTokens[i] != NULL; i++ )
    {
        char *pszEqual = strstr(papszTokens[i],"=");

        if( pszEqual == NULL )
            papszNV = CSLAddNameValue(papszNV, papszTokens[i], "" );
        else
        {
            pszEqual[0] = '\0';
            papszNV = CSLAddNameValue( papszNV, papszTokens[i], pszEqual+1 );
        }
    }

    CSLDestroy( papszTokens );

/* -------------------------------------------------------------------- */
/*      Operate on the basis of the projection name.                    */
/* -------------------------------------------------------------------- */
    const char *pszProj = CSLFetchNameValue(papszNV,"proj");

    if( pszProj == NULL )
    {
        CPLDebug( "OGR_PROJ4", "Can't find +proj= in:\n%s", pszProj4 );
        return OGRERR_CORRUPT_DATA;
    }

    else if( EQUAL(pszProj,"longlat") || EQUAL(pszProj,"latlong") )
    {
    }
    
    else if( EQUAL(pszProj,"cea") )
    {
        SetCEA( OSR_GDV( papszNV, "lat_ts", 0.0 ), 
                OSR_GDV( papszNV, "lon_0", 0.0 ), 
                OSR_GDV( papszNV, "x_0", 0.0 ), 
                OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"tmerc") )
    {
        SetTM( OSR_GDV( papszNV, "lat_0", 0.0 ), 
               OSR_GDV( papszNV, "lon_0", 0.0 ), 
               OSR_GDV( papszNV, "k", 1.0 ), 
               OSR_GDV( papszNV, "x_0", 0.0 ), 
               OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"utm") )
    {
        SetUTM( (int) OSR_GDV( papszNV, "zone", 0.0 ),
                (int) OSR_GDV( papszNV, "south", 1.0 ) );
    }

    else if( EQUAL(pszProj,"merc") )
    {
        SetMercator( OSR_GDV( papszNV, "lat_ts", 0.0 ), 
                     OSR_GDV( papszNV, "lon_0", 0.0 ), 
                     OSR_GDV( papszNV, "k", 1.0 ), 
                     OSR_GDV( papszNV, "x_0", 0.0 ), 
                     OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"stere") 
             && CSLFetchNameValue(papszNV,"k") != NULL )
    {
        SetOS( OSR_GDV( papszNV, "lat_0", 0.0 ), 
               OSR_GDV( papszNV, "lon_0", 0.0 ), 
               OSR_GDV( papszNV, "k", 1.0 ), 
               OSR_GDV( papszNV, "x_0", 0.0 ), 
               OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"stere") )
    {
        /* Should we be able to distinguish polar stereographic? */
        SetStereographic( OSR_GDV( papszNV, "lat_0", 0.0 ), 
                          OSR_GDV( papszNV, "lon_0", 0.0 ), 
                          1.0, 
                          OSR_GDV( papszNV, "x_0", 0.0 ), 
                          OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"eqc") )
    {
        SetEquirectangular( OSR_GDV( papszNV, "lat_ts", 0.0 ), 
                            OSR_GDV( papszNV, "lon_0", 0.0 ), 
                            OSR_GDV( papszNV, "x_0", 0.0 ), 
                            OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"gnom") )
    {
        SetGnomonic( OSR_GDV( papszNV, "lat_0", 0.0 ), 
                     OSR_GDV( papszNV, "lon_0", 0.0 ), 
                     OSR_GDV( papszNV, "x_0", 0.0 ), 
                     OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"ortho") )
    {
        SetOrthographic( OSR_GDV( papszNV, "lat_0", 0.0 ), 
                         OSR_GDV( papszNV, "lon_0", 0.0 ), 
                         OSR_GDV( papszNV, "x_0", 0.0 ), 
                         OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"laea") )
    {
        SetLAEA( OSR_GDV( papszNV, "lat_0", 0.0 ), 
                 OSR_GDV( papszNV, "lon_0", 0.0 ), 
                 OSR_GDV( papszNV, "x_0", 0.0 ), 
                 OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"aeqd") )
    {
        SetAE( OSR_GDV( papszNV, "lat_0", 0.0 ), 
               OSR_GDV( papszNV, "lon_0", 0.0 ), 
               OSR_GDV( papszNV, "x_0", 0.0 ), 
               OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"eqdc") )
    {
        SetEC( OSR_GDV( papszNV, "lat_1", 0.0 ), 
               OSR_GDV( papszNV, "lat_2", 0.0 ), 
               OSR_GDV( papszNV, "lat_0", 0.0 ), 
               OSR_GDV( papszNV, "lon_0", 0.0 ), 
               OSR_GDV( papszNV, "x_0", 0.0 ), 
               OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"mill") )
    {
        SetMC( OSR_GDV( papszNV, "lat_0", 0.0 ), 
               OSR_GDV( papszNV, "lon_0", 0.0 ), 
               OSR_GDV( papszNV, "x_0", 0.0 ), 
               OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"moll") )
    {
        SetMollweide( OSR_GDV( papszNV, "lon_0", 0.0 ), 
                      OSR_GDV( papszNV, "x_0", 0.0 ), 
                      OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"eck4") )
    {
        SetEckertIV( OSR_GDV( papszNV, "lon_0", 0.0 ), 
                     OSR_GDV( papszNV, "x_0", 0.0 ), 
                     OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"eck6") )
    {
        SetEckertVI( OSR_GDV( papszNV, "lon_0", 0.0 ), 
                     OSR_GDV( papszNV, "x_0", 0.0 ), 
                     OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"poly") )
    {
        SetPolyconic( OSR_GDV( papszNV, "lat_0", 0.0 ), 
                      OSR_GDV( papszNV, "lon_0", 0.0 ), 
                      OSR_GDV( papszNV, "x_0", 0.0 ), 
                      OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"aea") )
    {
        SetACEA( OSR_GDV( papszNV, "lat_1", 0.0 ), 
                 OSR_GDV( papszNV, "lat_2", 0.0 ), 
                 OSR_GDV( papszNV, "lat_0", 0.0 ), 
                 OSR_GDV( papszNV, "lon_0", 0.0 ), 
                 OSR_GDV( papszNV, "x_0", 0.0 ), 
                 OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"robin") )
    {
        SetRobinson( OSR_GDV( papszNV, "lon_0", 0.0 ), 
                     OSR_GDV( papszNV, "x_0", 0.0 ), 
                     OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"vandg") )
    {
        SetVDG( OSR_GDV( papszNV, "lon_0", 0.0 ), 
                OSR_GDV( papszNV, "x_0", 0.0 ), 
                OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"sinu") )
    {
        SetSinusoidal( OSR_GDV( papszNV, "lon_0", 0.0 ), 
                       OSR_GDV( papszNV, "x_0", 0.0 ), 
                       OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"gall") )
    {
        SetSinusoidal( OSR_GDV( papszNV, "lon_0", 0.0 ), 
                       OSR_GDV( papszNV, "x_0", 0.0 ), 
                       OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"lcc") ) /* 2SP form */
    {
        SetLCC( OSR_GDV( papszNV, "lat_1", 0.0 ), 
                OSR_GDV( papszNV, "lat_2", 0.0 ), 
                OSR_GDV( papszNV, "lat_0", 0.0 ), 
                OSR_GDV( papszNV, "lon_0", 0.0 ), 
                OSR_GDV( papszNV, "x_0", 0.0 ), 
                OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"omerc") )
    {
        SetHOM( OSR_GDV( papszNV, "lat_0", 0.0 ), 
                OSR_GDV( papszNV, "lonc", 0.0 ), 
                OSR_GDV( papszNV, "alpha", 0.0 ), 
                0.0, /* ??? */
                OSR_GDV( papszNV, "k", 1.0 ), 
                OSR_GDV( papszNV, "x_0", 0.0 ), 
                OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else
    {
        CPLDebug( "OGR_PROJ4", "Unsupported projection: %s", pszProj );
        return OGRERR_CORRUPT_DATA;
    }

/* -------------------------------------------------------------------- */
/*      Try to translate the datum.                                     */
/* -------------------------------------------------------------------- */
    const char *pszValue;
    int  bFullyDefined = FALSE;

    pszValue = CSLFetchNameValue(papszNV, "datum");
    if( pszValue == NULL )
    {
        /* do nothing */
    }
    else if( EQUAL(pszValue,"NAD27") || EQUAL(pszValue,"NAD83")
             || EQUAL(pszValue,"WGS84") || EQUAL(pszValue,"WGS72") )
    {
        SetWellKnownGeogCS( pszValue );
        bFullyDefined = TRUE;
    }
    else
    {
        /* we don't recognise the datum, and ignore it */
    }

/* -------------------------------------------------------------------- */
/*      Set the ellipsoid information.				         */
/* -------------------------------------------------------------------- */
    double dfSemiMajor, dfInvFlattening, dfSemiMinor;

    pszValue = CSLFetchNameValue(papszNV, "ellps");
    if( pszValue != NULL && !bFullyDefined )
    {
        for( i = 0; ogr_pj_ellps[i] != NULL; i += 4 )
        {

            if( !EQUAL(ogr_pj_ellps[i],pszValue) )
                continue;

            CPLAssert( EQUALN(ogr_pj_ellps[i+1],"a=",2) );
            
            dfSemiMajor = atof(ogr_pj_ellps[i+1]+2);
            if( EQUALN(ogr_pj_ellps[i+2],"rf=",3) )
                dfInvFlattening = atof(ogr_pj_ellps[i+2]+3);
            else
            {
                CPLAssert( EQUALN(ogr_pj_ellps[i+2],"b=",2) );
                dfSemiMinor = atof(ogr_pj_ellps[i+2]+2);
                
                if( ABS(dfSemiMajor/dfSemiMinor) - 1.0 < 0.0000000000001 )
                    dfInvFlattening = 0.0;
                else
                    dfInvFlattening = -1.0 / (dfSemiMinor/dfSemiMajor - 1.0);
            }
            
            SetGeogCS( ogr_pj_ellps[i+3], "unknown", ogr_pj_ellps[i], 
                       dfSemiMajor, dfInvFlattening );

            bFullyDefined = TRUE;
            break;
        }
    }

    if( !bFullyDefined )
    {
        dfSemiMajor = OSR_GDV( papszNV, "a", 0.0 );
        if( dfSemiMajor == 0.0 )
        {
            CPLDebug( "OGR_PROJ4", "Can't find ellipse definition in:\n%s", 
                      pszProj4 );
            return OGRERR_UNSUPPORTED_SRS;
        }
        
        dfSemiMinor = OSR_GDV( papszNV, "b", -1.0 );
        dfInvFlattening = OSR_GDV( papszNV, "rf", -1.0 );
        
        if( dfSemiMinor == -1.0 && dfInvFlattening == -1.0 )
        {
            CPLDebug( "OGR_PROJ4", "Can't find ellipse definition in:\n%s", 
                      pszProj4 );
            return OGRERR_UNSUPPORTED_SRS;
        }

        if( dfInvFlattening == -1.0 )
        {
            if( ABS(dfSemiMajor/dfSemiMinor) - 1.0 < 0.0000000000001 )
                dfInvFlattening = 0.0;
            else
                dfInvFlattening = -1.0 / (dfSemiMinor/dfSemiMajor - 1.0);
        }
        
        SetGeogCS( "unnamed ellipse", "unknown", "unnamed",
                   dfSemiMajor, dfInvFlattening );
        
        bFullyDefined = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Linear units translation                                        */
/* -------------------------------------------------------------------- */
    /* add here */
    
    return OGRERR_NONE;
}


/************************************************************************/
/*                          OSRExportToProj4()                          */
/************************************************************************/

OGRErr OSRExportToProj4( OGRSpatialReferenceH hSRS, char ** ppszReturn )

{
    *ppszReturn = NULL;

    return ((OGRSpatialReference *) hSRS)->exportToProj4( ppszReturn );
}

/************************************************************************/
/*                           exportToProj4()                            */
/************************************************************************/

OGRErr OGRSpatialReference::exportToProj4( char ** ppszProj4 )

{
    char        szProj4[512];
    const char *pszProjection = GetAttrValue("PROJECTION");

    szProj4[0] = '\0';

/* ==================================================================== */
/*      Handle the projection definition.                               */
/* ==================================================================== */

    if( pszProjection == NULL )
    {
        sprintf( szProj4+strlen(szProj4), "+proj=longlat " );
    }
    else if( EQUAL(pszProjection,SRS_PT_CYLINDRICAL_EQUAL_AREA) )
    {
        sprintf( szProj4+strlen(szProj4),
           "+proj=cea +lon_0=%.9f +lat_ts=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_TRANSVERSE_MERCATOR) )
    {
        int bNorth;
        int nZone = GetUTMZone( &bNorth );

        if( nZone != 0 )
        {
            if( bNorth )
                sprintf( szProj4+strlen(szProj4), "+proj=utm +zone=%d ", 
                         nZone );
            else
                sprintf( szProj4+strlen(szProj4),"+proj=utm +zone=%d +south ", 
                         nZone );
        }            
        else
            sprintf( szProj4+strlen(szProj4),
             "+proj=tmerc +lat_0=%.9f +lon_0=%.9f +k=%f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),

                 GetProjParm(SRS_PP_SCALE_FACTOR,1.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_MERCATOR_1SP) )
    {
        sprintf( szProj4+strlen(szProj4),
           "+proj=merc +lat_ts=%.9f +lon_0=%.9f +k=%f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_SCALE_FACTOR,1.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_OBLIQUE_STEREOGRAPHIC) )
    {
        sprintf( szProj4+strlen(szProj4),
           "+proj=stere +lat_0=%.9f +lon_0=%.9f +k=%f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_SCALE_FACTOR,1.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_STEREOGRAPHIC) )
    {
        sprintf( szProj4+strlen(szProj4),
           "+proj=stere +lat_0=%.9f +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_POLAR_STEREOGRAPHIC) )
    {
        /* note we are ignore the scale factory handled by SetPS() */
        
        sprintf( szProj4+strlen(szProj4),
                 "+proj=stere +lat_0=%.9f +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_EQUIRECTANGULAR) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=eqc +lat_ts=%.9f +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_GNOMONIC) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=gnom +lat_0=%.9f +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_ORTHOGRAPHIC) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=ortho +lat_0=%.9f +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=laea +lat_0=%.9f +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_AZIMUTHAL_EQUIDISTANT) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=aeqd +lat_0=%.9f +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_EQUIDISTANT_CONIC) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=eqdc +lat_0=%.9f +lon_0=%.9f +lat_1=%.9f +lat_2=%.9f"
                 " +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LATITUDE_OF_CENTER,0.0),
                 GetProjParm(SRS_PP_LONGITUDE_OF_CENTER,0.0),
                 GetProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0),
                 GetProjParm(SRS_PP_STANDARD_PARALLEL_2,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_MILLER_CYLINDRICAL) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=mill +lat_0=%.9f +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_MOLLWEIDE) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=moll +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_ECKERT_IV) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=eck4 +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_ECKERT_VI) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=eck6 +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_POLYCONIC) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=poly +lat_0=%.9f +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_ALBERS_CONIC_EQUAL_AREA) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=aea +lat_1=%.9f +lat_2=%.9f +lat_0=%.9f +lon_0=%.9f"
                 " +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0),
                 GetProjParm(SRS_PP_STANDARD_PARALLEL_2,0.0),
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_ROBINSON) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=robin +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_VANDERGRINTEN) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=vandg +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_SINUSOIDAL) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=sinu +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LONGITUDE_OF_CENTER,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_GALL_STEREOGRAPHIC) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=gall +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP)
         || EQUAL(pszProjection,SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=lcc +lat_1=%.9f +lat_2=%.9f +lat_0=%.9f +lon_0=%.9f"
                 " +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0),
                 GetProjParm(SRS_PP_STANDARD_PARALLEL_2,0.0),
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }
    
    else if( EQUAL(pszProjection,SRS_PT_HOTINE_OBLIQUE_MERCATOR) )
    {
        /* not clear how ProjParm[3] - angle from rectified to skewed grid -
           should be applied ... see the +not_rot flag for PROJ.4.
           Just ignoring for now. */

        sprintf( szProj4+strlen(szProj4),
                 "+proj=omerc +lat_0=%.9f +lonc=%.9f +alpha=%.9f"
                 " +k=%.9f +x_0=%.3f +y_0=%.3f ",
                 GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetProjParm(SRS_PP_AZIMUTH,0.0),
                 GetProjParm(SRS_PP_SCALE_FACTOR,1.0),
                 GetProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

/* -------------------------------------------------------------------- */
/*      Handle earth model.  For now we just always emit the user       */
/*      defined ellipsoid parameters.                                   */
/* -------------------------------------------------------------------- */
    double	dfSemiMajor = GetSemiMajor();
    double	dfInvFlattening = GetInvFlattening();
    const char 	*pszPROJ4Ellipse = NULL;
    const char  *pszDatum = GetAttrValue("DATUM");

    if( ABS(dfSemiMajor-6378249.145) < 0.01
        && ABS(dfInvFlattening-293.465) < 0.0001 )
    {
        pszPROJ4Ellipse = "clrk80";	/* Clark 1880 */
    }
    else if( ABS(dfSemiMajor-6378245.0) < 0.01
             && ABS(dfInvFlattening-298.3) < 0.0001 )
    {
        pszPROJ4Ellipse = "krass";	/* Krassovsky */
    }
    else if( ABS(dfSemiMajor-6378388.0) < 0.01
             && ABS(dfInvFlattening-297.0) < 0.0001 )
    {
        pszPROJ4Ellipse = "intl"; 	/* International 1924 */
    }
    else if( ABS(dfSemiMajor-6378160.0) < 0.01
             && ABS(dfInvFlattening-298.25) < 0.0001 )
    {
        pszPROJ4Ellipse = "aust_SA"; 	/* Australian */
    }
    else if( ABS(dfSemiMajor-6377397.155) < 0.01
             && ABS(dfInvFlattening-299.1528128) < 0.0001 )
    {
        pszPROJ4Ellipse = "bessel";	/* Bessel 1841 */
    }
    else if( ABS(dfSemiMajor-6377483.865) < 0.01
             && ABS(dfInvFlattening-299.1528128) < 0.0001 )
    {
        pszPROJ4Ellipse = "bess_nam";	/* Bessel 1841 (Namibia / Schwarzeck)*/
    }
    else if( ABS(dfSemiMajor-6378160.0) < 0.01
             && ABS(dfInvFlattening-298.247167427) < 0.0001 )
    {
        pszPROJ4Ellipse = "GRS67";	/* GRS 1967 */
    }
    else if( ABS(dfSemiMajor-6378137) < 0.01
             && ABS(dfInvFlattening-298.257222101) < 0.000001 )
    {
        pszPROJ4Ellipse = "GRS80";	/* GRS 1980 */
    }
    else if( ABS(dfSemiMajor-6378206.4) < 0.01
             && ABS(dfInvFlattening-294.9786982) < 0.0001 )
    {
        pszPROJ4Ellipse = "clrk66";	/* Clarke 1866 */
    }
    else if( ABS(dfSemiMajor-6378206.4) < 0.01
             && ABS(dfInvFlattening-294.9786982) < 0.0001 )
    {
        pszPROJ4Ellipse = "mod_airy";	/* Modified Airy */
    }
    else if( ABS(dfSemiMajor-6377563.396) < 0.01
             && ABS(dfInvFlattening-299.3249646) < 0.0001 )
    {
        pszPROJ4Ellipse = "airy";	/* Modified Airy */
    }
    else if( ABS(dfSemiMajor-6378200) < 0.01
             && ABS(dfInvFlattening-298.3) < 0.0001 )
    {
        pszPROJ4Ellipse = "helmert";	/* Helmert 1906 */
    }
    else if( ABS(dfSemiMajor-6378155) < 0.01
             && ABS(dfInvFlattening-298.3) < 0.0001 )
    {
        pszPROJ4Ellipse = "fschr60m";	/* Modified Fischer 1960 */
    }
    else if( ABS(dfSemiMajor-6377298.556) < 0.01
             && ABS(dfInvFlattening-300.8017) < 0.0001 )
    {
        pszPROJ4Ellipse = "evrstSS";	/* Everest (Sabah & Sarawak) */
    }
    else if( ABS(dfSemiMajor-6378165.0) < 0.01
             && ABS(dfInvFlattening-298.3) < 0.0001 )
    {
        pszPROJ4Ellipse = "WGS60";	
    }
    else if( ABS(dfSemiMajor-6378145.0) < 0.01
             && ABS(dfInvFlattening-298.25) < 0.0001 )
    {
        pszPROJ4Ellipse = "WGS66";	
    }
    else if( ABS(dfSemiMajor-6378135.0) < 0.01
             && ABS(dfInvFlattening-298.26) < 0.0001 )
    {
        pszPROJ4Ellipse = "WGS72";	
    }
    else if( ABS(dfSemiMajor-6378137.0) < 0.01
             && ABS(dfInvFlattening-298.257223563) < 0.000001 )
    {
        pszPROJ4Ellipse = "WGS84";
    }
    else if( EQUAL(pszDatum,"North_American_Datum_1927") )
    {
//        pszPROJ4Ellipse = "clrk66:+datum=nad27"; /* NAD 27 */
        pszPROJ4Ellipse = "clrk66";
    }
    else if( EQUAL(pszDatum,"North_American_Datum_1983") )
    {
//        pszPROJ4Ellipse = "GRS80:+datum=nad83";	/* NAD 83 */
        pszPROJ4Ellipse = "GRS80";
    }
    
    if( pszPROJ4Ellipse == NULL )
        sprintf( szProj4+strlen(szProj4), "+a=%.3f +b=%.3f ",
                 GetSemiMajor(), GetSemiMinor() );
    else
        sprintf( szProj4+strlen(szProj4), "+ellps=%s ",
                 pszPROJ4Ellipse );

/* -------------------------------------------------------------------- */
/*      Translate the datum.                                            */
/* -------------------------------------------------------------------- */
    const char *pszPROJ4Datum = NULL;
    OGR_SRSNode *poTOWGS84 = GetAttrNode( "TOWGS84" );
    char  szTOWGS84[256];

    if( pszDatum == NULL )
        /* nothing */;

    else if( EQUAL(pszDatum,"North_American_Datum_1927") )
        pszPROJ4Datum = "+datum=NAD27";

    else if( EQUAL(pszDatum,"North_American_Datum_1983") )
        pszPROJ4Datum = "+datum=NAD83";

    else if( EQUAL(pszDatum,"WGS_1984") )
        pszPROJ4Datum = "+datum=WGS84";

    else if( poTOWGS84 != NULL )
    {
        if( poTOWGS84->GetChildCount() > 2
            && (poTOWGS84->GetChildCount() < 6 
                || EQUAL(poTOWGS84->GetChild(3)->GetValue(),"")
                && EQUAL(poTOWGS84->GetChild(4)->GetValue(),"")
                && EQUAL(poTOWGS84->GetChild(5)->GetValue(),"")
                && EQUAL(poTOWGS84->GetChild(6)->GetValue(),"")) )
        {
            sprintf( szTOWGS84, "+towgs84=%s,%s,%s",
                     poTOWGS84->GetChild(0)->GetValue(),
                     poTOWGS84->GetChild(1)->GetValue(),
                     poTOWGS84->GetChild(2)->GetValue() );
            pszPROJ4Datum = szTOWGS84;
        }
        else if( poTOWGS84->GetChildCount() > 6 )
        {
            sprintf( szTOWGS84, "+towgs84=%s,%s,%s,%s,%s,%s,%s",
                     poTOWGS84->GetChild(0)->GetValue(),
                     poTOWGS84->GetChild(1)->GetValue(),
                     poTOWGS84->GetChild(2)->GetValue(),
                     poTOWGS84->GetChild(3)->GetValue(),
                     poTOWGS84->GetChild(4)->GetValue(),
                     poTOWGS84->GetChild(5)->GetValue(),
                     poTOWGS84->GetChild(6)->GetValue() );
            pszPROJ4Datum = szTOWGS84;
        }
    }
    
    if( pszPROJ4Datum != NULL )
    {
        strcat( szProj4, pszPROJ4Datum );
        strcat( szProj4, " " );
    }
    
/* -------------------------------------------------------------------- */
/*      Handle linear units.                                            */
/* -------------------------------------------------------------------- */
    const char	*pszPROJ4Units=NULL;
    char  	*pszLinearUnits = NULL;
    double	dfLinearConv;

    dfLinearConv = GetLinearUnits( &pszLinearUnits );
        
    if( strstr(szProj4,"longlat") != NULL )
        pszPROJ4Units = NULL;
    
    else if( dfLinearConv == 1.0 )
        pszPROJ4Units = "m";

    else if( dfLinearConv == 1000.0 )
        pszPROJ4Units = "km";
    
    else if( dfLinearConv == 0.0254 )
        pszPROJ4Units = "in";
    
    else if( EQUAL(pszLinearUnits,SRS_UL_FOOT) )
        pszPROJ4Units = "ft";
    
    else if( EQUAL(pszLinearUnits,"IYARD") || dfLinearConv == 0.9144 )
        pszPROJ4Units = "yd";
    
    else if( dfLinearConv == 0.001 )
        pszPROJ4Units = "mm";
    
    else if( dfLinearConv == 0.01 )
        pszPROJ4Units = "cm";

    else if( EQUAL(pszLinearUnits,SRS_UL_US_FOOT) )
        pszPROJ4Units = "us-ft";

    else if( EQUAL(pszLinearUnits,SRS_UL_NAUTICAL_MILE) )
        pszPROJ4Units = "kmi";

    else if( EQUAL(pszLinearUnits,"Mile") 
             || EQUAL(pszLinearUnits,"IMILE") )
        pszPROJ4Units = "mi";

    else
    {
        sprintf( szProj4+strlen(szProj4), "+to_meter=+%.10f ",
                 dfLinearConv );
    }

    if( pszPROJ4Units != NULL )
        sprintf( szProj4+strlen(szProj4), "+units=%s ",
                 pszPROJ4Units );

    *ppszProj4 = CPLStrdup( szProj4 );

    return OGRERR_NONE;
}


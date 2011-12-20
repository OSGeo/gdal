/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGRSpatialReference interface to PROJ.4.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
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
 ****************************************************************************/

#include "ogr_spatialref.h"
#include "ogr_p.h"
#include "cpl_conv.h"

extern int EPSGGetWGS84Transform( int nGeogCS, double *padfTransform );

CPL_CVSID("$Id$");

/* -------------------------------------------------------------------- */
/*      The following list comes from osrs/proj/src/pj_ellps.c          */
/*      ... please update from time to time.                            */
/* -------------------------------------------------------------------- */
static const char *ogr_pj_ellps[] = {
"MERIT",        "a=6378137.0", "rf=298.257", "MERIT 1983",
"SGS85",        "a=6378136.0", "rf=298.257",  "Soviet Geodetic System 85",
"GRS80",        "a=6378137.0", "rf=298.257222101", "GRS 1980(IUGG, 1980)",
"IAU76",        "a=6378140.0", "rf=298.257", "IAU 1976",
"airy",         "a=6377563.396", "b=6356256.910", "Airy 1830",
"APL4.9",       "a=6378137.0.",  "rf=298.25", "Appl. Physics. 1965",
"NWL9D",        "a=6378145.0.",  "rf=298.25", "Naval Weapons Lab., 1965",
"mod_airy",     "a=6377340.189", "b=6356034.446", "Modified Airy",
"andrae",       "a=6377104.43",  "rf=300.0",    "Andrae 1876 (Den., Iclnd.)",
"aust_SA",      "a=6378160.0", "rf=298.25", "Australian Natl & S. Amer. 1969",
"GRS67",        "a=6378160.0", "rf=298.2471674270", "GRS 67(IUGG 1967)",
"bessel",       "a=6377397.155", "rf=299.1528128", "Bessel 1841",
"bess_nam",     "a=6377483.865", "rf=299.1528128", "Bessel 1841 (Namibia)",
"clrk66",       "a=6378206.4", "b=6356583.8", "Clarke 1866",
"clrk80",       "a=6378249.145", "rf=293.4663", "Clarke 1880 mod.",
"CPM",          "a=6375738.7", "rf=334.29", "Comm. des Poids et Mesures 1799",
"delmbr",       "a=6376428.",  "rf=311.5", "Delambre 1810 (Belgium)",
"engelis",      "a=6378136.05", "rf=298.2566", "Engelis 1985",
"evrst30",  "a=6377276.345", "rf=300.8017",  "Everest 1830",
"evrst48",  "a=6377304.063", "rf=300.8017",  "Everest 1948",
"evrst56",  "a=6377301.243", "rf=300.8017",  "Everest 1956",
"evrst69",  "a=6377295.664", "rf=300.8017",  "Everest 1969",
"evrstSS",  "a=6377298.556", "rf=300.8017",  "Everest (Sabah & Sarawak)",
"fschr60",  "a=6378166.",   "rf=298.3", "Fischer (Mercury Datum) 1960",
"fschr60m", "a=6378155.",   "rf=298.3", "Modified Fischer 1960",
"fschr68",  "a=6378150.",   "rf=298.3", "Fischer 1968",
"helmert",  "a=6378200.",   "rf=298.3", "Helmert 1906",
"hough",        "a=6378270.0", "rf=297.", "Hough",
"intl",         "a=6378388.0", "rf=297.", "International 1909 (Hayford)",
"krass",        "a=6378245.0", "rf=298.3", "Krassovsky, 1942",
"kaula",        "a=6378163.",  "rf=298.24", "Kaula 1961",
"lerch",        "a=6378139.",  "rf=298.257", "Lerch 1979",
"mprts",        "a=6397300.",  "rf=191.", "Maupertius 1738",
"new_intl",     "a=6378157.5", "b=6356772.2", "New International 1967",
"plessis",      "a=6376523.",  "b=6355863.", "Plessis 1817 (France)",
"SEasia",       "a=6378155.0", "b=6356773.3205", "Southeast Asia",
"walbeck",      "a=6376896.0", "b=6355834.8467", "Walbeck",
"WGS60",    "a=6378165.0",  "rf=298.3", "WGS 60",
"WGS66",        "a=6378145.0", "rf=298.25", "WGS 66",
"WGS72",        "a=6378135.0", "rf=298.26", "WGS 72",
"WGS84",    "a=6378137.0",  "rf=298.257223563", "WGS 84",
"sphere",   "a=6370997.0",  "b=6370997.0", "Normal Sphere (r=6370997)",
0, 0, 0, 0,
};

typedef struct
{
    const char* pszPJ;
    const char* pszOGR;
    int         nEPSG;
    int         nGCS;
} OGRProj4Datum;

/* Derived from proj/src/pj_datum.c */
/* WGS84, NAD27 and NAD83 are directly hard-coded in the code */
static const OGRProj4Datum ogr_pj_datums[] = {
    { "GGRS87", "Greek_Geodetic_Reference_System_1987", 4121, 6121},
    { "potsdam", "Deutsches_Hauptdreiecksnetz", 4314, 6314},
    { "carthage", "Carthage", 4223, 6223},
    { "hermannskogel", "Militar_Geographische_Institut", 4312, 6312},
    { "ire65", "TM65", 4299, 6299},
    { "nzgd49", "New_Zealand_Geodetic_Datum_1949", 4272, 6272},
    { "OSGB36", "OSGB_1936", 4277, 6277}
};

typedef struct
{
    const char* pszProj4PMName;
    const char* pszWKTPMName;
    const char* pszFromGreenwich;
    int         nPMCode;
} OGRProj4PM;

/* Derived from pj_datums.c */
static const OGRProj4PM ogr_pj_pms [] = {
    { "greenwich", "Greenwich", "0dE",               8901 },
    { "lisbon",    "Lisbon",    "9d07'54.862\"W",    8902 },
    { "paris",     "Paris",     "2d20'14.025\"E",    8903 },
    { "bogota",    "Bogota",    "74d04'51.3\"W",     8904 },
    { "madrid",    "Madrid",    "3d41'16.58\"W",     8905 },
    { "rome",      "Rome",      "12d27'8.4\"E",      8906 },
    { "bern",      "Bern",      "7d26'22.5\"E",      8907 },
    { "jakarta",   "Jakarta",   "106d48'27.79\"E",   8908 },
    { "ferro",     "Ferro",     "17d40'W",           8909 },
    { "brussels",  "Brussels",  "4d22'4.71\"E",      8910 },
    { "stockholm", "Stockholm", "18d3'29.8\"E",      8911 },
    { "athens",    "Athens",    "23d42'58.815\"E",   8912 },
    { "oslo",      "Oslo",      "10d43'22.5\"E",     8913 }
};

static const char* OGRGetProj4Datum(const char* pszDatum,
                                    int nEPSGDatum)
{
    unsigned int i;
    for(i=0;i<sizeof(ogr_pj_datums)/sizeof(ogr_pj_datums[0]);i++)
    {
        if (nEPSGDatum == ogr_pj_datums[i].nGCS ||
            EQUAL(pszDatum, ogr_pj_datums[i].pszOGR))
        {
            return ogr_pj_datums[i].pszPJ;
        }
    }
    return NULL;
}

static const OGRProj4PM* OGRGetProj4PMFromProj4Name(const char* pszProj4PMName)
{
    unsigned int i;
    for(i=0;i<sizeof(ogr_pj_pms)/sizeof(ogr_pj_pms[0]);i++)
    {
        if (EQUAL(pszProj4PMName, ogr_pj_pms[i].pszProj4PMName))
        {
            return &ogr_pj_pms[i];
        }
    }
    return NULL;
}

static const OGRProj4PM* OGRGetProj4PMFromCode(int nPMCode)
{
    unsigned int i;
    for(i=0;i<sizeof(ogr_pj_pms)/sizeof(ogr_pj_pms[0]);i++)
    {
        if (nPMCode == ogr_pj_pms[i].nPMCode)
        {
            return &ogr_pj_pms[i];
        }
    }
    return NULL;
}

static const OGRProj4PM* OGRGetProj4PMFromVal(double dfVal)
{
    unsigned int i;
    for(i=0;i<sizeof(ogr_pj_pms)/sizeof(ogr_pj_pms[0]);i++)
    {
        if (fabs(dfVal - CPLDMSToDec(ogr_pj_pms[i].pszFromGreenwich)) < 1e-10)
        {
            return &ogr_pj_pms[i];
        }
    }
    return NULL;
}

/************************************************************************/
/*                          OSRProj4Tokenize()                          */
/*                                                                      */
/*      Custom tokenizing function for PROJ.4 strings.  The main        */
/*      reason we can't just use CSLTokenizeString is to handle         */
/*      strings with a + sign in the exponents of parameter values.     */
/************************************************************************/

char **OSRProj4Tokenize( const char *pszFull )

{
    char *pszStart = NULL;
    char *pszFullWrk;
    char **papszTokens = NULL;
    int  i;

    if( pszFull == NULL )
        return NULL;

    pszFullWrk = CPLStrdup( pszFull );

    for( i=0; pszFullWrk[i] != '\0'; i++ )
    {
        switch( pszFullWrk[i] )
        {
          case '+':
            if( i == 0 || pszFullWrk[i-1] == '\0' )
            {
                if( pszStart != NULL )
                {
                    if( strstr(pszStart,"=") != NULL )
                        papszTokens = CSLAddString( papszTokens, pszStart );
                    else
                    {
                        CPLString osAsBoolean = pszStart;
                        osAsBoolean += "=yes";
                        papszTokens = CSLAddString( papszTokens, osAsBoolean );
                    }
                }
                pszStart = pszFullWrk + i + 1;
            }
            break;

          case ' ':
          case '\t':
          case '\n':
            pszFullWrk[i] = '\0';
            break;

          default:
            break;
        }
    }

    if( pszStart != NULL && strlen(pszStart) > 0 )
        papszTokens = CSLAddString( papszTokens, pszStart );

    CPLFree( pszFullWrk );

    return papszTokens;
}


/************************************************************************/
/*                         OSRImportFromProj4()                         */
/************************************************************************/
/** 
 * \brief Import PROJ.4 coordinate string.
 *
 * This function is the same as OGRSpatialReference::importFromProj4().
 */
OGRErr OSRImportFromProj4( OGRSpatialReferenceH hSRS, const char *pszProj4 )

{
    VALIDATE_POINTER1( hSRS, "OSRImportFromProj4", CE_Failure );

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

    // special hack to use k_0 if available.
    if( pszValue == NULL && EQUAL(pszField,"k") )
        pszValue = CSLFetchNameValue( papszNV, "k_0" );

    if( pszValue == NULL )
        return dfDefaultValue;
    else
        return CPLDMSToDec(pszValue);
}

/************************************************************************/
/*                          importFromProj4()                           */
/************************************************************************/

/**
 * \brief Import PROJ.4 coordinate string.
 *
 * The OGRSpatialReference is initialized from the passed PROJ.4 style
 * coordinate system string.  In addition to many +proj formulations which
 * have OGC equivelents, it is also possible to import "+init=epsg:n" style
 * definitions.  These are passed to importFromEPSG().  Other init strings
 * (such as the state plane zones) are not currently supported.   
 *
 * Example:
 *   pszProj4 = "+proj=utm +zone=11 +datum=WGS84" 
 *
 * Some parameters, such as grids, recognised by PROJ.4 may not be well
 * understood and translated into the OGRSpatialReference model. It is possible
 * to add the +wktext parameter which is a special keyword that OGR recognises
 * as meaning "embed the entire PROJ.4 string in the WKT and use it literally
 * when converting back to PROJ.4 format".
 * 
 * For example:
 * "+proj=nzmg +lat_0=-41 +lon_0=173 +x_0=2510000 +y_0=6023150 +ellps=intl
 *  +units=m +nadgrids=nzgd2kgrid0005.gsb +wktext"
 *
 * will be translated as :
 * \code
 * PROJCS["unnamed",
 *    GEOGCS["International 1909 (Hayford)",
 *        DATUM["unknown",
 *            SPHEROID["intl",6378388,297]],
 *        PRIMEM["Greenwich",0],
 *        UNIT["degree",0.0174532925199433]],
 *    PROJECTION["New_Zealand_Map_Grid"],
 *    PARAMETER["latitude_of_origin",-41],
 *    PARAMETER["central_meridian",173],
 *    PARAMETER["false_easting",2510000],
 *    PARAMETER["false_northing",6023150],
 *    UNIT["Meter",1],
 *    EXTENSION["PROJ4","+proj=nzmg +lat_0=-41 +lon_0=173 +x_0=2510000 
 *               +y_0=6023150 +ellps=intl  +units=m +nadgrids=nzgd2kgrid0005.gsb +wktext"]]
 * \endcode
 *
 * This method is the equivalent of the C function OSRImportFromProj4().
 *
 * @param pszProj4 the PROJ.4 style string. 
 *
 * @return OGRERR_NONE on success or OGRERR_CORRUPT_DATA on failure.
 */

OGRErr OGRSpatialReference::importFromProj4( const char * pszProj4 )

{
    char **papszNV = NULL;
    char **papszTokens;
    int  i;
    char *pszCleanCopy;

/* -------------------------------------------------------------------- */
/*      Clear any existing definition.                                  */
/* -------------------------------------------------------------------- */
    Clear();

/* -------------------------------------------------------------------- */
/*      Strip any newlines or other "funny" stuff that might occur      */
/*      if this string just came from reading a file.                   */
/* -------------------------------------------------------------------- */
    pszCleanCopy = CPLStrdup( pszProj4 );
    for( i = 0; pszCleanCopy[i] != '\0'; i++ )
    {
        if( pszCleanCopy[i] == 10 
            || pszCleanCopy[i] == 13 
            || pszCleanCopy[i] == 9 )
            pszCleanCopy[i] = ' ';
    }

/* -------------------------------------------------------------------- */
/*      Try to normalize the definition.  This should expand +init=     */
/*      clauses and so forth.                                           */
/* -------------------------------------------------------------------- */
    char *pszNormalized;

    pszNormalized = OCTProj4Normalize( pszCleanCopy );
    CPLFree( pszCleanCopy );
    
/* -------------------------------------------------------------------- */
/*      If we have an EPSG based init string, and no existing +proj     */
/*      portion then try to normalize into into a PROJ.4 string.        */
/* -------------------------------------------------------------------- */
    if( strstr(pszNormalized,"init=epsg:") != NULL 
        && strstr(pszNormalized,"proj=") == NULL )
    {
        OGRErr eErr;
        const char *pszNumber = strstr(pszNormalized,"init=epsg:") + 10;

        eErr = importFromEPSG( atoi(pszNumber) );
        if( eErr == OGRERR_NONE )
        {
            CPLFree( pszNormalized );
            return eErr;
        }
    }

/* -------------------------------------------------------------------- */
/*      Parse the PROJ.4 string into a cpl_string.h style name/value    */
/*      list.                                                           */
/* -------------------------------------------------------------------- */
    papszTokens = OSRProj4Tokenize( pszNormalized );
    CPLFree( pszNormalized );
    
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
/*      Extract the prime meridian, if there is one set.                */
/* -------------------------------------------------------------------- */
    const char *pszPM = CSLFetchNameValue( papszNV, "pm" );
    double dfFromGreenwich = 0.0;
    int    nPMCode = -1;

    if( pszPM != NULL )
    {
        const OGRProj4PM* psProj4PM = OGRGetProj4PMFromProj4Name(pszPM);
        if (psProj4PM)
        {
            dfFromGreenwich = CPLDMSToDec(psProj4PM->pszFromGreenwich);
            pszPM = psProj4PM->pszWKTPMName;
            nPMCode = psProj4PM->nPMCode;
        }
        else
        {
            dfFromGreenwich = CPLDMSToDec( pszPM );
            pszPM = "unnamed";
        }
    }
    else
        pszPM = "Greenwich";

/* -------------------------------------------------------------------- */
/*      Operate on the basis of the projection name.                    */
/* -------------------------------------------------------------------- */
    const char *pszProj = CSLFetchNameValue(papszNV,"proj");

    if( pszProj == NULL )
    {
        CPLDebug( "OGR_PROJ4", "Can't find +proj= in:\n%s", pszProj4 );
        CSLDestroy( papszNV );
        return OGRERR_CORRUPT_DATA;
    }

    else if( EQUAL(pszProj,"longlat") || EQUAL(pszProj,"latlong") )
    {
    }
    
    else if( EQUAL(pszProj,"geocent") )
    {
        SetGeocCS( "Geocentric" );
    }
    
    else if( EQUAL(pszProj,"bonne") )
    {
        SetBonne( OSR_GDV( papszNV, "lat_1", 0.0 ), 
                  OSR_GDV( papszNV, "lon_0", 0.0 ), 
                  OSR_GDV( papszNV, "x_0", 0.0 ), 
                  OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"cass") )
    {
        SetCS( OSR_GDV( papszNV, "lat_0", 0.0 ), 
               OSR_GDV( papszNV, "lon_0", 0.0 ), 
               OSR_GDV( papszNV, "x_0", 0.0 ), 
               OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"nzmg") )
    {
        SetNZMG( OSR_GDV( papszNV, "lat_0", -41.0 ), 
                 OSR_GDV( papszNV, "lon_0", 173.0 ), 
                 OSR_GDV( papszNV, "x_0", 2510000.0 ), 
                 OSR_GDV( papszNV, "y_0", 6023150.0 ) );
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
        const char *pszAxis = CSLFetchNameValue( papszNV, "axis" );

        if( pszAxis == NULL || !EQUAL(pszAxis,"wsu") )
            SetTM( OSR_GDV( papszNV, "lat_0", 0.0 ), 
                   OSR_GDV( papszNV, "lon_0", 0.0 ), 
                   OSR_GDV( papszNV, "k", 1.0 ), 
                   OSR_GDV( papszNV, "x_0", 0.0 ), 
                   OSR_GDV( papszNV, "y_0", 0.0 ) );
        else
            SetTMSO( OSR_GDV( papszNV, "lat_0", 0.0 ), 
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

    else if( EQUAL(pszProj,"merc") /* 2SP form */
             && OSR_GDV(papszNV, "lat_ts", 1000.0) < 999.0 )
    {
        SetMercator2SP( OSR_GDV( papszNV, "lat_ts", 0.0 ), 
                        0.0,
                        OSR_GDV( papszNV, "lon_0", 0.0 ), 
                        OSR_GDV( papszNV, "x_0", 0.0 ), 
                        OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"merc") ) /* 1SP form */
    {
        SetMercator( 0.0,
                     OSR_GDV( papszNV, "lon_0", 0.0 ), 
                     OSR_GDV( papszNV, "k", 1.0 ), 
                     OSR_GDV( papszNV, "x_0", 0.0 ), 
                     OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"stere") 
             && ABS(OSR_GDV( papszNV, "lat_0", 0.0 ) - 90) < 0.001 )
    {
        SetPS( OSR_GDV( papszNV, "lat_ts", 90.0 ), 
               OSR_GDV( papszNV, "lon_0", 0.0 ), 
               OSR_GDV( papszNV, "k", 1.0 ), 
               OSR_GDV( papszNV, "x_0", 0.0 ), 
               OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"stere") 
             && ABS(OSR_GDV( papszNV, "lat_0", 0.0 ) + 90) < 0.001 )
    {
        SetPS( OSR_GDV( papszNV, "lat_ts", -90.0 ), 
               OSR_GDV( papszNV, "lon_0", 0.0 ), 
               OSR_GDV( papszNV, "k", 1.0 ), 
               OSR_GDV( papszNV, "x_0", 0.0 ), 
               OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"sterea") )
    {
        SetOS( OSR_GDV( papszNV, "lat_0", 0.0 ), 
               OSR_GDV( papszNV, "lon_0", 0.0 ), 
               OSR_GDV( papszNV, "k", 1.0 ), 
               OSR_GDV( papszNV, "x_0", 0.0 ), 
               OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"stere") )
    {
        SetStereographic( OSR_GDV( papszNV, "lat_0", 0.0 ), 
                          OSR_GDV( papszNV, "lon_0", 0.0 ), 
                          OSR_GDV( papszNV, "k", 1.0 ),  
                          OSR_GDV( papszNV, "x_0", 0.0 ), 
                          OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"eqc") )
    {
        if( OSR_GDV( papszNV, "lat_ts", 0.0 ) != 0.0 )
          SetEquirectangular2( OSR_GDV( papszNV, "lat_0", 0.0 ),
                               OSR_GDV( papszNV, "lon_0", 0.0 ),
                               OSR_GDV( papszNV, "lat_ts", 0.0 ),
                               OSR_GDV( papszNV, "x_0", 0.0 ),
                               OSR_GDV( papszNV, "y_0", 0.0 ) );
        else
          SetEquirectangular( OSR_GDV( papszNV, "lat_0", 0.0 ),
                              OSR_GDV( papszNV, "lon_0", 0.0 ),
                              OSR_GDV( papszNV, "x_0", 0.0 ),
                              OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"gstmerc") )
    {
        SetGaussSchreiberTMercator( OSR_GDV( papszNV, "lat_0", -21.116666667 ),
                                    OSR_GDV( papszNV, "lon_0", 55.53333333309),
                                    OSR_GDV( papszNV, "k_0", 1.0 ),
                                    OSR_GDV( papszNV, "x_0", 160000.000 ),
                                    OSR_GDV( papszNV, "y_0", 50000.000 ) );
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

    else if( EQUAL(pszProj,"eck1") || EQUAL(pszProj,"eck2") || EQUAL(pszProj,"eck3") ||
             EQUAL(pszProj,"eck4") || EQUAL(pszProj,"eck5") || EQUAL(pszProj,"eck6"))
    {
        SetEckert(   pszProj[3] - '0',
                     OSR_GDV( papszNV, "lon_0", 0.0 ), 
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
        SetGS( OSR_GDV( papszNV, "lon_0", 0.0 ), 
               OSR_GDV( papszNV, "x_0", 0.0 ), 
               OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"goode") )
    {
        SetGH( OSR_GDV( papszNV, "lon_0", 0.0 ), 
               OSR_GDV( papszNV, "x_0", 0.0 ), 
               OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"igh") )
    {
        SetIGH();
    }

    else if( EQUAL(pszProj,"geos") )
    {
        SetGEOS( OSR_GDV( papszNV, "lon_0", 0.0 ), 
                 OSR_GDV( papszNV, "h", 35785831.0 ), 
                 OSR_GDV( papszNV, "x_0", 0.0 ), 
                 OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"lcc") ) 
    {
        if( OSR_GDV(papszNV, "lat_0", 0.0 ) 
            == OSR_GDV(papszNV, "lat_1", 0.0 ) )
        {
            /* 1SP form */
            SetLCC1SP( OSR_GDV( papszNV, "lat_0", 0.0 ), 
                       OSR_GDV( papszNV, "lon_0", 0.0 ), 
                       OSR_GDV( papszNV, "k_0", 1.0 ), 
                       OSR_GDV( papszNV, "x_0", 0.0 ), 
                       OSR_GDV( papszNV, "y_0", 0.0 ) );
        }
        else
        {
            /* 2SP form */
            SetLCC( OSR_GDV( papszNV, "lat_1", 0.0 ), 
                    OSR_GDV( papszNV, "lat_2", 0.0 ), 
                    OSR_GDV( papszNV, "lat_0", 0.0 ), 
                    OSR_GDV( papszNV, "lon_0", 0.0 ), 
                    OSR_GDV( papszNV, "x_0", 0.0 ), 
                    OSR_GDV( papszNV, "y_0", 0.0 ) );
        }
    }

    else if( EQUAL(pszProj,"omerc") )
    {
        SetHOM( OSR_GDV( papszNV, "lat_0", 0.0 ), 
                OSR_GDV( papszNV, "lonc", 0.0 ), 
                OSR_GDV( papszNV, "alpha", 0.0 ), 
                OSR_GDV( papszNV, "gamma", 0.0 ), 
                OSR_GDV( papszNV, "k", 1.0 ), 
                OSR_GDV( papszNV, "x_0", 0.0 ), 
                OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"somerc") )
    {
        SetHOM( OSR_GDV( papszNV, "lat_0", 0.0 ), 
                OSR_GDV( papszNV, "lon_0", 0.0 ), 
                90.0,  90.0, 
                OSR_GDV( papszNV, "k", 1.0 ), 
                OSR_GDV( papszNV, "x_0", 0.0 ), 
                OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"krovak") )
    {
        SetKrovak( OSR_GDV( papszNV, "lat_0", 0.0 ), 
                   OSR_GDV( papszNV, "lon_0", 0.0 ), 
                   OSR_GDV( papszNV, "alpha", 0.0 ), 
                   0.0, // pseudo_standard_parallel_1
                   OSR_GDV( papszNV, "k", 1.0 ), 
                   OSR_GDV( papszNV, "x_0", 0.0 ), 
                   OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj, "iwm_p") )
    {
        SetIWMPolyconic( OSR_GDV( papszNV, "lat_1", 0.0 ), 
                         OSR_GDV( papszNV, "lat_2", 0.0 ),
                         OSR_GDV( papszNV, "lon_0", 0.0 ), 
                         OSR_GDV( papszNV, "x_0", 0.0 ), 
                         OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj, "wag1") )
    {
        SetWagner( 1, 0.0,
                   OSR_GDV( papszNV, "x_0", 0.0 ), 
                   OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj, "wag2") )
    {
        SetWagner( 2, 0.0,
                   OSR_GDV( papszNV, "x_0", 0.0 ), 
                   OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj, "wag3") )
    {
        SetWagner( 3,
                   OSR_GDV( papszNV, "lat_ts", 0.0 ),
                   OSR_GDV( papszNV, "x_0", 0.0 ), 
                   OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj, "wag4") )
    {
        SetWagner( 4, 0.0,
                   OSR_GDV( papszNV, "x_0", 0.0 ), 
                   OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj, "wag5") )
    {
        SetWagner( 5, 0.0,
                   OSR_GDV( papszNV, "x_0", 0.0 ), 
                   OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj, "wag6") )
    {
        SetWagner( 6, 0.0,
                   OSR_GDV( papszNV, "x_0", 0.0 ), 
                   OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj, "wag7") )
    {
        SetWagner( 7, 0.0,
                   OSR_GDV( papszNV, "x_0", 0.0 ), 
                   OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else if( EQUAL(pszProj,"tpeqd") )
    {
        SetTPED( OSR_GDV( papszNV, "lat_1", 0.0 ), 
                 OSR_GDV( papszNV, "lon_1", 0.0 ), 
                 OSR_GDV( papszNV, "lat_2", 0.0 ), 
                 OSR_GDV( papszNV, "lon_2", 0.0 ), 
                 OSR_GDV( papszNV, "x_0", 0.0 ), 
                 OSR_GDV( papszNV, "y_0", 0.0 ) );
    }

    else
    {
        CPLDebug( "OGR_PROJ4", "Unsupported projection: %s", pszProj );
        CSLDestroy( papszNV );
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
    else if( (EQUAL(pszValue,"NAD27") || EQUAL(pszValue,"NAD83")
              || EQUAL(pszValue,"WGS84") || EQUAL(pszValue,"WGS72"))
             && dfFromGreenwich == 0.0 )
    {
        SetWellKnownGeogCS( pszValue );
        bFullyDefined = TRUE;
    }
    else
    {
        unsigned int i;
        for(i=0;i<sizeof(ogr_pj_datums)/sizeof(ogr_pj_datums[0]);i++)
        {
            if ( EQUAL(pszValue, ogr_pj_datums[i].pszPJ) )
            {
                OGRSpatialReference oGCS;
                oGCS.importFromEPSG( ogr_pj_datums[i].nEPSG );
                CopyGeogCSFrom( &oGCS );
                bFullyDefined = TRUE;
                break;
            }
        }

        /* If we don't recognise the datum, we ignore it */
    }

/* -------------------------------------------------------------------- */
/*      Set the ellipsoid information.                                   */
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
            
            dfSemiMajor = CPLAtof(ogr_pj_ellps[i+1]+2);
            if( EQUALN(ogr_pj_ellps[i+2],"rf=",3) )
                dfInvFlattening = CPLAtof(ogr_pj_ellps[i+2]+3);
            else
            {
                CPLAssert( EQUALN(ogr_pj_ellps[i+2],"b=",2) );
                dfSemiMinor = CPLAtof(ogr_pj_ellps[i+2]+2);
                
                if( ABS(dfSemiMajor/dfSemiMinor) - 1.0 < 0.0000000000001 )
                    dfInvFlattening = 0.0;
                else
                    dfInvFlattening = -1.0 / (dfSemiMinor/dfSemiMajor - 1.0);
            }
            
            SetGeogCS( ogr_pj_ellps[i+3], "unknown", ogr_pj_ellps[i], 
                       dfSemiMajor, dfInvFlattening,
                       pszPM, dfFromGreenwich );

            bFullyDefined = TRUE;
            break;
        }
    }

    if( !bFullyDefined )
    {
        dfSemiMajor = OSR_GDV( papszNV, "a", 0.0 );
        if( dfSemiMajor == 0.0 )
        {
            dfSemiMajor = OSR_GDV( papszNV, "R", 0.0 );
            if( dfSemiMajor != 0.0 )
            {
                dfSemiMinor = -1.0;
                dfInvFlattening = 0.0;
            }
            else
            {
                CPLDebug( "OGR_PROJ4", "Can't find ellipse definition, default to WGS84:\n%s", 
                          pszProj4 );
                
                dfSemiMajor = SRS_WGS84_SEMIMAJOR;
                dfSemiMinor = -1.0;
                dfInvFlattening = SRS_WGS84_INVFLATTENING;
            }
        }
        else
        {
            dfSemiMinor = OSR_GDV( papszNV, "b", -1.0 );
            dfInvFlattening = OSR_GDV( papszNV, "rf", -1.0 );
            if ( dfSemiMinor == -1.0 && dfInvFlattening == -1.0 )
            {
                double dfFlattening = OSR_GDV( papszNV, "f", -1.0 );
                if ( dfFlattening == 0.0 )
                    dfSemiMinor = dfSemiMajor;
                else if ( dfFlattening != -1.0 )
                    dfInvFlattening = 1.0 / dfFlattening;
            }
        }
        
        if( dfSemiMinor == -1.0 && dfInvFlattening == -1.0 )
        {
            CPLDebug( "OGR_PROJ4", "Can't find ellipse definition in:\n%s", 
                      pszProj4 );
            CSLDestroy( papszNV );
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
                   dfSemiMajor, dfInvFlattening,
                   pszPM, dfFromGreenwich );
        
        bFullyDefined = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Handle TOWGS84 conversion.                                      */
/* -------------------------------------------------------------------- */
    pszValue = CSLFetchNameValue(papszNV, "towgs84");
    if(pszValue!=NULL)
    {
        char **papszToWGS84 = CSLTokenizeStringComplex( pszValue, ",", 
                                                        FALSE, TRUE );

        if( CSLCount(papszToWGS84) >= 7 )
            SetTOWGS84( CPLAtof(papszToWGS84[0]), 
                        CPLAtof(papszToWGS84[1]), 
                        CPLAtof(papszToWGS84[2]), 
                        CPLAtof(papszToWGS84[3]), 
                        CPLAtof(papszToWGS84[4]), 
                        CPLAtof(papszToWGS84[5]), 
                        CPLAtof(papszToWGS84[6]) );
        else if( CSLCount(papszToWGS84) >= 3 )
            SetTOWGS84( CPLAtof(papszToWGS84[0]), 
                        CPLAtof(papszToWGS84[1]), 
                        CPLAtof(papszToWGS84[2]) );
        else
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "Seemingly corrupt +towgs84 option (%s), ignoring.", 
                      pszValue );
                        
        CSLDestroy(papszToWGS84);
    }

/* -------------------------------------------------------------------- */
/*      Handle nadgrids via an extension node.                          */
/* -------------------------------------------------------------------- */
    pszValue = CSLFetchNameValue(papszNV, "nadgrids");
    if( pszValue != NULL )
    {
        SetExtension( "DATUM", "PROJ4_GRIDS", pszValue );
    }

/* -------------------------------------------------------------------- */
/*      Linear units translation                                        */
/* -------------------------------------------------------------------- */
    if( IsProjected() || IsLocal() || IsGeocentric() )
    {
        pszValue = CSLFetchNameValue(papszNV, "to_meter");

        if( pszValue != NULL && CPLAtofM(pszValue) > 0.0 )
        {
            double dfValue = CPLAtofM(pszValue);

            if( fabs(dfValue - CPLAtof(SRS_UL_US_FOOT_CONV)) < 0.00000001 )
                SetLinearUnits( SRS_UL_US_FOOT, CPLAtof(SRS_UL_US_FOOT_CONV) );
            else if( fabs(dfValue - CPLAtof(SRS_UL_FOOT_CONV)) < 0.00000001 )
                SetLinearUnits( SRS_UL_FOOT, CPLAtof(SRS_UL_FOOT_CONV) );
            else if( dfValue == 1.0 )
                SetLinearUnits( SRS_UL_METER, 1.0 );
            else
                SetLinearUnits( "unknown", CPLAtofM(pszValue) );
        }
        else if( (pszValue = CSLFetchNameValue(papszNV, "units")) != NULL )
        {
            if( EQUAL(pszValue,"meter" ) || EQUAL(pszValue,"m") )
                SetLinearUnits( SRS_UL_METER, 1.0 );
            else if( EQUAL(pszValue,"us-ft" ) )
                SetLinearUnits( SRS_UL_US_FOOT, CPLAtof(SRS_UL_US_FOOT_CONV) );
            else if( EQUAL(pszValue,"ft" ) )
                SetLinearUnits( SRS_UL_FOOT, CPLAtof(SRS_UL_FOOT_CONV) );
            else if( EQUAL(pszValue,"yd" ) )
                SetLinearUnits( pszValue, 0.9144 );
            else if( EQUAL(pszValue,"us-yd" ) )
                SetLinearUnits( pszValue, 0.914401828803658 );
            else // This case is untranslatable.  Should add all proj.4 unts
                SetLinearUnits( pszValue, 1.0 );
        }
    }

/* -------------------------------------------------------------------- */
/*      Adjust linear parameters into PROJCS units if the linear        */
/*      units are not meters.                                           */
/* -------------------------------------------------------------------- */
    if( GetLinearUnits() != 1.0 && IsProjected() )
    {
        OGR_SRSNode *poPROJCS = GetAttrNode( "PROJCS" );
        int  i;

        for( i = 0; i < poPROJCS->GetChildCount(); i++ )
        {
            OGR_SRSNode *poParm = poPROJCS->GetChild(i);
            if( !EQUAL(poParm->GetValue(),"PARAMETER") 
                || poParm->GetChildCount() != 2 )
                continue;

            const char *pszParmName = poParm->GetChild(0)->GetValue();

            if( IsLinearParameter(pszParmName) )
                SetNormProjParm(pszParmName,GetProjParm(pszParmName));
        }        
    }

/* -------------------------------------------------------------------- */
/*      Handle geoidgrids via an extension node and COMPD_CS.           */
/* -------------------------------------------------------------------- */
    pszValue = CSLFetchNameValue(papszNV, "geoidgrids");
    if( pszValue != NULL )
    {
        OGR_SRSNode *poHorizSRS = GetRoot()->Clone();
        
        Clear();
        
        CPLString osName = poHorizSRS->GetChild(0)->GetValue();
        osName += " + ";
        osName += "Unnamed Vertical Datum";
        
        SetNode( "COMPD_CS", osName );
        GetRoot()->AddChild( poHorizSRS );

        OGR_SRSNode *poVertSRS;
        
        poVertSRS = new OGR_SRSNode( "VERT_CS" );
        GetRoot()->AddChild( poVertSRS );
        poVertSRS->AddChild( new OGR_SRSNode( "Unnamed" ) );

        CPLString osTarget = GetRoot()->GetValue();
        osTarget += "|VERT_CS|VERT_DATUM";

        SetNode( osTarget, "Unnamed" );
        
        poVertSRS->GetChild(1)->AddChild( new OGR_SRSNode( "2005" ) );
        SetExtension( osTarget, "PROJ4_GRIDS", pszValue );

        OGR_SRSNode *poAxis = new OGR_SRSNode( "AXIS" );

        poAxis->AddChild( new OGR_SRSNode( "Up" ) );
        poAxis->AddChild( new OGR_SRSNode( "UP" ) );
        
        poVertSRS->AddChild( poAxis );
    }

/* -------------------------------------------------------------------- */
/*      Handle vertical units.                                          */
/* -------------------------------------------------------------------- */
    if( GetRoot()->GetNode( "VERT_CS" ) != NULL )
    {
        const char *pszUnitName = NULL;
        const char *pszUnitConv = NULL;

        pszValue = CSLFetchNameValue(papszNV, "vto_meter");

        if( pszValue != NULL && CPLAtofM(pszValue) > 0.0 )
        {
            double dfValue = CPLAtofM(pszValue);

            if( fabs(dfValue - CPLAtof(SRS_UL_US_FOOT_CONV)) < 0.00000001 )
            {
                pszUnitName = SRS_UL_US_FOOT;
                pszUnitConv = SRS_UL_US_FOOT_CONV;
            }
            else if( fabs(dfValue - CPLAtof(SRS_UL_FOOT_CONV)) < 0.00000001 )
            {
                pszUnitName = SRS_UL_FOOT;
                pszUnitConv = SRS_UL_FOOT_CONV;
            }
            else if( dfValue == 1.0 )
            {
                pszUnitName = SRS_UL_METER;
                pszUnitConv = "1.0";
            }
            else
            {
                pszUnitName = "unknown";
                pszUnitConv = pszValue;
            }
        }
        else if( (pszValue = CSLFetchNameValue(papszNV, "vunits")) != NULL )
        {
            if( EQUAL(pszValue,"meter" ) || EQUAL(pszValue,"m") )
            {
                pszUnitName = SRS_UL_METER;
                pszUnitConv = "1.0";
            }
            else if( EQUAL(pszValue,"us-ft" ) )
            {
                pszUnitName = SRS_UL_US_FOOT;
                pszUnitConv = SRS_UL_US_FOOT_CONV;
            }
            else if( EQUAL(pszValue,"ft" ) )
            {
                pszUnitName = SRS_UL_FOOT;
                pszUnitConv = SRS_UL_FOOT_CONV;
            }
            else if( EQUAL(pszValue,"yd" ) )
            {
                pszUnitName = "Yard";
                pszUnitConv = "0.9144";
            }
            else if( EQUAL(pszValue,"us-yd" ) )
            {
                pszUnitName = "US Yard";
                pszUnitConv = "0.914401828803658";
            }
        }

        if( pszUnitName != NULL )
        {
            OGR_SRSNode *poVERT_CS = GetRoot()->GetNode( "VERT_CS" );
            OGR_SRSNode *poUnits; 

            poUnits = new OGR_SRSNode( "UNIT" );
            poUnits->AddChild( new OGR_SRSNode( pszUnitName ) );
            poUnits->AddChild( new OGR_SRSNode( pszUnitConv ) );
            
            poVERT_CS->AddChild( poUnits );
        }
    }

/* -------------------------------------------------------------------- */
/*      do we want to insert a PROJ.4 EXTENSION item?                   */
/* -------------------------------------------------------------------- */
    if( strstr(pszProj4,"wktext") != NULL )
        SetExtension( GetRoot()->GetValue(), "PROJ4", pszProj4 );
        
    CSLDestroy( papszNV );
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                           LinearToProj4()                            */
/************************************************************************/

static const char *LinearToProj4( double dfLinearConv, 
                                  const char *pszLinearUnits )

{
    if( dfLinearConv == 1.0 )
        return "m";

    else if( dfLinearConv == 1000.0 )
        return "km";
    
    else if( dfLinearConv == 0.0254 )
        return "in";
    
    else if( EQUAL(pszLinearUnits,SRS_UL_FOOT) 
             || fabs(dfLinearConv - atof(SRS_UL_FOOT_CONV)) < 0.000000001 )
        return "ft";
    
    else if( EQUAL(pszLinearUnits,"IYARD") || dfLinearConv == 0.9144 )
        return "yd";

    else if( dfLinearConv == 0.914401828803658 )
        return "us-yd";
    
    else if( dfLinearConv == 0.001 )
        return "mm";
    
    else if( dfLinearConv == 0.01 )
        return "cm";

    else if( EQUAL(pszLinearUnits,SRS_UL_US_FOOT) 
             || fabs(dfLinearConv - atof(SRS_UL_US_FOOT_CONV)) < 0.00000001 )
        return "us-ft";

    else if( EQUAL(pszLinearUnits,SRS_UL_NAUTICAL_MILE) )
        return "kmi";

    else if( EQUAL(pszLinearUnits,"Mile") 
             || EQUAL(pszLinearUnits,"IMILE") )
        return "mi";
    else
        return NULL;
}


/************************************************************************/
/*                          OSRExportToProj4()                          */
/************************************************************************/
/** 
 * \brief Export coordinate system in PROJ.4 format.
 *
 * This function is the same as OGRSpatialReference::exportToProj4().
 */
OGRErr CPL_STDCALL OSRExportToProj4( OGRSpatialReferenceH hSRS, 
                                     char ** ppszReturn )

{
    VALIDATE_POINTER1( hSRS, "OSRExportToProj4", CE_Failure );

    *ppszReturn = NULL;

    return ((OGRSpatialReference *) hSRS)->exportToProj4( ppszReturn );
}

/************************************************************************/
/*                           exportToProj4()                            */
/************************************************************************/

#define SAFE_PROJ4_STRCAT(szNewStr)  do { \
    if(CPLStrlcat(szProj4, szNewStr, sizeof(szProj4)) >= sizeof(szProj4)) { \
        CPLError(CE_Failure, CPLE_AppDefined, "String overflow when formatting proj.4 string"); \
        *ppszProj4 = CPLStrdup(""); \
        return OGRERR_FAILURE; \
    } } while(0);

/**
 * \brief Export coordinate system in PROJ.4 format.
 *
 * Converts the loaded coordinate reference system into PROJ.4 format
 * to the extent possible.  The string returned in ppszProj4 should be
 * deallocated by the caller with CPLFree() when no longer needed.
 *
 * LOCAL_CS coordinate systems are not translatable.  An empty string
 * will be returned along with OGRERR_NONE.  
 *
 * This method is the equivelent of the C function OSRExportToProj4().
 *
 * @param ppszProj4 pointer to which dynamically allocated PROJ.4 definition 
 * will be assigned. 
 *
 * @return OGRERR_NONE on success or an error code on failure. 
 */

OGRErr OGRSpatialReference::exportToProj4( char ** ppszProj4 ) const

{
    char        szProj4[512];
    const char *pszProjection = GetAttrValue("PROJECTION");
    CPLLocaleC  oLocaleEnforcer;

    szProj4[0] = '\0';

    if( GetRoot() == NULL )
    {
        *ppszProj4 = CPLStrdup("");
        CPLError( CE_Failure, CPLE_NotSupported,
                  "No translation an empty SRS to PROJ.4 format is known.");
        return OGRERR_UNSUPPORTED_SRS;
    }

/* -------------------------------------------------------------------- */
/*      Do we have a PROJ.4 override definition?                        */
/* -------------------------------------------------------------------- */
    const char *pszPredefProj4 = GetExtension( GetRoot()->GetValue(), 
                                               "PROJ4", NULL );
    if( pszPredefProj4 != NULL )
    {
        *ppszProj4 = CPLStrdup( pszPredefProj4 );
        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Get the prime meridian info.                                    */
/* -------------------------------------------------------------------- */
    const OGR_SRSNode *poPRIMEM = GetAttrNode( "PRIMEM" );
    double dfFromGreenwich = 0.0;

    if( poPRIMEM != NULL && poPRIMEM->GetChildCount() >= 2 
        && CPLAtof(poPRIMEM->GetChild(1)->GetValue()) != 0.0 )
    {
        dfFromGreenwich = CPLAtof(poPRIMEM->GetChild(1)->GetValue());
    }

/* ==================================================================== */
/*      Handle the projection definition.                               */
/* ==================================================================== */

    if( pszProjection == NULL && IsGeographic() )
    {
        sprintf( szProj4+strlen(szProj4), "+proj=longlat " );
    }
    else if( IsGeocentric() )
    {
        sprintf( szProj4+strlen(szProj4), "+proj=geocent " );
    }

    else if( pszProjection == NULL && !IsGeographic() )
    {
        // LOCAL_CS, or incompletely initialized coordinate systems.
        *ppszProj4 = CPLStrdup("");
        return OGRERR_NONE;
    }
    else if( EQUAL(pszProjection,SRS_PT_CYLINDRICAL_EQUAL_AREA) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=cea +lon_0=%.16g +lat_ts=%.16g +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_BONNE) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=bonne +lon_0=%.16g +lat_1=%.16g +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_CASSINI_SOLDNER) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=cass +lat_0=%.16g +lon_0=%.16g +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_NEW_ZEALAND_MAP_GRID) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=nzmg +lat_0=%.16g +lon_0=%.16g +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_TRANSVERSE_MERCATOR) ||
             EQUAL(pszProjection,SRS_PT_TRANSVERSE_MERCATOR_MI_21) ||
             EQUAL(pszProjection,SRS_PT_TRANSVERSE_MERCATOR_MI_22) ||
             EQUAL(pszProjection,SRS_PT_TRANSVERSE_MERCATOR_MI_23) ||
             EQUAL(pszProjection,SRS_PT_TRANSVERSE_MERCATOR_MI_24) ||
             EQUAL(pszProjection,SRS_PT_TRANSVERSE_MERCATOR_MI_25) )
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
                     "+proj=tmerc +lat_0=%.16g +lon_0=%.16g +k=%.16g +x_0=%.16g +y_0=%.16g ",
                     GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                     GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                     GetNormProjParm(SRS_PP_SCALE_FACTOR,1.0),
                     GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                     GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_TRANSVERSE_MERCATOR_SOUTH_ORIENTED) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=tmerc +lat_0=%.16g +lon_0=%.16g +k=%.16g +x_0=%.16g +y_0=%.16g +axis=wsu ",
                 GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_SCALE_FACTOR,1.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_MERCATOR_1SP) )
    {
        if( GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0) == 0.0 )
            sprintf( szProj4+strlen(szProj4),
                     "+proj=merc +lon_0=%.16g +k=%.16g +x_0=%.16g +y_0=%.16g ",
                     GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                     GetNormProjParm(SRS_PP_SCALE_FACTOR,1.0),
                     GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                     GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
        else if( GetNormProjParm(SRS_PP_SCALE_FACTOR,1.0) == 1.0 )
            sprintf( szProj4+strlen(szProj4),
                     "+proj=merc +lon_0=%.16g +lat_ts=%.16g +x_0=%.16g +y_0=%.16g ",
                     GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                     GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                     GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                     GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
        else
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                      "Mercator_1SP with scale != 1.0 and latitude of origin != 0, not supported by PROJ.4." );
            *ppszProj4 = CPLStrdup("");
            return OGRERR_UNSUPPORTED_SRS;
        }
    }

    else if( EQUAL(pszProjection,SRS_PT_MERCATOR_2SP) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=merc +lon_0=%.16g +lat_ts=%.16g +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_OBLIQUE_STEREOGRAPHIC) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=sterea +lat_0=%.16g +lon_0=%.16g +k=%.16g +x_0=%.16g +y_0=%.16g ",
//         "+proj=stere +lat_0=%.16g +lon_0=%.16g +k=%.16g +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_SCALE_FACTOR,1.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_STEREOGRAPHIC) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=stere +lat_0=%.16g +lon_0=%.16g +k=%.16g +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_SCALE_FACTOR,1.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_POLAR_STEREOGRAPHIC) )
    {
        if( GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0) >= 0.0 )
            sprintf( szProj4+strlen(szProj4),
                     "+proj=stere +lat_0=90 +lat_ts=%.16g +lon_0=%.16g "
                     "+k=%.16g +x_0=%.16g +y_0=%.16g ",
                     GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,90.0),
                     GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                     GetNormProjParm(SRS_PP_SCALE_FACTOR,1.0),
                     GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                     GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
        else
            sprintf( szProj4+strlen(szProj4),
                     "+proj=stere +lat_0=-90 +lat_ts=%.16g +lon_0=%.16g "
                     "+k=%.16g +x_0=%.16g +y_0=%.16g ",
                     GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,-90.0),
                     GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                     GetNormProjParm(SRS_PP_SCALE_FACTOR,1.0),
                     GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                     GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_EQUIRECTANGULAR) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=eqc +lat_ts=%.16g +lat_0=%.16g +lon_0=%.16g +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0),
                 GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_GAUSSSCHREIBERTMERCATOR) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=gstmerc +lat_0=%.16g +lon_0=%.16g"
                 " +k_0=%.16g +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,-21.116666667),
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,55.53333333309),
                 GetNormProjParm(SRS_PP_SCALE_FACTOR,1.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,160000.000),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,50000.000) );
    }

    else if( EQUAL(pszProjection,SRS_PT_GNOMONIC) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=gnom +lat_0=%.16g +lon_0=%.16g +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_ORTHOGRAPHIC) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=ortho +lat_0=%.16g +lon_0=%.16g +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=laea +lat_0=%.16g +lon_0=%.16g +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_AZIMUTHAL_EQUIDISTANT) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=aeqd +lat_0=%.16g +lon_0=%.16g +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_EQUIDISTANT_CONIC) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=eqdc +lat_0=%.16g +lon_0=%.16g +lat_1=%.16g +lat_2=%.16g"
                 " +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_LATITUDE_OF_CENTER,0.0),
                 GetNormProjParm(SRS_PP_LONGITUDE_OF_CENTER,0.0),
                 GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0),
                 GetNormProjParm(SRS_PP_STANDARD_PARALLEL_2,0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_MILLER_CYLINDRICAL) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=mill +lat_0=%.16g +lon_0=%.16g +x_0=%.16g +y_0=%.16g +R_A ",
                 GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_MOLLWEIDE) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=moll +lon_0=%.16g +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_ECKERT_I) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=eck1 +lon_0=%.16g +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_ECKERT_II) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=eck2 +lon_0=%.16g +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_ECKERT_III) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=eck3 +lon_0=%.16g +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }
    
    else if( EQUAL(pszProjection,SRS_PT_ECKERT_IV) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=eck4 +lon_0=%.16g +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }
    
    else if( EQUAL(pszProjection,SRS_PT_ECKERT_V) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=eck5 +lon_0=%.16g +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }
    
    else if( EQUAL(pszProjection,SRS_PT_ECKERT_VI) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=eck6 +lon_0=%.16g +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_POLYCONIC) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=poly +lat_0=%.16g +lon_0=%.16g +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_ALBERS_CONIC_EQUAL_AREA) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=aea +lat_1=%.16g +lat_2=%.16g +lat_0=%.16g +lon_0=%.16g"
                 " +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0),
                 GetNormProjParm(SRS_PP_STANDARD_PARALLEL_2,0.0),
                 GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_ROBINSON) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=robin +lon_0=%.16g +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_VANDERGRINTEN) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=vandg +lon_0=%.16g +x_0=%.16g +y_0=%.16g +R_A ",
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_SINUSOIDAL) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=sinu +lon_0=%.16g +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_LONGITUDE_OF_CENTER,0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_GALL_STEREOGRAPHIC) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=gall +lon_0=%.16g +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_GOODE_HOMOLOSINE) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=goode +lon_0=%.16g +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_IGH) )
    {
        sprintf( szProj4+strlen(szProj4), "+proj=igh " );
    }

    else if( EQUAL(pszProjection,SRS_PT_GEOSTATIONARY_SATELLITE) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=geos +lon_0=%.16g +h=%.16g +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_SATELLITE_HEIGHT,35785831.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP)
             || EQUAL(pszProjection,SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=lcc +lat_1=%.16g +lat_2=%.16g +lat_0=%.16g +lon_0=%.16g"
                 " +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0),
                 GetNormProjParm(SRS_PP_STANDARD_PARALLEL_2,0.0),
                 GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }
    
    else if( EQUAL(pszProjection,SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=lcc +lat_1=%.16g +lat_0=%.16g +lon_0=%.16g"
                 " +k_0=%.16g +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_SCALE_FACTOR,1.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_HOTINE_OBLIQUE_MERCATOR) )
    {
        /* special case for swiss oblique mercator : see bug 423 */
        if( fabs(GetNormProjParm(SRS_PP_AZIMUTH,0.0) - 90.0) < 0.0001 
            && fabs(GetNormProjParm(SRS_PP_RECTIFIED_GRID_ANGLE,0.0)-90.0) < 0.0001 )
        {
            sprintf( szProj4+strlen(szProj4),
                     "+proj=somerc +lat_0=%.16g +lon_0=%.16g"
                     " +k_0=%.16g +x_0=%.16g +y_0=%.16g ",
                     GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                     GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                     GetNormProjParm(SRS_PP_SCALE_FACTOR,1.0),
                     GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                     GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
        }
        else
        {
            sprintf( szProj4+strlen(szProj4),
                     "+proj=omerc +lat_0=%.16g +lonc=%.16g +alpha=%.16g"
                     " +k=%.16g +x_0=%.16g +y_0=%.16g ",
                     GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                     GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                     GetNormProjParm(SRS_PP_AZIMUTH,0.0),
                     GetNormProjParm(SRS_PP_SCALE_FACTOR,1.0),
                     GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                     GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );

            // RSO variant - http://trac.osgeo.org/proj/ticket/62
            // Note that gamma is only supported by PROJ 4.8.0 and later.
            if( GetNormProjParm(SRS_PP_RECTIFIED_GRID_ANGLE,1000.0) != 1000.0 )
            {
                sprintf( szProj4+strlen(szProj4), "+gamma=%.16g ",
                         GetNormProjParm(SRS_PP_RECTIFIED_GRID_ANGLE,1000.0));
            }
        }
    }

    else if( EQUAL(pszProjection,
                   SRS_PT_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=omerc +lat_0=%.16g"
                 " +lon_1=%.16g +lat_1=%.16g +lon_2=%.16g +lat_2=%.16g"
                 " +k=%.16g +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetNormProjParm(SRS_PP_LATITUDE_OF_POINT_1,0.0),
                 GetNormProjParm(SRS_PP_LONGITUDE_OF_POINT_1,0.0),
                 GetNormProjParm(SRS_PP_LATITUDE_OF_POINT_2,0.0),
                 GetNormProjParm(SRS_PP_LONGITUDE_OF_POINT_2,0.0),
                 GetNormProjParm(SRS_PP_SCALE_FACTOR,1.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_KROVAK) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=krovak +lat_0=%.16g +lon_0=%.16g +alpha=%.16g"
                 " +k=%.16g +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_LATITUDE_OF_CENTER,0.0),
                 GetNormProjParm(SRS_PP_LONGITUDE_OF_CENTER,0.0),
                 GetNormProjParm(SRS_PP_AZIMUTH,0.0),
                 GetNormProjParm(SRS_PP_SCALE_FACTOR,1.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection,SRS_PT_TWO_POINT_EQUIDISTANT) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=tpeqd +lat_1=%.16g +lon_1=%.16g "
                 "+lat_2=%.16g +lon_2=%.16g "
                 "+x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_LATITUDE_OF_1ST_POINT,0.0),
                 GetNormProjParm(SRS_PP_LONGITUDE_OF_1ST_POINT,0.0),
                 GetNormProjParm(SRS_PP_LATITUDE_OF_2ND_POINT,0.0),
                 GetNormProjParm(SRS_PP_LONGITUDE_OF_2ND_POINT,0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else if( EQUAL(pszProjection, SRS_PT_IMW_POLYCONIC) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=iwm_p +lat_1=%.16g +lat_2=%.16g +lon_0=%.16g "
                 "+x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_LATITUDE_OF_1ST_POINT, 0.0),
                 GetNormProjParm(SRS_PP_LATITUDE_OF_2ND_POINT, 0.0),
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0) );
    }

    else if( EQUAL(pszProjection, SRS_PT_WAGNER_I) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=wag1 +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0) );
    }

    else if( EQUAL(pszProjection, SRS_PT_WAGNER_II) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=wag2 +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0) );
    }

    else if( EQUAL(pszProjection, SRS_PT_WAGNER_III) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=wag3 +lat_ts=%.16g +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0) );
    }

    else if( EQUAL(pszProjection, SRS_PT_WAGNER_IV) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=wag4 +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0) );
    }

    else if( EQUAL(pszProjection, SRS_PT_WAGNER_V) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=wag5 +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0) );
    }

    else if( EQUAL(pszProjection, SRS_PT_WAGNER_VI) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=wag6 +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0) );
    }

    else if( EQUAL(pszProjection, SRS_PT_WAGNER_VII) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=wag7 +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0) );
    }

    /* Note: This never really gets used currently.  See bug 423 */
    else if( EQUAL(pszProjection,SRS_PT_SWISS_OBLIQUE_CYLINDRICAL) )
    {
        sprintf( szProj4+strlen(szProj4),
                 "+proj=somerc +lat_0=%.16g +lon_0=%.16g"
                 " +x_0=%.16g +y_0=%.16g ",
                 GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                 GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                 GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                 GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0) );
    }

    else
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "No translation for %s to PROJ.4 format is known.", 
                  pszProjection );
        *ppszProj4 = CPLStrdup("");
        return OGRERR_UNSUPPORTED_SRS;
    }

/* -------------------------------------------------------------------- */
/*      Handle earth model.  For now we just always emit the user       */
/*      defined ellipsoid parameters.                                   */
/* -------------------------------------------------------------------- */
    double      dfSemiMajor = GetSemiMajor();
    double      dfInvFlattening = GetInvFlattening();
    const char  *pszPROJ4Ellipse = NULL;
    const char  *pszDatum = GetAttrValue("DATUM");

    if( ABS(dfSemiMajor-6378249.145) < 0.01
        && ABS(dfInvFlattening-293.465) < 0.0001 )
    {
        pszPROJ4Ellipse = "clrk80";     /* Clark 1880 */
    }
    else if( ABS(dfSemiMajor-6378245.0) < 0.01
             && ABS(dfInvFlattening-298.3) < 0.0001 )
    {
        pszPROJ4Ellipse = "krass";      /* Krassovsky */
    }
    else if( ABS(dfSemiMajor-6378388.0) < 0.01
             && ABS(dfInvFlattening-297.0) < 0.0001 )
    {
        pszPROJ4Ellipse = "intl";       /* International 1924 */
    }
    else if( ABS(dfSemiMajor-6378160.0) < 0.01
             && ABS(dfInvFlattening-298.25) < 0.0001 )
    {
        pszPROJ4Ellipse = "aust_SA";    /* Australian */
    }
    else if( ABS(dfSemiMajor-6377397.155) < 0.01
             && ABS(dfInvFlattening-299.1528128) < 0.0001 )
    {
        pszPROJ4Ellipse = "bessel";     /* Bessel 1841 */
    }
    else if( ABS(dfSemiMajor-6377483.865) < 0.01
             && ABS(dfInvFlattening-299.1528128) < 0.0001 )
    {
        pszPROJ4Ellipse = "bess_nam";   /* Bessel 1841 (Namibia / Schwarzeck)*/
    }
    else if( ABS(dfSemiMajor-6378160.0) < 0.01
             && ABS(dfInvFlattening-298.247167427) < 0.0001 )
    {
        pszPROJ4Ellipse = "GRS67";      /* GRS 1967 */
    }
    else if( ABS(dfSemiMajor-6378137) < 0.01
             && ABS(dfInvFlattening-298.257222101) < 0.000001 )
    {
        pszPROJ4Ellipse = "GRS80";      /* GRS 1980 */
    }
    else if( ABS(dfSemiMajor-6378206.4) < 0.01
             && ABS(dfInvFlattening-294.9786982) < 0.0001 )
    {
        pszPROJ4Ellipse = "clrk66";     /* Clarke 1866 */
    }
    else if( ABS(dfSemiMajor-6377340.189) < 0.01
             && ABS(dfInvFlattening-299.3249646) < 0.0001 )
    {
        pszPROJ4Ellipse = "mod_airy";   /* Modified Airy */
    }
    else if( ABS(dfSemiMajor-6377563.396) < 0.01
             && ABS(dfInvFlattening-299.3249646) < 0.0001 )
    {
        pszPROJ4Ellipse = "airy";       /* Airy */
    }
    else if( ABS(dfSemiMajor-6378200) < 0.01
             && ABS(dfInvFlattening-298.3) < 0.0001 )
    {
        pszPROJ4Ellipse = "helmert";    /* Helmert 1906 */
    }
    else if( ABS(dfSemiMajor-6378155) < 0.01
             && ABS(dfInvFlattening-298.3) < 0.0001 )
    {
        pszPROJ4Ellipse = "fschr60m";   /* Modified Fischer 1960 */
    }
    else if( ABS(dfSemiMajor-6377298.556) < 0.01
             && ABS(dfInvFlattening-300.8017) < 0.0001 )
    {
        pszPROJ4Ellipse = "evrstSS";    /* Everest (Sabah & Sarawak) */
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
    else if( pszDatum != NULL && EQUAL(pszDatum,"North_American_Datum_1927") )
    {
//        pszPROJ4Ellipse = "clrk66:+datum=nad27"; /* NAD 27 */
        pszPROJ4Ellipse = "clrk66";
    }
    else if( pszDatum != NULL && EQUAL(pszDatum,"North_American_Datum_1983") )
    {
//        pszPROJ4Ellipse = "GRS80:+datum=nad83";       /* NAD 83 */
        pszPROJ4Ellipse = "GRS80";
    }

    char szEllipseDef[128];

    if( pszPROJ4Ellipse == NULL )
        sprintf( szEllipseDef, "+a=%.16g +b=%.16g ",
                 GetSemiMajor(), GetSemiMinor() );
    else
        sprintf( szEllipseDef, "+ellps=%s ",
                 pszPROJ4Ellipse );

/* -------------------------------------------------------------------- */
/*      Translate the datum.                                            */
/* -------------------------------------------------------------------- */
    const char *pszPROJ4Datum = NULL;
    const OGR_SRSNode *poTOWGS84 = GetAttrNode( "TOWGS84" );
    char  szTOWGS84[256];
    int nEPSGDatum = -1;
    const char *pszAuthority;
    int nEPSGGeogCS = -1;
    const char *pszGeogCSAuthority;
    const char *pszProj4Grids = GetExtension( "DATUM", "PROJ4_GRIDS" );

    pszAuthority = GetAuthorityName( "DATUM" );

    if( pszAuthority != NULL && EQUAL(pszAuthority,"EPSG") )
        nEPSGDatum = atoi(GetAuthorityCode( "DATUM" ));

    pszGeogCSAuthority = GetAuthorityName( "GEOGCS" );

    if( pszGeogCSAuthority != NULL && EQUAL(pszGeogCSAuthority,"EPSG") )
        nEPSGGeogCS = atoi(GetAuthorityCode( "GEOGCS" ));

    if( pszDatum == NULL )
        /* nothing */;

    else if( EQUAL(pszDatum,SRS_DN_NAD27) || nEPSGDatum == 6267 )
        pszPROJ4Datum = "NAD27";

    else if( EQUAL(pszDatum,SRS_DN_NAD83) || nEPSGDatum == 6269 )
        pszPROJ4Datum = "NAD83";

    else if( EQUAL(pszDatum,SRS_DN_WGS84) || nEPSGDatum == 6326 )
        pszPROJ4Datum = "WGS84";

    else if( (pszPROJ4Datum = OGRGetProj4Datum(pszDatum, nEPSGDatum)) != NULL )
    {
        /* nothing */
    }

    if( pszProj4Grids != NULL )
    {
        SAFE_PROJ4_STRCAT( szEllipseDef );
        szEllipseDef[0] = '\0';
        SAFE_PROJ4_STRCAT( "+nadgrids=" );
        SAFE_PROJ4_STRCAT( pszProj4Grids );
        SAFE_PROJ4_STRCAT(  " " );
        pszPROJ4Datum = NULL;
    }

    if( pszPROJ4Datum == NULL 
        || CSLTestBoolean(CPLGetConfigOption("OVERRIDE_PROJ_DATUM_WITH_TOWGS84", "YES")) )
    {
        if( poTOWGS84 != NULL )
        {
            int iChild;
            if( poTOWGS84->GetChildCount() >= 3
                && (poTOWGS84->GetChildCount() < 7 
                    || (EQUAL(poTOWGS84->GetChild(3)->GetValue(),"")
                        && EQUAL(poTOWGS84->GetChild(4)->GetValue(),"")
                        && EQUAL(poTOWGS84->GetChild(5)->GetValue(),"")
                        && EQUAL(poTOWGS84->GetChild(6)->GetValue(),""))) )
            {
                SAFE_PROJ4_STRCAT( szEllipseDef );
                szEllipseDef[0] = '\0';
                SAFE_PROJ4_STRCAT( "+towgs84=");
                for(iChild = 0; iChild < 3; iChild ++)
                {
                    if (iChild > 0 ) SAFE_PROJ4_STRCAT( "," );
                    SAFE_PROJ4_STRCAT( poTOWGS84->GetChild(iChild)->GetValue() );
                }
                SAFE_PROJ4_STRCAT( " " );
                pszPROJ4Datum = NULL;
            }
            else if( poTOWGS84->GetChildCount() >= 7)
            {
                SAFE_PROJ4_STRCAT( szEllipseDef );
                szEllipseDef[0] = '\0';
                SAFE_PROJ4_STRCAT( "+towgs84=");
                for(iChild = 0; iChild < 7; iChild ++)
                {
                    if (iChild > 0 ) SAFE_PROJ4_STRCAT( "," );
                    SAFE_PROJ4_STRCAT( poTOWGS84->GetChild(iChild)->GetValue() );
                }
                SAFE_PROJ4_STRCAT( " " );
                pszPROJ4Datum = NULL;
            }
        }
        
        // If we don't know the datum, trying looking up TOWGS84 parameters
        // based on the EPSG GCS code.
        else if( nEPSGGeogCS != -1 && pszPROJ4Datum == NULL )
        {
            double padfTransform[7];
            if( EPSGGetWGS84Transform( nEPSGGeogCS, padfTransform ) )
            {
                sprintf( szTOWGS84, "+towgs84=%.16g,%.16g,%.16g,%.16g,%.16g,%.16g,%.16g",
                         padfTransform[0],
                         padfTransform[1],
                         padfTransform[2],
                         padfTransform[3],
                         padfTransform[4],
                         padfTransform[5],
                         padfTransform[6] );
                SAFE_PROJ4_STRCAT( szEllipseDef );
                szEllipseDef[0] = '\0';

                SAFE_PROJ4_STRCAT( szTOWGS84 );
                SAFE_PROJ4_STRCAT( " " );
                pszPROJ4Datum = NULL;
            }
        }
    }
    
    if( pszPROJ4Datum != NULL )
    {
        SAFE_PROJ4_STRCAT( "+datum=" );
        SAFE_PROJ4_STRCAT( pszPROJ4Datum );
        SAFE_PROJ4_STRCAT( " " );
    }
    else // The ellipsedef may already have been appended and will now
        // be empty, otherwise append now.
    {
        SAFE_PROJ4_STRCAT( szEllipseDef );
        szEllipseDef[0] = '\0';
    }

/* -------------------------------------------------------------------- */
/*      Is there prime meridian info to apply?                          */
/* -------------------------------------------------------------------- */
    if( poPRIMEM != NULL && poPRIMEM->GetChildCount() >= 2 
        && CPLAtof(poPRIMEM->GetChild(1)->GetValue()) != 0.0 )
    {
        const char *pszAuthority = GetAuthorityName( "PRIMEM" );
        char szPMValue[128];
        int  nCode = -1;

        if( pszAuthority != NULL && EQUAL(pszAuthority,"EPSG") )
            nCode = atoi(GetAuthorityCode( "PRIMEM" ));

        const OGRProj4PM* psProj4PM = NULL;
        if (nCode > 0)
            psProj4PM = OGRGetProj4PMFromCode(nCode);
        if (psProj4PM == NULL)
            psProj4PM = OGRGetProj4PMFromVal(dfFromGreenwich);

        if (psProj4PM != NULL)
        {
            strcpy( szPMValue, psProj4PM->pszProj4PMName );
        }
        else
        {
            sprintf( szPMValue, "%.16g", dfFromGreenwich );
        }

        SAFE_PROJ4_STRCAT( "+pm=" );
        SAFE_PROJ4_STRCAT( szPMValue );
        SAFE_PROJ4_STRCAT( " " );
    }
    
/* -------------------------------------------------------------------- */
/*      Handle linear units.                                            */
/* -------------------------------------------------------------------- */
    const char  *pszPROJ4Units=NULL;
    char        *pszLinearUnits = NULL;
    double      dfLinearConv;

    dfLinearConv = GetLinearUnits( &pszLinearUnits );
        
    if( strstr(szProj4,"longlat") != NULL )
        pszPROJ4Units = NULL;
    else 
    {
        pszPROJ4Units = LinearToProj4( dfLinearConv, pszLinearUnits );

        if( pszPROJ4Units == NULL )
        {
            char szLinearConv[128];
            sprintf( szLinearConv, "%.16g", dfLinearConv );
            SAFE_PROJ4_STRCAT( "+to_meter=" );
            SAFE_PROJ4_STRCAT( szLinearConv );
            SAFE_PROJ4_STRCAT( " " );
        }
    }

    if( pszPROJ4Units != NULL )
    {
        SAFE_PROJ4_STRCAT( "+units=");
        SAFE_PROJ4_STRCAT( pszPROJ4Units );
        SAFE_PROJ4_STRCAT( " " );
    }

/* -------------------------------------------------------------------- */
/*      If we have vertical datum grids, attach them to the proj.4 string.*/
/* -------------------------------------------------------------------- */
    const char *pszProj4Geoids = GetExtension( "VERT_DATUM", "PROJ4_GRIDS" );
    
    if( pszProj4Geoids != NULL )
    {
        SAFE_PROJ4_STRCAT( "+geoidgrids=" );
        SAFE_PROJ4_STRCAT( pszProj4Geoids );
        SAFE_PROJ4_STRCAT(  " " );
    }    

/* -------------------------------------------------------------------- */
/*      Handle vertical units, but only if we have them.                */
/* -------------------------------------------------------------------- */
    const OGR_SRSNode *poVERT_CS = GetRoot()->GetNode( "VERT_CS" );
    const OGR_SRSNode *poVUNITS = NULL;

    if( poVERT_CS != NULL )
        poVUNITS = poVERT_CS->GetNode( "UNIT" );

    if( poVUNITS != NULL && poVUNITS->GetChildCount() >= 2 )
    {
        pszPROJ4Units = NULL;

        dfLinearConv = CPLAtof( poVUNITS->GetChild(1)->GetValue() );
        
        pszPROJ4Units = LinearToProj4( dfLinearConv,
                                       poVUNITS->GetChild(0)->GetValue() );
            
        if( pszPROJ4Units == NULL )
        {
            char szLinearConv[128];
            sprintf( szLinearConv, "%.16g", dfLinearConv );
            SAFE_PROJ4_STRCAT( "+vto_meter=" );
            SAFE_PROJ4_STRCAT( szLinearConv );
            SAFE_PROJ4_STRCAT( " " );
        }
        else
        {
            SAFE_PROJ4_STRCAT( "+vunits=");
            SAFE_PROJ4_STRCAT( pszPROJ4Units );
            SAFE_PROJ4_STRCAT( " " );
        }
    }

/* -------------------------------------------------------------------- */
/*      Add the no_defs flag to ensure that no values from              */
/*      proj_def.dat are implicitly used with our definitions.          */
/* -------------------------------------------------------------------- */
    SAFE_PROJ4_STRCAT( "+no_defs " );
    
    *ppszProj4 = CPLStrdup( szProj4 );

    return OGRERR_NONE;
}


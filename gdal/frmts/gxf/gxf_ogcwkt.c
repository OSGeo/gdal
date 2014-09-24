/******************************************************************************
 * $Id$
 *
 * Project:  GXF Reader
 * Purpose:  Handle GXF to OGC WKT projection transformation.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2009-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "gxfopen.h"

CPL_CVSID("$Id$");

/* -------------------------------------------------------------------- */
/* the following #defines come from ogr_spatialref.h in the GDAL/OGR	*/
/* distribution (see http://gdal.velocet.ca/projects/opengis) and	*/
/* should be kept in sync with that file. 				*/
/* -------------------------------------------------------------------- */

#define SRS_PT_ALBERS_CONIC_EQUAL_AREA					\
				"Albers_Conic_Equal_Area"
#define SRS_PT_AZIMUTHAL_EQUIDISTANT "Azimuthal_Equidistant"
#define SRS_PT_CASSINI_SOLDNER	"Cassini_Soldner"
#define SRS_PT_CYLINDRICAL_EQUAL_AREA "Cylindrical_Equal_Area"
#define SRS_PT_ECKERT_IV        "Eckert_IV"
#define SRS_PT_ECKERT_VI        "Eckert_VI"
#define SRS_PT_EQUIDISTANT_CONIC "Equidistant_Conic"
#define SRS_PT_EQUIRECTANGULAR  "Equirectangular"
#define SRS_PT_GALL_STEREOGRAPHIC "Gall_Stereographic"
#define SRS_PT_GNOMONIC		"Gnomonic"
#define SRS_PT_HOTINE_OBLIQUE_MERCATOR 					\
                                "Hotine_Oblique_Mercator"
#define SRS_PT_LABORDE_OBLIQUE_MERCATOR 				\
                                "Laborde_Oblique_Mercator"
#define SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP				\
                                "Lambert_Conformal_Conic_1SP"
#define SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP			        \
                                "Lambert_Conformal_Conic_2SP"
#define SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM		        \
                                "Lambert_Conformal_Conic_2SP_Belgium"
#define SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA		        \
                                "Lambert_Azimuthal_Equal_Area"
#define SRS_PT_MERCATOR_1SP     "Mercator_1SP"
#define SRS_PT_MERCATOR_2SP     "Mercator_2SP"
#define SRS_PT_MILLER_CYLINDRICAL "Miller_Cylindrical"
#define SRS_PT_MOLLWEIDE        "Mollweide"
#define SRS_PT_NEW_ZEALAND_MAP_GRID					\
                                "New_Zealand_Map_Grid"
#define SRS_PT_OBLIQUE_STEREOGRAPHIC					\
                                "Oblique_Stereographic"
#define SRS_PT_ORTHOGRAPHIC	"Orthographic"
#define SRS_PT_POLAR_STEREOGRAPHIC					\
                                "Polar_Stereographic"
#define SRS_PT_POLYCONIC	"Polyconic"
#define SRS_PT_ROBINSON         "Robinson"
#define SRS_PT_SINUSOIDAL	"Sinusoidal"
#define SRS_PT_STEREOGRAPHIC	"Stereographic"
#define SRS_PT_SWISS_OBLIQUE_CYLINDRICAL 				\
                                "Swiss_Oblique_Cylindrical"
#define SRS_PT_TRANSVERSE_MERCATOR					\
                                "Transverse_Mercator"
#define SRS_PT_TRANSVERSE_MERCATOR_SOUTH_ORIENTED			\
                                "Transverse_Mercator_South_Orientated"
#define SRS_PT_TUNISIA_MINING_GRID					\
                                "Tunisia_Mining_Grid"
#define SRS_PT_VANDERGRINTEN	"VanDerGrinten"

#define SRS_PP_CENTRAL_MERIDIAN		"central_meridian"
#define SRS_PP_SCALE_FACTOR     	"scale_factor"
#define SRS_PP_STANDARD_PARALLEL_1	"standard_parallel_1"
#define SRS_PP_STANDARD_PARALLEL_2	"standard_parallel_2"
#define SRS_PP_LONGITUDE_OF_CENTER      "longitude_of_center"
#define SRS_PP_LATITUDE_OF_CENTER       "latitude_of_center"
#define SRS_PP_LONGITUDE_OF_ORIGIN      "longitude_of_origin"
#define SRS_PP_LATITUDE_OF_ORIGIN       "latitude_of_origin"
#define SRS_PP_FALSE_EASTING		"false_easting"
#define SRS_PP_FALSE_NORTHING		"false_northing"
#define SRS_PP_AZIMUTH			"azimuth"
#define SRS_PP_LONGITUDE_OF_POINT_1     "longitude_of_point_1"
#define SRS_PP_LATITUDE_OF_POINT_1      "latitude_of_point_1"
#define SRS_PP_LONGITUDE_OF_POINT_2     "longitude_of_point_2"
#define SRS_PP_LATITUDE_OF_POINT_2      "latitude_of_point_2"
#define SRS_PP_LONGITUDE_OF_POINT_3     "longitude_of_point_3"
#define SRS_PP_LATITUDE_OF_POINT_3      "latitude_of_point_3"
#define SRS_PP_RECTIFIED_GRID_ANGLE	"rectified_grid_angle"

/* -------------------------------------------------------------------- */
/*      This table was copied from gt_wkt_srs.cpp in the libgeotiff     */
/*      distribution.  Please keep changes in sync.                     */
/* -------------------------------------------------------------------- */
static char *papszDatumEquiv[] =
{
    "Militar_Geographische_Institut",
    "Militar_Geographische_Institute",
    "World_Geodetic_System_1984",
    "WGS_1984",
    "WGS_72_Transit_Broadcast_Ephemeris",
    "WGS_1972_Transit_Broadcast_Ephemeris",
    "World_Geodetic_System_1972",
    "WGS_1972",
    "European_Terrestrial_Reference_System_89",
    "European_Reference_System_1989",
    NULL
};

/************************************************************************/
/*                          WKTMassageDatum()                           */
/*                                                                      */
/*      Massage an EPSG datum name into WMT format.  Also transform     */
/*      specific exception cases into WKT versions.                     */
/*                                                                      */
/*      This function was copied from the gt_wkt_srs.cpp file in the    */
/*      libgeotiff distribution.  Please keep changes in sync.          */
/************************************************************************/

static void WKTMassageDatum( char ** ppszDatum )

{
    int		i, j;
    char	*pszDatum;

    pszDatum = *ppszDatum;
    if (pszDatum[0] == '\0')
        return;

/* -------------------------------------------------------------------- */
/*      Translate non-alphanumeric values to underscores.               */
/* -------------------------------------------------------------------- */
    for( i = 0; pszDatum[i] != '\0'; i++ )
    {
        if( pszDatum[i] != '+'
            && !(pszDatum[i] >= 'A' && pszDatum[i] <= 'Z')
            && !(pszDatum[i] >= 'a' && pszDatum[i] <= 'z')
            && !(pszDatum[i] >= '0' && pszDatum[i] <= '9') )
        {
            pszDatum[i] = '_';
        }
    }

/* -------------------------------------------------------------------- */
/*      Remove repeated and trailing underscores.                       */
/* -------------------------------------------------------------------- */
    for( i = 1, j = 0; pszDatum[i] != '\0'; i++ )
    {
        if( pszDatum[j] == '_' && pszDatum[i] == '_' )
            continue;

        pszDatum[++j] = pszDatum[i];
    }
    if( pszDatum[j] == '_' )
        pszDatum[j] = '\0';
    else
        pszDatum[j+1] = '\0';
    
/* -------------------------------------------------------------------- */
/*      Search for datum equivelences.  Specific massaged names get     */
/*      mapped to OpenGIS specified names.                              */
/* -------------------------------------------------------------------- */
    for( i = 0; papszDatumEquiv[i] != NULL; i += 2 )
    {
        if( EQUAL(*ppszDatum,papszDatumEquiv[i]) )
        {
            CPLFree( *ppszDatum );
            *ppszDatum = CPLStrdup( papszDatumEquiv[i+1] );
            return;
        }
    }
}

/************************************************************************/
/*                           OGCWKTSetProj()                            */
/************************************************************************/

static void OGCWKTSetProj( char * pszProjection, char ** papszMethods,
                           const char * pszTransformName,
                           const char * pszParm1, 
                           const char * pszParm2, 
                           const char * pszParm3, 
                           const char * pszParm4, 
                           const char * pszParm5, 
                           const char * pszParm6,
                           const char * pszParm7 )

{
    int		iParm, nCount = CSLCount(papszMethods);
    const char	*apszParmNames[8];

    apszParmNames[0] = pszParm1;
    apszParmNames[1] = pszParm2;
    apszParmNames[2] = pszParm3;
    apszParmNames[3] = pszParm4;
    apszParmNames[4] = pszParm5;
    apszParmNames[5] = pszParm6;
    apszParmNames[6] = pszParm7;
    apszParmNames[7] = NULL;

    sprintf( pszProjection,
             "PROJECTION[\"%s\"]",
             pszTransformName );

    for( iParm = 0; iParm < nCount-1 && apszParmNames[iParm] != NULL; iParm++ )
    {
        sprintf( pszProjection + strlen(pszProjection),
                 ",PARAMETER[\"%s\",%s]",
                 apszParmNames[iParm],
                 papszMethods[iParm+1] );
    }
}


/************************************************************************/
/*                    GXFGetMapProjectionAsOGCWKT()                     */
/************************************************************************/

/**
 * Return the GXF Projection in OpenGIS Well Known Text format.
 *
 * The returned string becomes owned by the caller, and should be freed
 * with CPLFree() or VSIFree().  The return value will be "" if
 * no projection information is passed.
 *
 * The mapping of GXF projections to OGC WKT format is not complete.  Please
 * see the gxf_ogcwkt.c code to better understand limitations of this
 * translation.  More information about OGC WKT format can be found in
 * the OpenGIS Simple Features specification for OLEDB/COM found on the
 * OpenGIS web site at <a href="http://www.opengis.org/">www.opengis.org</a>.
 * The translation uses some code cribbed from the OGR library, about which
 * more can be learned from <a href="http://gdal.velocet.ca/projects/opengis/">
 * http://gdal.velocet.ca/projects/opengis/</a>.
 *
 * For example, the following GXF definitions:
 * <pre>
 * #UNIT_LENGTH                        
 * m,1
 * #MAP_PROJECTION
 * "NAD83 / UTM zone 19N"
 * "GRS 1980",6378137,0.081819191,0
 * "Transverse Mercator",0,-69,0.9996,500000,0
 * </pre>
 *
 * Would translate to (without the nice formatting):
 * <pre>
   PROJCS["NAD83 / UTM zone 19N",
          GEOGCS["GRS 1980",
                 DATUM["GRS_1980",
                     SPHEROID["GRS 1980",6378137,298.257222413684]],
                 PRIMEM["unnamed",0],
                 UNIT["degree",0.0174532925199433]],
          PROJECTION["Transverse_Mercator"],
          PARAMETER["latitude_of_origin",0],
          PARAMETER["central_meridian",-69],
          PARAMETER["scale_factor",0.9996],
          PARAMETER["false_easting",500000],
          PARAMETER["false_northing",0],
          UNIT["m",1]]
 * </pre>
 *
 * @param hGXF handle to GXF file, as returned by GXFOpen().
 *
 * @return string containing OGC WKT projection.
 */

char *GXFGetMapProjectionAsOGCWKT( GXFHandle hGXF )

{
    GXFInfo_t	*psGXF = (GXFInfo_t *) hGXF;
    char	**papszMethods = NULL;
    char	szWKT[1024];
    char	szGCS[512];
    char	szProjection[512];

/* -------------------------------------------------------------------- */
/*      If there was nothing in the file return "unknown".              */
/* -------------------------------------------------------------------- */
    if( CSLCount(psGXF->papszMapProjection) < 2 )
        return( CPLStrdup( "" ) );

    strcpy( szWKT, "" );
    strcpy( szGCS, "" );
    strcpy( szProjection, "" );

/* -------------------------------------------------------------------- */
/*      Parse the third line, looking for known projection methods.     */
/* -------------------------------------------------------------------- */
    if( psGXF->papszMapProjection[2] != NULL )
    {
        /* We allow more than 80 characters if the projection parameters */
        /* are on 2 lines as allowed by GXF 3 */
        if( strlen(psGXF->papszMapProjection[2]) > 120 )
            return( CPLStrdup( "" ) );
        papszMethods = CSLTokenizeStringComplex(psGXF->papszMapProjection[2],
                                                ",", TRUE, TRUE );
    }

#ifdef DBMALLOC
    malloc_chain_check(1);
#endif    
    
/* -------------------------------------------------------------------- */
/*      Create the PROJCS.                                              */
/* -------------------------------------------------------------------- */
    if( papszMethods == NULL
        || papszMethods[0] == NULL 
        || EQUAL(papszMethods[0],"Geographic") )
    {
        /* do nothing */
    }

    else if( EQUAL(papszMethods[0],"Lambert Conic Conformal (1SP)") )
    {
        OGCWKTSetProj( szProjection, papszMethods,
                       SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP,
                       SRS_PP_LATITUDE_OF_ORIGIN,
                       SRS_PP_CENTRAL_MERIDIAN,
                       SRS_PP_SCALE_FACTOR,
                       SRS_PP_FALSE_EASTING,
                       SRS_PP_FALSE_NORTHING,
                       NULL,
                       NULL );
    }
    
    else if( EQUAL(papszMethods[0],"Lambert Conic Conformal (2SP)") )
    {
        OGCWKTSetProj( szProjection, papszMethods,
                       SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP,
                       SRS_PP_STANDARD_PARALLEL_1,
                       SRS_PP_STANDARD_PARALLEL_2,
                       SRS_PP_LATITUDE_OF_ORIGIN,
                       SRS_PP_CENTRAL_MERIDIAN,
                       SRS_PP_FALSE_EASTING,
                       SRS_PP_FALSE_NORTHING,
                       NULL );
    }
    
    else if( EQUAL(papszMethods[0],"Lambert Conformal (2SP Belgium)") )
    {
        OGCWKTSetProj( szProjection, papszMethods,
                       SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM,
                       SRS_PP_STANDARD_PARALLEL_1,
                       SRS_PP_STANDARD_PARALLEL_2,
                       SRS_PP_LATITUDE_OF_ORIGIN,
                       SRS_PP_CENTRAL_MERIDIAN,
                       SRS_PP_FALSE_EASTING,
                       SRS_PP_FALSE_NORTHING,
                       NULL );
    }
    
    else if( EQUAL(papszMethods[0],"Mercator (1SP)"))
    {
        OGCWKTSetProj( szProjection, papszMethods,
                       SRS_PT_MERCATOR_1SP,
                       SRS_PP_LATITUDE_OF_ORIGIN,
                       SRS_PP_CENTRAL_MERIDIAN,
                       SRS_PP_SCALE_FACTOR,
                       SRS_PP_FALSE_EASTING,
                       SRS_PP_FALSE_NORTHING,
                       NULL,
                       NULL );
    }
    
    else if( EQUAL(papszMethods[0],"Mercator (2SP)"))
    {
        OGCWKTSetProj( szProjection, papszMethods,
                       SRS_PT_MERCATOR_2SP,
                       SRS_PP_LATITUDE_OF_ORIGIN,/* should it be StdParalle1?*/
                       SRS_PP_CENTRAL_MERIDIAN,
                       SRS_PP_FALSE_EASTING,
                       SRS_PP_FALSE_NORTHING,
                       NULL,
                       NULL,
                       NULL );
    }
    
    else if( EQUAL(papszMethods[0],"Laborde Oblique Mercator") )
    {
        OGCWKTSetProj( szProjection, papszMethods,
                       SRS_PT_LABORDE_OBLIQUE_MERCATOR,
                       SRS_PP_LATITUDE_OF_CENTER,
                       SRS_PP_LONGITUDE_OF_CENTER,
                       SRS_PP_AZIMUTH,
                       SRS_PP_SCALE_FACTOR,
                       SRS_PP_FALSE_EASTING,
                       SRS_PP_FALSE_NORTHING,
                       NULL );

    }

    else if( EQUAL(papszMethods[0],"Hotine Oblique Mercator") )
    {
        OGCWKTSetProj( szProjection, papszMethods,
                       SRS_PT_HOTINE_OBLIQUE_MERCATOR,
                       SRS_PP_LATITUDE_OF_CENTER,
                       SRS_PP_LONGITUDE_OF_CENTER,
                       SRS_PP_AZIMUTH,
                       SRS_PP_RECTIFIED_GRID_ANGLE,
                       SRS_PP_SCALE_FACTOR, /* not in normal formulation */
                       SRS_PP_FALSE_EASTING,
                       SRS_PP_FALSE_NORTHING );
    }

    else if( EQUAL(papszMethods[0],"New Zealand Map Grid") )

    {
        OGCWKTSetProj( szProjection, papszMethods,
                       SRS_PT_NEW_ZEALAND_MAP_GRID,
                       SRS_PP_LATITUDE_OF_ORIGIN,
                       SRS_PP_CENTRAL_MERIDIAN,
                       SRS_PP_FALSE_EASTING,
                       SRS_PP_FALSE_NORTHING,
                       NULL,
                       NULL,
                       NULL );
    }
    
    else if( EQUAL(papszMethods[0],"Oblique Stereographic") )
    {
        OGCWKTSetProj( szProjection, papszMethods,
                       SRS_PT_OBLIQUE_STEREOGRAPHIC,
                       SRS_PP_LATITUDE_OF_ORIGIN,
                       SRS_PP_CENTRAL_MERIDIAN,
                       SRS_PP_SCALE_FACTOR,
                       SRS_PP_FALSE_EASTING,
                       SRS_PP_FALSE_NORTHING,
                       NULL,
                       NULL );
    }
    
    else if( EQUAL(papszMethods[0],"Polar Stereographic") )
    {
        OGCWKTSetProj( szProjection, papszMethods,
                       SRS_PT_POLAR_STEREOGRAPHIC,
                       SRS_PP_LATITUDE_OF_ORIGIN,
                       SRS_PP_CENTRAL_MERIDIAN,
                       SRS_PP_SCALE_FACTOR,
                       SRS_PP_FALSE_EASTING,
                       SRS_PP_FALSE_NORTHING,
                       NULL,
                       NULL );
    }
    
    else if( EQUAL(papszMethods[0],"Swiss Oblique Cylindrical") )
    {
        OGCWKTSetProj( szProjection, papszMethods,
                       SRS_PT_SWISS_OBLIQUE_CYLINDRICAL,
                       SRS_PP_LATITUDE_OF_CENTER,
                       SRS_PP_LONGITUDE_OF_CENTER,
                       SRS_PP_FALSE_EASTING,
                       SRS_PP_FALSE_NORTHING,
                       NULL,
                       NULL,
                       NULL );
    }
    
    else if( EQUAL(papszMethods[0],"Transverse Mercator") )
    {
        OGCWKTSetProj( szProjection, papszMethods,
                       SRS_PT_TRANSVERSE_MERCATOR,
                       SRS_PP_LATITUDE_OF_ORIGIN,
                       SRS_PP_CENTRAL_MERIDIAN,
                       SRS_PP_SCALE_FACTOR,
                       SRS_PP_FALSE_EASTING,
                       SRS_PP_FALSE_NORTHING,
                       NULL,
                       NULL );
    }

    else if( EQUAL(papszMethods[0],"Transverse Mercator (South Oriented)")
          || EQUAL(papszMethods[0],"Transverse Mercator (South Orientated)"))
    {
        OGCWKTSetProj( szProjection, papszMethods,
                       SRS_PT_TRANSVERSE_MERCATOR_SOUTH_ORIENTED,
                       SRS_PP_LATITUDE_OF_ORIGIN,
                       SRS_PP_CENTRAL_MERIDIAN,
                       SRS_PP_SCALE_FACTOR,
                       SRS_PP_FALSE_EASTING,
                       SRS_PP_FALSE_NORTHING,
                       NULL,
                       NULL );
    }

    else if( EQUAL(papszMethods[0],"*Albers Conic") )
    {
        OGCWKTSetProj( szProjection, papszMethods,
                       SRS_PT_ALBERS_CONIC_EQUAL_AREA,
                       SRS_PP_STANDARD_PARALLEL_1,
                       SRS_PP_STANDARD_PARALLEL_2,
                       SRS_PP_LATITUDE_OF_CENTER,
                       SRS_PP_LONGITUDE_OF_CENTER,
                       SRS_PP_FALSE_EASTING,
                       SRS_PP_FALSE_NORTHING,
                       NULL );
    }

    else if( EQUAL(papszMethods[0],"*Equidistant Conic") )
    {
        OGCWKTSetProj( szProjection, papszMethods,
                       SRS_PT_EQUIDISTANT_CONIC,
                       SRS_PP_STANDARD_PARALLEL_1,
                       SRS_PP_STANDARD_PARALLEL_2,
                       SRS_PP_LATITUDE_OF_CENTER,
                       SRS_PP_LONGITUDE_OF_CENTER,
                       SRS_PP_FALSE_EASTING,
                       SRS_PP_FALSE_NORTHING,
                       NULL );
    }

    else if( EQUAL(papszMethods[0],"*Polyconic") )
    {
        OGCWKTSetProj( szProjection, papszMethods,
                       SRS_PT_POLYCONIC,
                       SRS_PP_LATITUDE_OF_ORIGIN,
                       SRS_PP_CENTRAL_MERIDIAN,
                       SRS_PP_SCALE_FACTOR, /* not normally expected */
                       SRS_PP_FALSE_EASTING,
                       SRS_PP_FALSE_NORTHING,
                       NULL,
                       NULL );
    }

    CSLDestroy( papszMethods );

    
/* -------------------------------------------------------------------- */
/*      Extract the linear Units specification.                         */
/* -------------------------------------------------------------------- */
    if( psGXF->pszUnitName != NULL && strlen(szProjection) > 0 )
    {
        if( strlen(psGXF->pszUnitName) > 80 )
            return CPLStrdup("");

        sprintf( szProjection+strlen(szProjection),
                 ",UNIT[\"%s\",%.15g]",
                 psGXF->pszUnitName, psGXF->dfUnitToMeter );
    }
    
/* -------------------------------------------------------------------- */
/*      Build GEOGCS.  There are still "issues" with the generation     */
/*      of the GEOGCS/Datum and Spheroid names.  Of these, only the     */
/*      datum name is really significant.                               */
/* -------------------------------------------------------------------- */
    if( CSLCount(psGXF->papszMapProjection) > 1 )
    {
        char	**papszTokens;
        
        if( strlen(psGXF->papszMapProjection[1]) > 80 )
            return CPLStrdup("");
        
        papszTokens = CSLTokenizeStringComplex(psGXF->papszMapProjection[1],
                                               ",", TRUE, TRUE );


        if( CSLCount(papszTokens) > 2 )
        {
            double	dfMajor = atof(papszTokens[1]);
            double	dfEccentricity = atof(papszTokens[2]);
            double	dfInvFlattening, dfMinor;
            char	*pszOGCDatum;

            /* translate eccentricity to inv flattening. */
            if( dfEccentricity == 0.0 )
                dfInvFlattening = 0.0;
            else
            {
                dfMinor = dfMajor * pow(1.0-dfEccentricity*dfEccentricity,0.5);
                dfInvFlattening = 1.0 / (1 - dfMinor/dfMajor);
            }

            pszOGCDatum = CPLStrdup(papszTokens[0]);
            WKTMassageDatum( &pszOGCDatum );
            
            sprintf( szGCS,
                     "GEOGCS[\"%s\","
                       "DATUM[\"%s\","
                       "SPHEROID[\"%s\",%s,%.15g]],",
                     papszTokens[0],
                     pszOGCDatum,
                     papszTokens[0], /* this is datum, but should be ellipse*/
                     papszTokens[1],
                     dfInvFlattening );
            CPLFree( pszOGCDatum );
        }

        if( CSLCount(papszTokens) > 3 )
            sprintf( szGCS + strlen(szGCS),
                     "PRIMEM[\"unnamed\",%s],",
                     papszTokens[3] );
        
        strcat( szGCS, "UNIT[\"degree\",0.0174532925199433]]" );
        
        CSLDestroy( papszTokens );
    }
    
    CPLAssert(strlen(szProjection) < sizeof(szProjection));
    CPLAssert(strlen(szGCS) < sizeof(szGCS));

/* -------------------------------------------------------------------- */
/*      Put this all together into a full projection.                   */
/* -------------------------------------------------------------------- */
    if( strlen(szProjection) > 0 )
    {
        if( strlen(psGXF->papszMapProjection[0]) > 80 )
            return CPLStrdup("");

        if( psGXF->papszMapProjection[0][0] == '"' )
            sprintf( szWKT,
                     "PROJCS[%s,%s,%s]",
                     psGXF->papszMapProjection[0],
                     szGCS,
                     szProjection );
        else
            sprintf( szWKT,
                     "PROJCS[\"%s\",%s,%s]",
                     psGXF->papszMapProjection[0],
                     szGCS,
                     szProjection );
            
    }
    else
    {
        strcpy( szWKT, szGCS );
    }

    return( CPLStrdup( szWKT ) );
}


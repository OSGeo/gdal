/******************************************************************************
 * $Id$
 *
 * Name:     gdalprojdef.cpp
 * Project:  GDAL Core Projections
 * Purpose:  Implementation of the GDALProjDef class, a class abstracting
 *           the interface to PROJ.4 projection services.
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
 * $Log$
 * Revision 1.5  1999/07/29 19:09:23  warmerda
 * added windows support for proj.dll
 *
 * Revision 1.4  1999/07/29 18:01:31  warmerda
 * added support for translation of OGIS WKT projdefs to proj.4
 *
 * Revision 1.3  1999/03/02 21:09:48  warmerda
 * add GDALDecToDMS()
 *
 * Revision 1.2  1999/01/27 20:28:01  warmerda
 * Don't call CPLGetsymbol() for every function if first fails.
 *
 * Revision 1.1  1999/01/11 15:36:26  warmerda
 * New
 *
 */

#include "gdal_priv.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"

typedef struct { double u, v; }	UV;

#define PJ void

static PJ	*(*pfn_pj_init)(int, char**) = NULL;
static UV	(*pfn_pj_fwd)(UV, PJ *) = NULL;
static UV	(*pfn_pj_inv)(UV, PJ *) = NULL;
static void	(*pfn_pj_free)(PJ *) = NULL;

#define RAD_TO_DEG	57.29577951308232
#define DEG_TO_RAD	.0174532925199432958

#ifdef WIN32
#  define LIBNAME      "proj.dll"
#else
#  define LIBNAME      "libproj.so"
#endif

/************************************************************************/
/*                          LoadProjLibrary()                           */
/************************************************************************/

static int LoadProjLibrary()

{
    static int	bTriedToLoad = FALSE;
    
    if( bTriedToLoad )
        return( pfn_pj_init != NULL );

    bTriedToLoad = TRUE;
    
    pfn_pj_init = (PJ *(*)(int, char**)) CPLGetSymbol( LIBNAME,
                                                       "pj_init" );
    if( pfn_pj_init == NULL )
       return( FALSE );

    pfn_pj_fwd = (UV (*)(UV,PJ*)) CPLGetSymbol( LIBNAME, "pj_fwd" );
    pfn_pj_inv = (UV (*)(UV,PJ*)) CPLGetSymbol( LIBNAME, "pj_inv" );
    pfn_pj_free = (void (*)(PJ*)) CPLGetSymbol( LIBNAME, "pj_free" );

    return( TRUE );
}

/************************************************************************/
/*                            GDALProjDef()                             */
/************************************************************************/

GDALProjDef::GDALProjDef( const char * pszProjectionIn )

{
    if( pszProjectionIn == NULL )
        pszProjection = CPLStrdup( "" );
    else
        pszProjection = CPLStrdup( pszProjectionIn );

    psPJ = NULL;

    SetProjectionString( pszProjectionIn );
}

/************************************************************************/
/*                         GDALCreateProjDef()                          */
/************************************************************************/

GDALProjDefH GDALCreateProjDef( const char * pszProjection )

{
    return( (GDALProjDefH) (new GDALProjDef( pszProjection )) );
}

/************************************************************************/
/*                            ~GDALProjDef()                            */
/************************************************************************/

GDALProjDef::~GDALProjDef()

{
    CPLFree( pszProjection );
    if( psPJ != NULL )
        pfn_pj_free( psPJ );
}

/************************************************************************/
/*                         GDALDestroyProjDef()                         */
/************************************************************************/

void GDALDestroyProjDef( GDALProjDefH hProjDef )

{
    delete (GDALProjDef *) hProjDef;
}

/************************************************************************/
/*                        SetProjectionString()                         */
/************************************************************************/

CPLErr GDALProjDef::SetProjectionString( const char * pszProjectionIn )

{
    char	**args;

    if( psPJ != NULL && pfn_pj_free != NULL )
    {
        pfn_pj_free( psPJ );
        psPJ = NULL;
    }

    if( pszProjection != NULL )
        CPLFree( pszProjection );
    
    pszProjection = CPLStrdup( pszProjectionIn );

    if( !LoadProjLibrary() )
    {
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      If this is an OGIS string we will try and translate it to       */
/*      PROJ.4 format.                                                  */
/* -------------------------------------------------------------------- */
    char	*pszProj4Projection = NULL;
    
    if( EQUALN(pszProjection,"PROJCS",6) || EQUALN(pszProjection,"GEOGCS",6) )
    {
        OGRSpatialReference 	oSRS;
        char			*pszProjRef = pszProjection;

        if( oSRS.importFromWkt( &pszProjRef ) != OGRERR_NONE
            || oSRS.exportToProj4( &pszProj4Projection ) != OGRERR_NONE )
            return CE_Failure;
    }
    else
    {
        pszProj4Projection = CPLStrdup( pszProjection );
    }

/* -------------------------------------------------------------------- */
/*      Tokenize, and pass tokens to PROJ.4 initialization function.    */
/* -------------------------------------------------------------------- */
    args = CSLTokenizeStringComplex( pszProj4Projection, " +", TRUE, FALSE );

    psPJ = pfn_pj_init( CSLCount(args), args );

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
    CSLDestroy( args );
    CPLFree( pszProj4Projection );

    if( psPJ == NULL )
        return CE_Failure;
    else
        return CE_None;
}

/************************************************************************/
/*                             ToLongLat()                              */
/************************************************************************/

CPLErr GDALProjDef::ToLongLat( double * padfX, double * padfY )

{
    UV		uv;
    
    CPLAssert( padfX != NULL && padfY != NULL );
    
    if( psPJ == NULL )
        return CE_Failure;

    if( strstr(pszProjection,"+proj=longlat") != NULL
        || strstr(pszProjection,"+proj=latlong") != NULL )
        return CE_None;

    uv.u = *padfX;
    uv.v = *padfY;

    uv = pfn_pj_inv( uv, psPJ );

    *padfX = uv.u * RAD_TO_DEG;
    *padfY = uv.v * RAD_TO_DEG;
    
    return( CE_None );
}

/************************************************************************/
/*                       GDALReprojectToLongLat()                       */
/************************************************************************/

CPLErr GDALReprojectToLongLat( GDALProjDefH hProjDef,
                               double * padfX, double * padfY )

{
    return( ((GDALProjDef *) hProjDef)->ToLongLat(padfX, padfY) );
}

/************************************************************************/
/*                            FromLongLat()                             */
/************************************************************************/

CPLErr GDALProjDef::FromLongLat( double * padfX, double * padfY )

{
    UV		uv;
    
    CPLAssert( padfX != NULL && padfY != NULL );
    
    if( psPJ == NULL )
        return CE_Failure;

    if( strstr(pszProjection,"+proj=longlat") != NULL
        || strstr(pszProjection,"+proj=latlong") != NULL )
        return CE_None;

    uv.u = *padfX * DEG_TO_RAD;
    uv.v = *padfY * DEG_TO_RAD;

    uv = pfn_pj_fwd( uv, psPJ );

    *padfX = uv.u;
    *padfY = uv.v;
    
    return( CE_None );
}

/************************************************************************/
/*                      GDALReprojectFromLongLat()                      */
/************************************************************************/

CPLErr GDALReprojectFromLongLat( GDALProjDefH hProjDef,
                                 double * padfX, double * padfY )

{
    return( ((GDALProjDef *) hProjDef)->FromLongLat(padfX, padfY) );
}

/************************************************************************/
/*                            GDALDecToDMS()                            */
/*                                                                      */
/*      Translate a decimal degrees value to a DMS string with          */
/*      hemisphere.                                                     */
/************************************************************************/

const char *GDALDecToDMS( double dfAngle, const char * pszAxis,
                          int nPrecision )

{
    int		nDegrees, nMinutes;
    double	dfSeconds;
    char	szFormat[30];
    static char szBuffer[50];
    const char	*pszHemisphere;
    

    nDegrees = (int) ABS(dfAngle);
    nMinutes = (int) ((ABS(dfAngle) - nDegrees) * 60);
    dfSeconds = (ABS(dfAngle) * 3600 - nDegrees*3600 - nMinutes*60);

    if( EQUAL(pszAxis,"Long") && dfAngle < 0.0 )
        pszHemisphere = "W";
    else if( EQUAL(pszAxis,"Long") )
        pszHemisphere = "E";
    else if( dfAngle < 0.0 )
        pszHemisphere = "S";
    else
        pszHemisphere = "N";

    sprintf( szFormat, "%%3dd%%2d\'%%.%df\"%s", nPrecision, pszHemisphere );
    sprintf( szBuffer, szFormat, nDegrees, nMinutes, dfSeconds );

    return( szBuffer );
}


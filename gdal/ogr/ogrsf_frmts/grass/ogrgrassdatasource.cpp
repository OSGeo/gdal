/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGRASSDataSource class.
 * Author:   Radim Blazek, radim.blazek@gmail.com 
 *
 ******************************************************************************
 * Copyright (c) 2005, Radim Blazek <radim.blazek@gmail.com>
 * Copyright (c) 2008-2010, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogrgrass.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                         Grass2CPLErrorHook()                         */
/************************************************************************/
int Grass2OGRErrorHook( char * pszMessage, int bFatal )
{
    if( !bFatal )
        CPLError( CE_Warning, CPLE_AppDefined, "GRASS warning: %s", pszMessage );
    else
        CPLError( CE_Warning, CPLE_AppDefined, "GRASS fatal error: %s", pszMessage );

    return 0;
}

/************************************************************************/
/*                         OGRGRASSDataSource()                         */
/************************************************************************/
OGRGRASSDataSource::OGRGRASSDataSource()
{
    pszName = NULL;
    pszGisdbase = NULL;
    pszLocation = NULL;
    pszMapset = NULL;
    pszMap = NULL;
    papoLayers = NULL;
    nLayers = 0;
    bOpened = FALSE;
}

/************************************************************************/
/*                        ~OGRGRASSDataSource()                         */
/************************************************************************/
OGRGRASSDataSource::~OGRGRASSDataSource()
{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    
    if ( pszName ) CPLFree( pszName );
    if ( papoLayers ) CPLFree( papoLayers );
    if ( pszGisdbase ) G_free( pszGisdbase );
    if ( pszLocation ) G_free( pszLocation );
    if ( pszMapset ) G_free( pszMapset );
    if ( pszMap ) G_free( pszMap );
    
    if (bOpened)
        Vect_close(&map);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

#if (GRASS_VERSION_MAJOR  >= 6 && GRASS_VERSION_MINOR  >= 3) || GRASS_VERSION_MAJOR  >= 7 
typedef int (*GrassErrorHandler)(const char *, int);
#else
typedef int (*GrassErrorHandler)(char *, int);
#endif

int OGRGRASSDataSource::Open( const char * pszNewName, int bUpdate,
                              int bTestOpen, int bSingleNewFileIn )
{
    VSIStatBuf  stat;
    
    CPLAssert( nLayers == 0 );
    
    pszName = CPLStrdup( pszNewName ); // Released by destructor

/* -------------------------------------------------------------------- */
/*      Do the given path contains 'vector' and 'head'?                 */
/* -------------------------------------------------------------------- */
    if ( strstr(pszName,"vector") == NULL || strstr(pszName,"head") == NULL )
    {
        if( !bTestOpen )
	{
            CPLError( CE_Failure, CPLE_AppDefined,
                 "%s is not GRASS vector, access failed.\n", pszName );
	}
	return FALSE;
    }
    
/* -------------------------------------------------------------------- */
/*      Is the given a regular file?                                    */
/* -------------------------------------------------------------------- */
    if( CPLStat( pszName, &stat ) != 0 || !VSI_ISREG(stat.st_mode) )
    {
        if( !bTestOpen )
	{
            CPLError( CE_Failure, CPLE_AppDefined,
                 "%s is not GRASS vector, access failed.\n", pszName );
	}

        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Parse datasource name                                           */
/* -------------------------------------------------------------------- */
    if ( !SplitPath(pszName, &pszGisdbase, &pszLocation, 
		    &pszMapset, &pszMap) ) 
    {
        if( !bTestOpen )
	{
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s is not GRASS datasource name, access failed.\n", 
		      pszName );
	}
	return FALSE;
    }
			
    CPLDebug ( "GRASS", "Gisdbase: %s", pszGisdbase );
    CPLDebug ( "GRASS", "Location: %s", pszLocation );
    CPLDebug ( "GRASS", "Mapset: %s", pszMapset );
    CPLDebug ( "GRASS", "Map: %s", pszMap );

/* -------------------------------------------------------------------- */
/*      Init GRASS library                                              */
/* -------------------------------------------------------------------- */
    // GISBASE is path to the directory where GRASS is installed,
    // it is necessary because there are database drivers.
    if ( !getenv( "GISBASE" ) ) {
        static char* gisbaseEnv = NULL;
        const char *gisbase = GRASS_GISBASE;
        CPLError( CE_Warning, CPLE_AppDefined, "GRASS warning: GISBASE "
                "enviroment variable was not set, using:\n%s", gisbase );
        char buf[2000];
        snprintf ( buf, sizeof(buf), "GISBASE=%s", gisbase );
        buf[sizeof(buf)-1] = '\0';

        CPLFree(gisbaseEnv);
        gisbaseEnv = CPLStrdup ( buf );
        putenv( gisbaseEnv );
    }

    // Don't use GISRC file and read/write GRASS variables 
    // (from location G_VAR_GISRC) to memory only.
    G_set_gisrc_mode ( G_GISRC_MODE_MEMORY );

    // Init GRASS libraries (required). G_no_gisinit() doesn't check 
    // write permissions for mapset compare to G_gisinit()
    G_no_gisinit();  

    // Set error function
    G_set_error_routine ( (GrassErrorHandler) Grass2OGRErrorHook );

/* -------------------------------------------------------------------- */
/*      Set GRASS variables                                             */
/* -------------------------------------------------------------------- */
     G__setenv( "GISDBASE", pszGisdbase );
     G__setenv( "LOCATION_NAME", pszLocation );
     G__setenv( "MAPSET", pszMapset); 
     G_reset_mapsets();
     G_add_mapset_to_search_path ( pszMapset );

/* -------------------------------------------------------------------- */
/*      Open GRASS vector map                                           */
/* -------------------------------------------------------------------- */
#if GRASS_VERSION_MAJOR  < 7
    Vect_set_fatal_error ( GV_FATAL_PRINT ); // Print error and continue
#endif
    Vect_set_open_level (2);
    int level = Vect_open_old ( &map, pszMap, pszMapset);

    if ( level < 2 ) {
        CPLError( CE_Failure, CPLE_AppDefined,
                 "Cannot open GRASS vector %s on level 2.\n", pszName );
	return FALSE;
    }

    CPLDebug ( "GRASS", "Num lines = %d", Vect_get_num_lines(&map) );
    
/* -------------------------------------------------------------------- */
/*      Build a list of layers.                                         */
/* -------------------------------------------------------------------- */
    int ncidx = Vect_cidx_get_num_fields ( &map );
    CPLDebug ( "GRASS", "Num layers = %d", ncidx );

    for ( int i = 0; i < ncidx; i++ ) {
	// Create the layer object
	OGRGRASSLayer       *poLayer;

        poLayer = new OGRGRASSLayer( i, &map );
	
        // Add layer to data source layer list
	papoLayers = (OGRGRASSLayer **)
	    CPLRealloc( papoLayers,  sizeof(OGRGRASSLayer *) * (nLayers+1) );
	papoLayers[nLayers++] = poLayer;
    }
    
    bOpened = TRUE;
    
    return TRUE;
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/
OGRLayer *
OGRGRASSDataSource::CreateLayer( const char * pszLayerName,
                                 OGRSpatialReference *poSRS,
                                 OGRwkbGeometryType eType,
                                 char ** papszOptions )

{
    CPLError( CE_Failure, CPLE_NoWriteAccess,
	      "CreateLayer is not supported by GRASS driver" );

    return NULL;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/
int OGRGRASSDataSource::TestCapability( const char * pszCap )
{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/
OGRLayer *OGRGRASSDataSource::GetLayer( int iLayer )
{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                            SplitPath()                               */
/* Split full path to cell or group to:                                 */
/*     gisdbase, location, mapset, name                                 */
/* New string are allocated and should be freed when no longer needed.  */
/*                                                                      */
/* Returns: true - OK                                                   */
/*          false - failed                                              */
/************************************************************************/
bool OGRGRASSDataSource::SplitPath( char *path, char **gisdbase, 
	                     char **location, char **mapset, char **map )
{
    char *p, *ptr[5], *tmp;
    int  i = 0;
    
    CPLDebug ( "GRASS", "OGRGRASSDataSource::SplitPath" );
    
    *gisdbase = *location = *mapset = *map = NULL;
    
    if ( !path || strlen(path) == 0 ) 
	return false;

    tmp = G_store ( path );

    while ( (p = strrchr(tmp,'/')) != NULL  && i < 5 ) {
	*p = '\0';
	
	if ( strlen(p+1) == 0 ) /* repeated '/' */
	    continue;

	ptr[i++] = p+1;
    }

    /* Note: empty GISDBASE == 0 is not accepted (relative path) */
    if ( i != 5 ) {
        free ( tmp );
	return false;
    }

    if ( strcmp(ptr[0],"head") != 0 || strcmp(ptr[2],"vector") != 0 ) {
       return false;
    }       

    *gisdbase = G_store ( tmp );
    *location = G_store ( ptr[4] );
    *mapset   = G_store ( ptr[3] );
    *map      = G_store ( ptr[1] );

    free ( tmp );
    return true;
}


/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRShapeDataSource class.
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
 * Revision 1.1  1999/11/04 21:16:11  warmerda
 * New
 *
 */

#include "ogrshape.h"
#include "cpl_conv.h"
#include "cpl_string.h"

/************************************************************************/
/*                         OGRShapeDataSource()                         */
/************************************************************************/

OGRShapeDataSource::OGRShapeDataSource()

{
    pszName = NULL;
    papoLayers = NULL;
    nLayers = 0;
}

/************************************************************************/
/*                        ~OGRShapeDataSource()                         */
/************************************************************************/

OGRShapeDataSource::~OGRShapeDataSource()

{
    CPLFree( pszName );

    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    
    CPLFree( papoLayers );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRShapeDataSource::Open( const char * pszNewName, int bUpdate,
                              int bTestOpen )

{
    VSIStatBuf	stat;
    
    CPLAssert( nLayers == 0 );
    
    pszName = CPLStrdup( pszNewName );

    bDSUpdate = bUpdate;
    
/* -------------------------------------------------------------------- */
/*      Is the given path a directory or a regular file?                */
/* -------------------------------------------------------------------- */
    if( VSIStat( pszNewName, &stat ) != 0 
        || (!VSI_ISDIR(stat.st_mode) && !VSI_ISREG(stat.st_mode)) )
    {
        if( !bTestOpen )
            CPLError( CE_Failure, CPLE_AppDefined,
                   "%s is neither a file or directory, Shape access failed.\n",
                      pszNewName );

        return FALSE;
    }
    
/* -------------------------------------------------------------------- */
/*      Build a list of filenames we figure are Tiger files.            */
/* -------------------------------------------------------------------- */
    if( VSI_ISREG(stat.st_mode) )
    {
        if( !OpenFile( pszNewName, bUpdate, bTestOpen ) )
        {
            if( !bTestOpen )
                CPLError( CE_Failure, CPLE_OpenFailed,
                          "Failed to open shapefile %s.\n"
                          "It may be corrupt.\n",
                          pszNewName );

            return FALSE;
        }

        return TRUE;
    }
    else
    {
        char      **papszCandidates = CPLReadDir( pszNewName );

        for( int iCan = 0; iCan < CSLCount(papszCandidates); iCan++ )
        {
            char	*pszFilename;
            const char  *pszCandidate = papszCandidates[iCan];

            if( strlen(pszCandidate) < 4
                || !EQUAL(pszCandidate+strlen(pszCandidate)-4,".shp") )
                continue;

            pszFilename =
                CPLStrdup(CPLFormFilename(pszNewName, pszCandidate, NULL));

            if( !OpenFile( pszFilename, bUpdate, bTestOpen )
                && !bTestOpen )
            {
                CPLError( CE_Failure, CPLE_OpenFailed,
                          "Failed to open shapefile %s.\n"
                          "It may be corrupt.\n",
                          pszFilename );
                return FALSE;
            }
            
            CPLFree( pszFilename );
        }

        CSLDestroy( papszCandidates );
        
        if( !bTestOpen && nLayers == 0 && !bUpdate )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "No Shapefiles found in directory %s\n",
                      pszNewName );
        }
    }

    return nLayers > 0 || bUpdate;
}

/************************************************************************/
/*                              OpenFile()                              */
/************************************************************************/

int OGRShapeDataSource::OpenFile( const char *pszNewName, int bUpdate,
                                  int bTestOpen )

{
    SHPHandle	hSHP;
    DBFHandle	hDBF;

/* -------------------------------------------------------------------- */
/*      SHPOpen() should include better (CPL based) error reporting,    */
/*      and we should be trying to distinquish at this point whether    */
/*      failure is a result of trying to open a non-shapefile, or       */
/*      whether it was a shapefile and we want to report the error up.  */
/* -------------------------------------------------------------------- */
    if( bUpdate )
        hSHP = SHPOpen( pszNewName, "r+" );
    else
        hSHP = SHPOpen( pszNewName, "r" );

    if( hSHP == NULL )
        return FALSE;
    
/* -------------------------------------------------------------------- */
/*      Open the .dbf file, if it exists.                               */
/* -------------------------------------------------------------------- */
    if( bUpdate )
        hDBF = DBFOpen( pszNewName, "r+" );
    else
        hDBF = DBFOpen( pszNewName, "r" );

/* -------------------------------------------------------------------- */
/*      Extract the basename of the file.                               */
/* -------------------------------------------------------------------- */
    char	*pszBasename;

    pszBasename = CPLStrdup(CPLGetBasename(pszNewName));

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRShapeLayer	*poLayer;

    poLayer = new OGRShapeLayer( pszBasename, hSHP, hDBF, bUpdate );
    CPLFree( pszBasename );

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRShapeLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRShapeLayer *) * (nLayers+1) );
    papoLayers[nLayers++] = poLayer;
    
    return TRUE;
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRShapeDataSource::CreateLayer( const char * pszLayerName,
                                 OGRSpatialReference *,
                                 OGRwkbGeometryType eType,
                                 char ** papszOptions )

{
    SHPHandle	hSHP;
    DBFHandle	hDBF;
    int		nShapeType;

/* -------------------------------------------------------------------- */
/*      Verify we are in update mode.                                   */
/* -------------------------------------------------------------------- */
    if( !bDSUpdate )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Data source %s opened read-only.\n"
                  "New layer %s cannot be created.\n",
                  pszName, pszLayerName );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Figure out what type of layer we need.                          */
/* -------------------------------------------------------------------- */
    if( eType == wkbUnknown || eType == wkbLineString )
        nShapeType = SHPT_ARC;
    else if( eType == wkbPoint )
        nShapeType = SHPT_POINT;
    else if( eType == wkbPolygon )
        nShapeType = SHPT_POLYGON;
    else if( eType == wkbMultiPoint )
        nShapeType = SHPT_MULTIPOINT;
    else if( eType == wkbPoint25D )
        nShapeType = SHPT_POINTZ;
    else if( eType == wkbLineString25D )
        nShapeType = SHPT_ARCZ;
    else
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Geometry type of `%d' not supported in shapefiles.\n",
                  (int) eType );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Create the shapefile.                                           */
/* -------------------------------------------------------------------- */
    char	*pszFilename =
        CPLStrdup(CPLFormFilename(pszName,pszLayerName,".shp"));
    
    hSHP = SHPCreate( pszFilename, nShapeType );

    if( hSHP == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to open Shapefile `%s'.\n",
                  pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a DBF file.                                              */
/* -------------------------------------------------------------------- */
    hDBF = DBFCreate( CPLFormFilename( pszName, pszLayerName, ".dbf" ) );

    if( hDBF == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to open Shape DBF file `%s'.\n",
                  CPLFormFilename( pszName, pszLayerName, ".dbf" ) );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRShapeLayer	*poLayer;

    poLayer = new OGRShapeLayer( pszLayerName, hSHP, hDBF, TRUE );

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRShapeLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRShapeLayer *) * (nLayers+1) );
    
    papoLayers[nLayers++] = poLayer;

    return poLayer;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRShapeDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRShapeDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

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
 * Revision 1.8  2001/12/11 21:45:36  warmerda
 * fixed unlikely memory leak of pszWKT
 *
 * Revision 1.7  2001/09/04 15:35:14  warmerda
 * add support for deferring geometry type selection till first feature
 *
 * Revision 1.6  2001/07/18 04:55:16  warmerda
 * added CPL_CSVID
 *
 * Revision 1.5  2001/03/16 22:16:10  warmerda
 * added support for ESRI .prj files
 *
 * Revision 1.4  2000/11/02 16:26:12  warmerda
 * fixed geometry type message
 *
 * Revision 1.3  2000/08/29 15:11:47  warmerda
 * added Z types for SHPT
 *
 * Revision 1.2  2000/03/14 21:37:16  warmerda
 * added SHPT= option support
 *
 * Revision 1.1  1999/11/04 21:16:11  warmerda
 * New
 *
 */

#include "ogrshape.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

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
/*      Is there an associated .prj file we can read?                   */
/* -------------------------------------------------------------------- */
    OGRSpatialReference *poSRS = NULL;
    const char  *pszPrjFile = CPLResetExtension( pszNewName, "prj" );
    FILE	*fp;

    fp = VSIFOpen( pszPrjFile, "r" );
    if( fp != NULL )
    {
        char	**papszLines;

        VSIFClose( fp );
        
        papszLines = CSLLoad( pszPrjFile );

        poSRS = new OGRSpatialReference();
        if( poSRS->importFromESRI( papszLines ) != OGRERR_NONE )
        {
            delete poSRS;
            poSRS = NULL;
        }
        CSLDestroy( papszLines );
    }

/* -------------------------------------------------------------------- */
/*      Extract the basename of the file.                               */
/* -------------------------------------------------------------------- */
    char	*pszBasename;

    pszBasename = CPLStrdup(CPLGetBasename(pszNewName));

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRShapeLayer	*poLayer;

    poLayer = new OGRShapeLayer( pszBasename, hSHP, hDBF, poSRS, bUpdate );
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
                                 OGRSpatialReference *poSRS,
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
        nShapeType = -1;
    
/* -------------------------------------------------------------------- */
/*      Has the application overridden this with a special creation     */
/*      option?                                                         */
/* -------------------------------------------------------------------- */
    const char *pszOverride = CSLFetchNameValue( papszOptions, "SHPT" );

    if( pszOverride == NULL )
        /* ignore */;
    else if( EQUAL(pszOverride,"POINT") )
        nShapeType = SHPT_POINT;
    else if( EQUAL(pszOverride,"ARC") )
        nShapeType = SHPT_ARC;
    else if( EQUAL(pszOverride,"POLYGON") )
        nShapeType = SHPT_POLYGON;
    else if( EQUAL(pszOverride,"MULTIPOINT") )
        nShapeType = SHPT_MULTIPOINT;
    else if( EQUAL(pszOverride,"POINTZ") )
        nShapeType = SHPT_POINTZ;
    else if( EQUAL(pszOverride,"ARCZ") )
        nShapeType = SHPT_ARCZ;
    else if( EQUAL(pszOverride,"POLYGONZ") )
        nShapeType = SHPT_POLYGONZ;
    else if( EQUAL(pszOverride,"MULTIPOINTZ") )
        nShapeType = SHPT_MULTIPOINTZ;
    else
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Unknown SHPT value of `%s' passed to Shapefile layer\n"
                  "creation.  Creation aborted.\n",
                  pszOverride );

        return NULL;
    }

    if( nShapeType == -1 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Geometry type of `%s' not supported in shapefiles.\n"
                  "Type can be overridden with a layer creation option\n"
                  "of SHPT=POINT/ARC/POLYGON/MULTIPOINT.\n",
                  OGRGeometryTypeToName(eType) );
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
/*      Create the .prj file, if required.                              */
/* -------------------------------------------------------------------- */
    if( poSRS != NULL )
    {
        char	*pszWKT = NULL;
        const char *pszPrjFile = CPLFormFilename(pszName,pszLayerName,"prj");
        FILE	*fp;

        /* the shape layer needs it's own copy */
        poSRS = poSRS->Clone();

        if( poSRS->exportToWkt( &pszWKT ) == OGRERR_NONE 
            && (fp = VSIFOpen( pszPrjFile, "wt" )) != NULL )
        {
            VSIFWrite( pszWKT, strlen(pszWKT), 1, fp );
            VSIFClose( fp );
        }

        CPLFree( pszWKT );
    }

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRShapeLayer	*poLayer;

    poLayer = new OGRShapeLayer( pszLayerName, hSHP, hDBF, poSRS, TRUE,
                                 eType );

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
